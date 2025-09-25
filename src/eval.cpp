#include "eval.h"
#include <algorithm>

namespace Hypnos { namespace Eval {

DynParams g_dyn{};

static inline int clamp16(int v) { return std::max(-16, std::min(16, v)); }
static inline int clamp50(int v) { return std::max(  0, std::min(50, v)); }

void set_dyn_open_mat(int v)   { g_dyn.open_mat.store(clamp16(v),   std::memory_order_relaxed); }
void set_dyn_open_pos(int v)   { g_dyn.open_pos.store(clamp16(v),   std::memory_order_relaxed); }
void set_dyn_end_mat(int v)    { g_dyn.end_mat.store(clamp16(v),    std::memory_order_relaxed); }
void set_dyn_end_pos(int v)    { g_dyn.end_pos.store(clamp16(v),    std::memory_order_relaxed); }
void set_dyn_complexity(int v) { g_dyn.complexity.store(clamp50(v), std::memory_order_relaxed); }

}} // namespace Hypnos::Eval
