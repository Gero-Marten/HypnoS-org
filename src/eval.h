// eval.h

#pragma once
#include <atomic>

namespace Hypnos { namespace Eval {

struct DynParams {
    std::atomic<int> open_mat{0};
    std::atomic<int> open_pos{0};
    std::atomic<int> end_mat{0};
    std::atomic<int> end_pos{0};
    std::atomic<int> complexity{10};
};

extern DynParams g_dyn;

struct DynSnapshot {
    int open_mat;
    int open_pos;
    int end_mat;
    int end_pos;
    int complexity;
};

inline DynSnapshot dyn_snapshot() {
    return {
        g_dyn.open_mat.load(std::memory_order_relaxed),
        g_dyn.open_pos.load(std::memory_order_relaxed),
        g_dyn.end_mat.load(std::memory_order_relaxed),
        g_dyn.end_pos.load(std::memory_order_relaxed),
        g_dyn.complexity.load(std::memory_order_relaxed)
    };
}

}} // namespace

