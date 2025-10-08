/*
  HypnoS, a UCI chess playing engine derived from Stockfish
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

  HypnoS is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  HypnoS is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "engine.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "evaluate.h"
#include "misc.h"
#include "nnue/network.h"
#include "nnue/nnue_common.h"
#include "numa.h"
#include "perft.h"
#include "polybook.h"
#include "position.h"
#include "search.h"
#include "syzygy/tbprobe.h"
#include "types.h"
#include "uci.h"
#include "ucioption.h"

// --- HypnoS Experience integration ---
#ifdef HYP_FIXED_ZOBRIST
#include "experience.h"
#include "experience_compat.h"  // per Experience::g_options e utility
#endif

namespace Hypnos {

#ifdef HYP_FIXED_ZOBRIST
static void on_exp_enabled(const Option& opt) {
    sync_cout << "info string Experience Enabled is now: "
              << (opt ? "enabled" : "disabled") << sync_endl;
    ::Experience::init();
    if (bool(opt))
        ::Experience::resume_learning();
}

static void on_exp_file(const Option&) {
    ::Experience::init();
}
#else
static void on_exp_enabled(const Option&) {}
static void on_exp_file(const Option&) {}
#endif

namespace NN = Eval::NNUE;

constexpr auto StartFEN   = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr int  MaxHashMB  = Is64Bit ? 33554432 : 2048;
int            MaxThreads = std::max(1024, 4 * int(get_hardware_concurrency()));

Engine::Engine(std::optional<std::string> path) :
    binaryDirectory(path ? CommandLine::get_binary_directory(*path) : ""),
    numaContext(NumaConfig::from_system()),
    states(new std::deque<StateInfo>(1)),
    threads(),
    networks(
      numaContext,
      NN::Networks(
        NN::NetworkBig({EvalFileDefaultNameBig, "None", ""}, NN::EmbeddedNNUEType::BIG),
        NN::NetworkSmall({EvalFileDefaultNameSmall, "None", ""}, NN::EmbeddedNNUEType::SMALL))) {
    pos.set(StartFEN, false, &states->back());

#ifdef HYP_FIXED_ZOBRIST
    // Bridge to allow experience.cpp to use Options["..."]
    ::Experience::g_options = &options;
#endif

    options.add(  //
      "Debug Log File", Option("", [](const Option& o) {
          start_logger(o);
          return std::nullopt;
      }));

    options.add(  //
      "NumaPolicy", Option("auto", [this](const Option& o) {
          set_numa_config_from_option(o);
          return numa_config_information_as_string() + "\n"
               + thread_allocation_information_as_string();
      }));

    options.add(  //
      "Threads", Option(1, 1, MaxThreads, [this](const Option&) {
          resize_threads();
          return thread_allocation_information_as_string();
      }));

    options.add(  //
      "Hash", Option(16, 1, MaxHashMB, [this](const Option& o) {
          set_tt_size(o);
          return std::nullopt;
      }));

    options.add(  //
      "Clear Hash", Option([this](const Option&) {
          search_clear();
          return std::nullopt;
      }));

    options.add(  //
      "Clean Search", Option(false));

    options.add(  //
      "Ponder", Option(false));

    options.add(  //
      "MultiPV", Option(1, 1, MAX_MOVES));

    options.add("Contempt", Option(20, 0, 100));

    options.add("Move Overhead", Option(10, 0, 5000));

    options.add("nodestime", Option(0, 0, 10000));

    options.add("UCI_Chess960", Option(false));

    options.add("UCI_ShowWDL", Option(false));

    options.add(  //
      "SyzygyPath", Option("", [](const Option& o) {
          Tablebases::init(o);
          return std::nullopt;
      }));

    options.add("Syzygy50MoveRule", Option(true));

    options.add("SyzygyProbeLimit", Option(7, 0, 7));

    options.add("Book1", Option(false));

    options.add("Book1 File", Option("", [](const Option& o) {
        polybook[0].init(o);
        return std::nullopt;
      }));

    options.add("Book1 BestBookMove", Option(false));

    options.add("Book1 Depth", Option(255, 1, 350));

    options.add("Book1 Width", Option(1, 1, 10));

    options.add("Book2", Option(false));

    options.add("Book2 File", Option("", [](const Option& o) {
        polybook[1].init(o);
        return std::nullopt;
      }));

    options.add("Book2 BestBookMove", Option(false));

    options.add("Book2 Depth", Option(255, 1, 350));

    options.add("Book2 Width", Option(1, 1, 10));
	
    //#ifdef HYP_FIXED_ZOBRIST
    // ===== HypnoS Experience UCI options =====
    options.add("Experience Enabled",
                Option(true, [](const Option& opt) {
                    on_exp_enabled(opt);
                    sync_cout << "info string Experience Enabled is now: "
                              << (opt ? "enabled" : "disabled") << sync_endl;
                    return std::nullopt;
                }));

    options.add("Experience File",
                Option("Hypnos.exp", [](const Option& opt) {
                    on_exp_file(opt);
                    return std::nullopt;
                }));

    options.add("Experience Readonly",
                Option(false, [](const Option& opt) {
                    sync_cout << "info string Experience Readonly is now: "
                              << (opt ? "enabled" : "disabled") << sync_endl;
                    return std::nullopt;
                }));

    options.add("Experience Book",
                Option(false, [](const Option& opt) {
                    sync_cout << "info string Experience Book is now: "
                              << (opt ? "enabled" : "disabled") << sync_endl;
                    return std::nullopt;
                }));

    options.add("Experience Book Width",
                Option(1, 1, 20, [](const Option& opt) {
                    sync_cout << "info string Experience Book Width = " << int(opt) << sync_endl;
                    return std::nullopt;
                }));

    options.add("Experience Book Eval Importance",
                Option(5, 0, 10, [](const Option& opt) {
                    sync_cout << "info string Experience Book Eval Importance = " << int(opt) << sync_endl;
                    return std::nullopt;
                }));

    options.add("Experience Book Min Depth",
                Option(27, Experience::MinDepth, 64, [](const Option& opt) {
                    sync_cout << "info string Experience Book Min Depth = " << int(opt) << sync_endl;
                    return std::nullopt;
                }));

    options.add("Experience Book Max Moves",
                Option(16, 1, 100, [](const Option& opt) {
                    sync_cout << "info string Experience Book Max Moves = " << int(opt) << sync_endl;
                    return std::nullopt;
                }));

    //#endif

    options.add("Variety",
                Option(0, 0, 40, [](const Option& opt) {
                    sync_cout << "info string Variety = " << int(opt) << sync_endl;
                    return std::nullopt;
                }));

    options.add("Variety Max Score",
                Option(50, 0, 300, [](const Option& opt) {
                    sync_cout << "info string Variety Max Score = " << int(opt) << sync_endl;
                    return std::nullopt;
                }));

    options.add("Variety Max Moves",
                Option(12, 0, 60, [](const Option& opt) {
                    sync_cout << "info string Variety Max Moves = " << int(opt) << sync_endl;
                    return std::nullopt;
                }));

// ===== HypnoS HardSuite options (minimal) =====

// HardSuiteMode — master switch: when ON, we enable the internal solver recipe.
options.add("HardSuiteMode",
    Option(true, [](const Option& opt) {
        sync_cout << "info string HardSuiteMode = " << (opt ? "enabled" : "disabled") << sync_endl;
        return std::nullopt;
    }));

// HardSuiteVerbose — print HY runtime info lines (default OFF)
options.add("HardSuiteVerbose",
    Option(false, [](const Option& opt){
        sync_cout << "info string HardSuiteVerbose = "
                  << (opt ? "enabled" : "disabled") << sync_endl;
        return std::nullopt;
    }));

// HardSuiteTactical — boolean (default = ON)
options.add("HardSuiteTactical",
    Option(true, [](const Option& opt) {
        sync_cout << "info string HardSuiteTactical = "
                  << (opt ? "enabled" : "disabled") << sync_endl;
        return std::nullopt;
    }));

// AutoSyncMultiPV — when ON, mirror SolveMultiPV into MultiPV (for GUIs that hide MultiPV)
options.add("AutoSyncMultiPV",
    Option(true, [](const Option& opt) {
        sync_cout << "info string AutoSyncMultiPV = "
                  << (opt ? "enabled" : "disabled") << sync_endl;
        return std::nullopt;
    }));

// SolveMultiPV — spin (1..16): Phase-A width (no internal write to MultiPV)
options.add("SolveMultiPV",
    Option(4, 1, 16, [&](const Option& opt) {
        const int v = int(opt);
        sync_cout << "info string SolveMultiPV = " << v << sync_endl;

        if (bool(options["AutoSyncMultiPV"])) {
            // purely informational: we cap internally to this width
            sync_cout << "info string (HY) AutoSync active: using SolveMultiPV as MultiPV cap = "
                      << v << sync_endl;
        }
        return std::nullopt;
    }));

// PVVerifyDepth — spin (0..4) : re-search PV head with soft pruning disabled
options.add("PVVerifyDepth",
    Option(3, 0, 12, [](const Option& opt) {
        sync_cout << "info string PVVerifyDepth = " << int(opt) << sync_endl;
        return std::nullopt;
    }));

// VerifyCutoffsDepth — spin (0..10) : verify low-depth TT/ProbCut cutoffs
options.add("VerifyCutoffsDepth",
    Option(8, 0, 20, [](const Option& opt) {
        sync_cout << "info string VerifyCutoffsDepth = " << int(opt) << sync_endl;
        return std::nullopt;
    }));

// QuietSEEPruneGate — spin (0..100 cp) : keep quiet moves unless SEE is worse than -gate
options.add("QuietSEEPruneGate",
    Option(45, 0, 100, [](const Option& opt) {
        sync_cout << "info string QuietSEEPruneGate = " << int(opt) << " cp" << sync_endl;
        return std::nullopt;
    }));

// ===== end of HypnoS HardSuite options =====
    options.add(  //
      "EvalFile", Option(EvalFileDefaultNameBig, [this](const Option& o) {
          load_big_network(o);
          return std::nullopt;
      }));

    options.add(  //
      "EvalFileSmall", Option(EvalFileDefaultNameSmall, [this](const Option& o) {
          load_small_network(o);
          return std::nullopt;
      }));

    load_networks();
    resize_threads();
}

std::uint64_t Engine::perft(const std::string& fen, Depth depth, bool isChess960) {
    verify_networks();

    return Benchmark::perft(fen, depth, isChess960);
}

void Engine::go(Search::LimitsType& limits) {
    assert(limits.perft == 0);
    verify_networks();

    threads.start_thinking(options, pos, states, limits);
}
void Engine::stop() { threads.stop = true; }

void Engine::search_clear() {
    wait_for_search_finished();

    tt.clear(threads);
    threads.clear();

    // @TODO wont work with multiple instances
    Tablebases::init(options["SyzygyPath"]);  // Free mapped files
}

void Engine::set_on_update_no_moves(std::function<void(const Engine::InfoShort&)>&& f) {
    updateContext.onUpdateNoMoves = std::move(f);
}

void Engine::set_on_update_full(std::function<void(const Engine::InfoFull&)>&& f) {
    updateContext.onUpdateFull = std::move(f);
}

void Engine::set_on_iter(std::function<void(const Engine::InfoIter&)>&& f) {
    updateContext.onIter = std::move(f);
}

void Engine::set_on_bestmove(std::function<void(std::string_view, std::string_view)>&& f) {
    updateContext.onBestmove = std::move(f);
}

void Engine::set_on_verify_networks(std::function<void(std::string_view)>&& f) {
    onVerifyNetworks = std::move(f);
}

void Engine::wait_for_search_finished() { threads.main_thread()->wait_for_search_finished(); }

void Engine::set_position(const std::string& fen, const std::vector<std::string>& moves) {
    // Drop the old state and create a new one
    states = StateListPtr(new std::deque<StateInfo>(1));
    pos.set(fen, options["UCI_Chess960"], &states->back());

    for (const auto& move : moves)
    {
        auto m = UCIEngine::to_move(pos, move);

        if (m == Move::none())
            break;

        states->emplace_back();
        pos.do_move(m, states->back());
    }
}

// modifiers

void Engine::set_numa_config_from_option(const std::string& o) {
    if (o == "auto" || o == "system")
    {
        numaContext.set_numa_config(NumaConfig::from_system());
    }
    else if (o == "hardware")
    {
        // Don't respect affinity set in the system.
        numaContext.set_numa_config(NumaConfig::from_system(false));
    }
    else if (o == "none")
    {
        numaContext.set_numa_config(NumaConfig{});
    }
    else
    {
        numaContext.set_numa_config(NumaConfig::from_string(o));
    }

    // Force reallocation of threads in case affinities need to change.
    resize_threads();
    threads.ensure_network_replicated();
}

void Engine::resize_threads() {
    threads.wait_for_search_finished();
    threads.set(numaContext.get_numa_config(), {options, threads, tt, networks}, updateContext);

    // Reallocate the hash with the new threadpool size
    set_tt_size(options["Hash"]);
    threads.ensure_network_replicated();
}

void Engine::set_tt_size(size_t mb) {
    wait_for_search_finished();
    tt.resize(mb, threads);
}

void Engine::set_ponderhit(bool b) { threads.main_manager()->ponder = b; }

// network related

void Engine::verify_networks() const {
    networks->big.verify(options["EvalFile"], onVerifyNetworks);
    networks->small.verify(options["EvalFileSmall"], onVerifyNetworks);
}

void Engine::load_networks() {
    networks.modify_and_replicate([this](NN::Networks& networks_) {
        networks_.big.load(binaryDirectory, options["EvalFile"]);
        networks_.small.load(binaryDirectory, options["EvalFileSmall"]);
    });
    threads.clear();
    threads.ensure_network_replicated();
}

void Engine::load_big_network(const std::string& file) {
    networks.modify_and_replicate(
      [this, &file](NN::Networks& networks_) { networks_.big.load(binaryDirectory, file); });
    threads.clear();
    threads.ensure_network_replicated();
}

void Engine::load_small_network(const std::string& file) {
    networks.modify_and_replicate(
      [this, &file](NN::Networks& networks_) { networks_.small.load(binaryDirectory, file); });
    threads.clear();
    threads.ensure_network_replicated();
}

void Engine::save_network(const std::pair<std::optional<std::string>, std::string> files[2]) {
    networks.modify_and_replicate([&files](NN::Networks& networks_) {
        networks_.big.save(files[0].first);
        networks_.small.save(files[1].first);
    });
}

// utility functions

void Engine::trace_eval() const {
    StateListPtr trace_states(new std::deque<StateInfo>(1));
    Position     p;
    p.set(pos.fen(), options["UCI_Chess960"], &trace_states->back());

    verify_networks();

    sync_cout << "\n" << Eval::trace(p, *networks) << sync_endl;
}

const OptionsMap& Engine::get_options() const { return options; }
OptionsMap&       Engine::get_options() { return options; }

std::string Engine::fen() const { return pos.fen(); }

void Engine::flip() { pos.flip(); }

std::string Engine::visualize() const {
    std::stringstream ss;
    ss << pos;
    return ss.str();
}

int Engine::get_hashfull(int maxAge) const { return tt.hashfull(maxAge); }

std::vector<std::pair<size_t, size_t>> Engine::get_bound_thread_count_by_numa_node() const {
    auto                                   counts = threads.get_bound_thread_count_by_numa_node();
    const NumaConfig&                      cfg    = numaContext.get_numa_config();
    std::vector<std::pair<size_t, size_t>> ratios;
    NumaIndex                              n = 0;
    for (; n < counts.size(); ++n)
        ratios.emplace_back(counts[n], cfg.num_cpus_in_numa_node(n));
    if (!counts.empty())
        for (; n < cfg.num_numa_nodes(); ++n)
            ratios.emplace_back(0, cfg.num_cpus_in_numa_node(n));
    return ratios;
}

std::string Engine::get_numa_config_as_string() const {
    return numaContext.get_numa_config().to_string();
}

std::string Engine::numa_config_information_as_string() const {
    auto cfgStr = get_numa_config_as_string();
    return "Available processors: " + cfgStr;
}

std::string Engine::thread_binding_information_as_string() const {
    auto              boundThreadsByNode = get_bound_thread_count_by_numa_node();
    std::stringstream ss;
    if (boundThreadsByNode.empty())
        return ss.str();

    bool isFirst = true;

    for (auto&& [current, total] : boundThreadsByNode)
    {
        if (!isFirst)
            ss << ":";
        ss << current << "/" << total;
        isFirst = false;
    }

    return ss.str();
}

std::string Engine::thread_allocation_information_as_string() const {
    std::stringstream ss;

    size_t threadsSize = threads.size();
    ss << "Using " << threadsSize << (threadsSize > 1 ? " threads" : " thread");

    auto boundThreadsByNodeStr = thread_binding_information_as_string();
    if (boundThreadsByNodeStr.empty())
        return ss.str();

    ss << " with NUMA node thread binding: ";
    ss << boundThreadsByNodeStr;

    return ss.str();
}
}
