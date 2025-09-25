#pragma once
#include <atomic>

namespace Hypnos { namespace Eval {

struct DynParams {
    std::atomic<int> open_mat{0};     // dyn_open_mat
    std::atomic<int> open_pos{0};     // dyn_open_pos
    std::atomic<int> end_mat{0};      // dyn_end_mat
    std::atomic<int> end_pos{0};      // dyn_end_pos
    std::atomic<int> complexity{10};  // dyn_complexity_gain (0..50)
};

extern DynParams g_dyn;

void set_dyn_open_mat(int v);
void set_dyn_open_pos(int v);
void set_dyn_end_mat(int v);
void set_dyn_end_pos(int v);
void set_dyn_complexity(int v);

// optional: snapshot
inline DynParams dyn_snapshot() { return g_dyn; }

}} // namespace Hypnos::Eval
