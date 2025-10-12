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

#include "uci.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "benchmark.h"
#include "engine.h"
#include "experience.h"
#include "memory.h"
#include "movegen.h"
#include "position.h"
#include "score.h"
#include "search.h"
#include "types.h"
#include "ucioption.h"

#if defined(HYP_FIXED_ZOBRIST)
#include "experience_compat.h"  // map 'Options' -> Experience::g_options
#endif

#include <mutex>
#if defined(HYP_FIXED_ZOBRIST)
#include <filesystem>
#endif

namespace Hypnos {

// forward: definita in misc.cpp
void start_logger(const std::string& fname);

#if defined(HYP_FIXED_ZOBRIST)

// Forward declaration: the definition is in position.cpp (where you include hypnos_zobrist.h)
namespace HypnosZobrist { void SetHypnosZobrist(); }

// One-shot init of Zobrist/Experience with path normalization and without duplicate logs
namespace {
    std::once_flag exp_once;

    // Normalize the path: if it's relative -> use the working dir. Returns true if the value changed.
    static bool normalize_experience_path(Engine& engine) {
        auto& opts = engine.get_options();

        std::string current = std::string(opts["Experience File"]);
        if (current.empty())
            current = "Hypnos.exp";

        std::filesystem::path p(current);
        if (p.is_relative())
            p = std::filesystem::current_path() / p;

        const std::string resolved = p.string();
        if (resolved == std::string(opts["Experience File"]))
            return false;  // no changes -> no setoption() -> no extra logs

        // Update the option using setoption (triggers: reload + single print)
        std::ostringstream cmd;
        cmd << "name Experience File value " << resolved;
        auto ss = std::istringstream(cmd.str());
        engine.wait_for_search_finished();
        opts.setoption(ss);
        return true;
    }

