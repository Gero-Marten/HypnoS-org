#pragma once

namespace DynGate {
// true = dynamic weights enabled for this node
extern thread_local bool enabled;
// [0..1] global per-iteration strength ramp for dynamic weights
extern thread_local float strength;
}