    void ensure_exp_initialized(Engine& engine) {
        std::call_once(exp_once, [&]{
            // Expose Options to the legacy Experience layer
            ::Experience::g_options = &engine.get_options();

            // Normalize path; if changed, setoption() will already handle reload and print
            const bool changed = normalize_experience_path(engine);

            // Set Zobrist
            HypnosZobrist::SetHypnosZobrist();

            // Initialize only if the option change hasn’t already done it
            if (!changed)
                Experience::init();

            // Always wait for loading to finish
            Experience::wait_for_loading_finished();
        });
    }
}
#endif

constexpr auto BenchmarkCommand = "speedtest";

constexpr auto StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
template<typename... Ts>
struct overload: Ts... {
    using Ts::operator()...;
};

template<typename... Ts>
overload(Ts...) -> overload<Ts...>;

void UCIEngine::print_info_string(std::string_view str) {
    sync_cout_start();
    for (auto& line : split(str, "\n"))
    {
        if (!is_whitespace(line))
        {
            std::cout << "info string " << line << '\n';
        }
    }
    sync_cout_end();
}

UCIEngine::UCIEngine(int argc, char** argv) :
    engine(argv[0]),
    cli(argc, argv) {

    engine.get_options().add_info_listener([](const std::optional<std::string>& str) {
        if (str.has_value())
            print_info_string(*str);
    });

    init_search_update_listeners();

#if defined(HYP_FIXED_ZOBRIST)
    ensure_exp_initialized(engine);
    Experience::wait_for_loading_finished();
#endif
}

void UCIEngine::init_search_update_listeners() {
    engine.set_on_iter([](const auto& i) { on_iter(i); });
    engine.set_on_update_no_moves([](const auto& i) { on_update_no_moves(i); });
    engine.set_on_update_full(
      [this](const auto& i) { on_update_full(i, engine.get_options()["UCI_ShowWDL"]); });
    engine.set_on_bestmove([](const auto& bm, const auto& p) { on_bestmove(bm, p); });
    engine.set_on_verify_networks([](const auto& s) { print_info_string(s); });
}

void UCIEngine::loop() {
    std::string token, cmd;

    // Execute any command-line arguments once
    for (int i = 1; i < cli.argc; ++i)
        cmd += std::string(cli.argv[i]) + " ";

    do {
        // If there are no arguments, read from stdin; EOF => quit
        if (cli.argc == 1 && !std::getline(std::cin, cmd))
            cmd = "quit";

        std::istringstream is(cmd);

        token.clear();                 // Avoid "stale" token on empty line
        is >> std::skipws >> token;

        if (token == "quit" || token == "stop") {
            engine.stop();
        }
        else if (token == "ponderhit") {
            // The GUI played the expected move: disable ponder
            engine.set_ponderhit(false);
        }
        else if (token == "uci") {
            // Avvia il log UCI automaticamente alla prima chiamata
            static bool log_started = false;
            if (!log_started) {
                start_logger("hypnos_uci.log");
                log_started = true;
            }

            sync_cout << "id name " << engine_info(true) << "\n"
                      << engine.get_options() << sync_endl;
            sync_cout << "uciok" << sync_endl;
        }
        else if (token == "setoption") {
            setoption(is);
#if defined(HYP_FIXED_ZOBRIST)
            ensure_exp_initialized(engine);
            // Make sure Experience has finished any pending loading
            Experience::wait_for_loading_finished();
#endif
        }
        else if (token == "go") {
            const std::string firstFEN = engine.fen();
#if defined(HYP_FIXED_ZOBRIST)
            ensure_exp_initialized(engine);
            if (firstFEN == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
                Experience::resume_learning();
#endif
            print_info_string(engine.numa_config_information_as_string());
            print_info_string(engine.thread_allocation_information_as_string());
            go(is);
        }
        else if (token == "position") {
            position(is);
        }
        else if (token == "ucinewgame") {
#if defined(HYP_FIXED_ZOBRIST)
            ensure_exp_initialized(engine);
            Experience::save();
#endif
            engine.search_clear();
#if defined(HYP_FIXED_ZOBRIST)
            Experience::resume_learning();
#endif
        }
        else if (token == "isready") {
#if defined(HYP_FIXED_ZOBRIST)
            ensure_exp_initialized(engine);
            Experience::wait_for_loading_finished();
#endif
            sync_cout << "readyok" << sync_endl;
        }
        else if (token == "bench") {
            bench(is);
        }
        else if (token == BenchmarkCommand) {
            benchmark(is);
        }
        else if (token == "d") {
            sync_cout << engine.visualize() << sync_endl;
        }
        else if (token == "eval") {
            engine.trace_eval();
        }
        else if (token == "compiler") {
            sync_cout << compiler_info() << sync_endl;
        }
#if defined(HYP_FIXED_ZOBRIST)
        else if (token == "exp") {
            // Show Experience for the current position (synthetic view)
            ensure_exp_initialized(engine);
            Experience::wait_for_loading_finished();

            StateInfo st{};
            Position  pos;
            pos.set(engine.fen(), false, &st); // 'false' if not using Chess960
            Experience::show_exp(pos, false);
        }
        else if (token == "expex") {
            // Show Experience for the current position (extended view)
            ensure_exp_initialized(engine);
            Experience::wait_for_loading_finished();

            StateInfo st{};
            Position  pos;
            pos.set(engine.fen(), false, &st); // 'false' if you are not using Chess960
            Experience::show_exp(pos, true);
        }
        else if (token == "defrag")
        {
            ensure_exp_initialized(engine);
            Experience::wait_for_loading_finished();

            std::vector<std::string> args;
            for (std::string a; is >> std::skipws >> a; )
                args.emplace_back(std::move(a));

            // If no filename is provided, use the "Experience File" Option
            if (args.empty()) {
                args.emplace_back(std::string(engine.get_options()["Experience File"]));
            }

            std::vector<char*> cargs;
            cargs.reserve(args.size());
            for (auto& s : args)
                cargs.push_back(const_cast<char*>(s.c_str()));

            Experience::defrag((int)cargs.size(), cargs.data());
        }
        else if (token == "merge")
        {
            ensure_exp_initialized(engine);
            Experience::wait_for_loading_finished();

            std::vector<std::string> args;
            for (std::string a; is >> std::skipws >> a; )
                args.emplace_back(std::move(a));

            if (args.empty()) {
                sync_cout << "info string Syntax: merge <target.exp> <file1.exp> [file2.exp] ..." << sync_endl;
            } else {
                // Single argument: use Experience File as the target, arg[0] is the source
                if (args.size() == 1) {
                    std::string target = std::string(engine.get_options()["Experience File"]);
                    args.insert(args.begin(), std::move(target));
                }

                std::vector<char*> cargs;
                cargs.reserve(args.size());
                for (auto& s : args)
                    cargs.push_back(const_cast<char*>(s.c_str()));

                Experience::merge((int)cargs.size(), cargs.data());
            }
        }
        else if (token == "import_cpgn")
        {
            ensure_exp_initialized(engine);
            Experience::wait_for_loading_finished();

            // Syntax: import_cpgn <source.cpgn>
            std::vector<std::string> args;
            for (std::string a; is >> std::skipws >> a; )
                args.emplace_back(std::move(a));

            if (args.empty()) {
                sync_cout << "info string Syntax: import_cpgn <source.cpgn>" << sync_endl;
            } else {
                // Destination is the current Options["Experience File"]
                std::vector<char*> cargs;
                cargs.reserve(args.size());
                for (auto& s : args)
                    cargs.push_back(const_cast<char*>(s.c_str()));
                Experience::import_cpgn((int)cargs.size(), cargs.data());
            }
        }
        else if (token == "import_pgn")
        {
            ensure_exp_initialized(engine);
            Experience::wait_for_loading_finished();

            // Syntax: import_pgn <source.pgn>
            std::vector<std::string> args;
            for (std::string a; is >> std::skipws >> a; )
                args.emplace_back(std::move(a));

            if (args.empty()) {
                sync_cout << "info string Syntax: import_pgn <source.pgn>" << sync_endl;
            } else {
                // Destination is the current Options["Experience File"]
                std::vector<char*> cargs;
                cargs.reserve(args.size());
                for (auto& s : args)
                    cargs.push_back(const_cast<char*>(s.c_str()));
                Experience::import_pgn((int)cargs.size(), cargs.data());
            }
        }
        else if (token == "cpgn_to_exp")
        {
            ensure_exp_initialized(engine);
            Experience::wait_for_loading_finished();

            // Syntax: cpgn_to_exp <source.cpgn> <dest.exp>
            std::vector<std::string> args;
            for (std::string a; is >> std::skipws >> a; )
                args.emplace_back(std::move(a));

            if (args.size() < 2) {
                sync_cout << "info string Syntax: cpgn_to_exp <source.cpgn> <dest.exp>" << sync_endl;
            } else {
                std::vector<char*> cargs;
                cargs.reserve(args.size());
                for (auto& s : args)
                    cargs.push_back(const_cast<char*>(s.c_str()));
                Experience::cpgn_to_exp((int)cargs.size(), cargs.data());
            }
        }
        else if (token == "pgn_to_exp")
        {
            ensure_exp_initialized(engine);
            Experience::wait_for_loading_finished();

            // Syntax: pgn_to_exp <source.pgn> <dest.exp>
            std::vector<std::string> args;
            for (std::string a; is >> std::skipws >> a; )
                args.emplace_back(std::move(a));

            if (args.size() < 2) {
                sync_cout << "info string Syntax: pgn_to_exp <source.pgn> <dest.exp>" << sync_endl;
            } else {
                std::vector<char*> cargs;
                cargs.reserve(args.size());
                for (auto& s : args)
                    cargs.push_back(const_cast<char*>(s.c_str()));
                Experience::pgn_to_exp((int)cargs.size(), cargs.data());
            }
        }
#endif

        else if (token == "legal") {
            // Print every LEGAL move in the current engine position.
            // Format: "legal <m1> <m2> ..."
            sync_cout_start();
            std::cout << "legal";
            for (Move m : MoveList<LEGAL>(pos))
                std::cout << ' ' << UCIEngine::move(m, pos.is_chess960());
            std::cout << std::endl;
            sync_cout_end();
        }

        else if (token == "moves") {
            // Alias of 'legal' for viewers expecting 'moves' token.
            // Format: "moves <m1> <m2> ..."
            sync_cout_start();
            std::cout << "moves";
            for (Move m : MoveList<LEGAL>(pos))
                std::cout << ' ' << UCIEngine::move(m, pos.is_chess960());
            std::cout << std::endl;
            sync_cout_end();
        }

        else if (!token.empty() && token[0] != '#') {
            sync_cout << "Unknown command: '" << cmd
                      << "'. Type help for more information." << sync_endl;
        }

        if (cli.argc > 1)
            token = "quit";

    } while (token != "quit");

#if defined(HYP_FIXED_ZOBRIST)
    // Writes to disk what has been collected in RAM
    Experience::save();
    sync_cout << "info string [EXP] saved on quit" << sync_endl;
#endif
}

Search::LimitsType UCIEngine::parse_limits(std::istream& is) {
    Search::LimitsType limits;
    std::string        token;

    limits.startTime = now();  // The search starts as early as possible

    while (is >> token)
        if (token == "searchmoves")  // Needs to be the last command on the line
            while (is >> token)
                limits.searchmoves.push_back(to_lower(token));

        else if (token == "wtime")
            is >> limits.time[WHITE];
        else if (token == "btime")
            is >> limits.time[BLACK];
        else if (token == "winc")
            is >> limits.inc[WHITE];
        else if (token == "binc")
            is >> limits.inc[BLACK];
        else if (token == "movestogo")
            is >> limits.movestogo;
        else if (token == "depth")
            is >> limits.depth;
        else if (token == "nodes")
            is >> limits.nodes;
        else if (token == "movetime")
            is >> limits.movetime;
        else if (token == "mate")
            is >> limits.mate;
        else if (token == "perft")
            is >> limits.perft;
        else if (token == "infinite")
            limits.infinite = 1;
        else if (token == "ponder")
            limits.ponderMode = true;

    return limits;
}

void UCIEngine::go(std::istringstream& is) {

    Search::LimitsType limits = parse_limits(is);

    if (limits.perft)
        perft(limits);
    else
        engine.go(limits);
}

void UCIEngine::bench(std::istream& args) {
    std::string token;
    uint64_t    num, nodes = 0, cnt = 1;
    uint64_t    nodesSearched = 0;
    const auto& options       = engine.get_options();

    engine.set_on_update_full([&](const auto& i) {
        nodesSearched = i.nodes;
        on_update_full(i, options["UCI_ShowWDL"]);
    });

    std::vector<std::string> list = Benchmark::setup_bench(engine.fen(), args);

    num = count_if(list.begin(), list.end(),
                   [](const std::string& s) { return s.find("go ") == 0 || s.find("eval") == 0; });

    TimePoint elapsed = now();

    for (const auto& cmd : list)
    {
        std::istringstream is(cmd);
        is >> std::skipws >> token;

        if (token == "go" || token == "eval")
        {
            std::cerr << "\nPosition: " << cnt++ << '/' << num << " (" << engine.fen() << ")"
                      << std::endl;
            if (token == "go")
            {
                Search::LimitsType limits = parse_limits(is);

                if (limits.perft)
                    nodesSearched = perft(limits);
                else
                {
                    engine.go(limits);
                    engine.wait_for_search_finished();
                }

                nodes += nodesSearched;
                nodesSearched = 0;
            }
            else
                engine.trace_eval();
        }
        else if (token == "setoption")
            setoption(is);
        else if (token == "position")
            position(is);
        else if (token == "ucinewgame")
        {
            engine.search_clear();  // search_clear may take a while
            elapsed = now();
        }
    }

    elapsed = now() - elapsed + 1;  // Ensure positivity to avoid a 'divide by zero'

    dbg_print();

    std::cerr << "\n==========================="    //
              << "\nTotal time (ms) : " << elapsed  //
              << "\nNodes searched  : " << nodes    //
              << "\nNodes/second    : " << 1000 * nodes / elapsed << std::endl;

    // reset callback, to not capture a dangling reference to nodesSearched
    engine.set_on_update_full([&](const auto& i) { on_update_full(i, options["UCI_ShowWDL"]); });
}

void UCIEngine::benchmark(std::istream& args) {
    // Probably not very important for a test this long, but include for completeness and sanity.
    static constexpr int NUM_WARMUP_POSITIONS = 3;

    std::string token;
    uint64_t    nodes = 0, cnt = 1;
    uint64_t    nodesSearched = 0;

    engine.set_on_update_full([&](const Engine::InfoFull& i) { nodesSearched = i.nodes; });

    engine.set_on_iter([](const auto&) {});
    engine.set_on_update_no_moves([](const auto&) {});
    engine.set_on_bestmove([](const auto&, const auto&) {});
    engine.set_on_verify_networks([](const auto&) {});

    Benchmark::BenchmarkSetup setup = Benchmark::setup_benchmark(args);

    const int numGoCommands = count_if(setup.commands.begin(), setup.commands.end(),
                                       [](const std::string& s) { return s.find("go ") == 0; });

    TimePoint totalTime = 0;

    // Set options once at the start.
    auto ss = std::istringstream("name Threads value " + std::to_string(setup.threads));
    setoption(ss);
    ss = std::istringstream("name Hash value " + std::to_string(setup.ttSize));
    setoption(ss);
    ss = std::istringstream("name UCI_Chess960 value false");
    setoption(ss);

    // Warmup
    for (const auto& cmd : setup.commands)
    {
        std::istringstream is(cmd);
        is >> std::skipws >> token;

        if (token == "go")
        {
            // One new line is produced by the search, so omit it here
            std::cerr << "\rWarmup position " << cnt++ << '/' << NUM_WARMUP_POSITIONS;

            Search::LimitsType limits = parse_limits(is);

            TimePoint elapsed = now();

            // Run with silenced network verification
            engine.go(limits);
            engine.wait_for_search_finished();

            totalTime += now() - elapsed;

            nodes += nodesSearched;
            nodesSearched = 0;
        }
        else if (token == "position")
            position(is);
        else if (token == "ucinewgame")
        {
            engine.search_clear();  // search_clear may take a while
        }

        if (cnt > NUM_WARMUP_POSITIONS)
            break;
    }

    std::cerr << "\n";

    cnt   = 1;
    nodes = 0;

    int           numHashfullReadings = 0;
    constexpr int hashfullAges[]      = {0, 999};  // Only normal hashfull and touched hash.
    int           totalHashfull[std::size(hashfullAges)] = {0};
    int           maxHashfull[std::size(hashfullAges)]   = {0};

    auto updateHashfullReadings = [&]() {
        numHashfullReadings += 1;

        for (int i = 0; i < static_cast<int>(std::size(hashfullAges)); ++i)
        {
            const int hashfull = engine.get_hashfull(hashfullAges[i]);
            maxHashfull[i]     = std::max(maxHashfull[i], hashfull);
            totalHashfull[i] += hashfull;
        }
    };

    engine.search_clear();  // search_clear may take a while

    for (const auto& cmd : setup.commands)
    {
        std::istringstream is(cmd);
        is >> std::skipws >> token;

        if (token == "go")
        {
            // One new line is produced by the search, so omit it here
            std::cerr << "\rPosition " << cnt++ << '/' << numGoCommands;

            Search::LimitsType limits = parse_limits(is);

            TimePoint elapsed = now();

            // Run with silenced network verification
            engine.go(limits);
            engine.wait_for_search_finished();

            totalTime += now() - elapsed;

            updateHashfullReadings();

            nodes += nodesSearched;
            nodesSearched = 0;
        }
        else if (token == "position")
            position(is);
        else if (token == "ucinewgame")
        {
            engine.search_clear();  // search_clear may take a while
        }
    }

    totalTime = std::max<TimePoint>(totalTime, 1);  // Ensure positivity to avoid a 'divide by zero'

    dbg_print();

    std::cerr << "\n";

    static_assert(
      std::size(hashfullAges) == 2 && hashfullAges[0] == 0 && hashfullAges[1] == 999,
      "Hardcoded for display. Would complicate the code needlessly in the current state.");

    std::string threadBinding = engine.thread_binding_information_as_string();
    if (threadBinding.empty())
        threadBinding = "none";

    // clang-format off

    std::cerr << "==========================="
              << "\nVersion                    : "
              << engine_version_info()
              // "\nCompiled by                : "
              << compiler_info()
              << "Large pages                : " << (has_large_pages() ? "yes" : "no")
              << "\nUser invocation            : " << BenchmarkCommand << " "
              << setup.originalInvocation << "\nFilled invocation          : " << BenchmarkCommand
              << " " << setup.filledInvocation
              << "\nAvailable processors       : " << engine.get_numa_config_as_string()
              << "\nThread count               : " << setup.threads
              << "\nThread binding             : " << threadBinding
              << "\nTT size [MiB]              : " << setup.ttSize
              << "\nHash max, avg [per mille]  : "
              << "\n    single search          : " << maxHashfull[0] << ", "
              << totalHashfull[0] / numHashfullReadings
              << "\n    single game            : " << maxHashfull[1] << ", "
              << totalHashfull[1] / numHashfullReadings
              << "\nTotal nodes searched       : " << nodes
              << "\nTotal search time [s]      : " << totalTime / 1000.0
              << "\nNodes/second               : " << 1000 * nodes / totalTime << std::endl;

    // clang-format on

    init_search_update_listeners();

#if defined(HYP_FIXED_ZOBRIST)
    // Ensures normalized path and EXP loading (idempotent)
    ensure_exp_initialized(engine);
    Experience::wait_for_loading_finished();
#endif
}

void UCIEngine::setoption(std::istringstream& is) {
    engine.wait_for_search_finished();
    engine.get_options().setoption(is);
}

std::uint64_t UCIEngine::perft(const Search::LimitsType& limits) {
    auto nodes = engine.perft(engine.fen(), limits.perft, engine.get_options()["UCI_Chess960"]);
    sync_cout << "\nNodes searched: " << nodes << "\n" << sync_endl;
    return nodes;
}

void UCIEngine::position(std::istringstream& is) {
    std::string token, fen;

    is >> token;

    if (token == "startpos")
    {
        fen = StartFEN;
        is >> token;  // Consume the "moves" token, if any
    }
    else if (token == "fen")
        while (is >> token && token != "moves")
            fen += token + " ";
    else
        return;

    std::vector<std::string> moves;

    while (is >> token)
    {
        moves.push_back(token);
    }

    engine.set_position(fen, moves);
}

namespace {

struct WinRateParams {
    double a;
    double b;
};

WinRateParams win_rate_params(const Position& pos) {

    int material = pos.count<PAWN>() + 3 * pos.count<KNIGHT>() + 3 * pos.count<BISHOP>()
                 + 5 * pos.count<ROOK>() + 9 * pos.count<QUEEN>();

    // The fitted model only uses data for material counts in [17, 78], and is anchored at count 58.
    double m = std::clamp(material, 17, 78) / 58.0;

    // Return a = p_a(material) and b = p_b(material), see github.com/official-Hypnos/WDL_model
    constexpr double as[] = {-13.50030198, 40.92780883, -36.82753545, 386.83004070};
    constexpr double bs[] = {96.53354896, -165.79058388, 90.89679019, 49.29561889};

    double a = (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
    double b = (((bs[0] * m + bs[1]) * m + bs[2]) * m) + bs[3];

    return {a, b};
}

// The win rate model is 1 / (1 + exp((a - eval) / b)), where a = p_a(material) and b = p_b(material).
// It fits the LTC fishtest statistics rather accurately.
int win_rate_model(Value v, const Position& pos) {

    auto [a, b] = win_rate_params(pos);

    // Return the win rate in per mille units, rounded to the nearest integer.
    return int(0.5 + 1000 / (1 + std::exp((a - double(v)) / b)));
}
}

std::string UCIEngine::format_score(const Score& s) {
    constexpr int TB_CP = 20000;
    const auto    format =
      overload{[](Score::Mate mate) -> std::string {
                   auto m = (mate.plies > 0 ? (mate.plies + 1) : mate.plies) / 2;
                   return std::string("mate ") + std::to_string(m);
               },
               [](Score::Tablebase tb) -> std::string {
                   return std::string("cp ")
                        + std::to_string((tb.win ? TB_CP - tb.plies : -TB_CP - tb.plies));
               },
               [](Score::InternalUnits units) -> std::string {
                   return std::string("cp ") + std::to_string(units.value);
               }};

    return s.visit(format);
}

// Turns a Value to an integer centipawn number,
// without treatment of mate and similar special scores.
int UCIEngine::to_cp(Value v, const Position& pos) {

    // In general, the score can be defined via the WDL as
    // (log(1/L - 1) - log(1/W - 1)) / (log(1/L - 1) + log(1/W - 1)).
    // Based on our win_rate_model, this simply yields v / a.

    auto [a, b] = win_rate_params(pos);

    return std::round(100 * int(v) / a);
}

std::string UCIEngine::wdl(Value v, const Position& pos) {
    std::stringstream ss;

    int wdl_w = win_rate_model(v, pos);
    int wdl_l = win_rate_model(-v, pos);
    int wdl_d = 1000 - wdl_w - wdl_l;
    ss << wdl_w << " " << wdl_d << " " << wdl_l;

    return ss.str();
}

std::string UCIEngine::square(Square s) {
    return std::string{char('a' + file_of(s)), char('1' + rank_of(s))};
}

std::string UCIEngine::move(Move m, bool chess960) {
    if (m == Move::none())
        return "(none)";

    if (m == Move::null())
        return "0000";

    Square from = m.from_sq();
    Square to   = m.to_sq();

    if (m.type_of() == CASTLING && !chess960)
        to = make_square(to > from ? FILE_G : FILE_C, rank_of(from));

    std::string move = square(from) + square(to);

    if (m.type_of() == PROMOTION)
        move += " pnbrqk"[m.promotion_type()];

    return move;
}


std::string UCIEngine::to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](auto c) { return std::tolower(c); });

    return str;
}

Move UCIEngine::to_move(const Position& pos, std::string str) {
    str = to_lower(str);

    for (const auto& m : MoveList<LEGAL>(pos))
        if (str == move(m, pos.is_chess960()))
            return m;

    return Move::none();
}

void UCIEngine::on_update_no_moves(const Engine::InfoShort& info) {
    sync_cout << "info depth " << info.depth << " score " << format_score(info.score) << sync_endl;
}

void UCIEngine::on_update_full(const Engine::InfoFull& info, bool showWDL) {
    std::stringstream ss;

    ss << "info";
    ss << " depth " << info.depth                 //
       << " seldepth " << info.selDepth           //
       << " multipv " << info.multiPV             //
       << " score " << format_score(info.score);  //

    if (!info.bound.empty())
        ss << " " << info.bound;

    if (showWDL)
        ss << " wdl " << info.wdl;

    ss << " nodes " << info.nodes        //
       << " nps " << info.nps            //
       << " hashfull " << info.hashfull  //
       << " tbhits " << info.tbHits      //
       << " time " << info.timeMs        //
       << " pv " << info.pv;             //

    sync_cout << ss.str() << sync_endl;
}

void UCIEngine::on_iter(const Engine::InfoIter& info) {
    std::stringstream ss;

    ss << "info";
    ss << " depth " << info.depth                     //
       << " currmove " << info.currmove               //
       << " currmovenumber " << info.currmovenumber;  //

    sync_cout << ss.str() << sync_endl;
}

void UCIEngine::on_bestmove(std::string_view bestmove, std::string_view ponder) {
    sync_cout << "bestmove " << bestmove;
    if (!ponder.empty())
        std::cout << " ponder " << ponder;
    std::cout << sync_endl;

#if defined(HYP_FIXED_ZOBRIST)
    // Persisti l'Experience dopo ogni ricerca (compat con Viewer a ondate)
    Experience::save();
#endif
}

}  // namespace Hypnos
