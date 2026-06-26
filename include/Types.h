#pragma once

// ============================================================================
// Types.h — Common type definitions for the LOB simulation
// ============================================================================
//
// Integer tick pricing:
//   All prices are stored internally as int64_t "ticks" to eliminate
//   floating-point comparison issues in map keys and matching logic.
//
//   PRICE_SCALE = 100 → prices have 2 decimal places of precision.
//   Example: $100.25 is stored as tick value 10025.
//
//   toTicks(double)  — convert a double price to integer ticks
//   fromTicks(Price)  — convert integer ticks back to a human-readable double
//
// Why not double?
//   Using double as std::map keys can produce phantom price levels:
//   (0.1 + 0.2) != 0.3 in IEEE 754, so two orders at "the same price"
//   might land in different map buckets.  Integer keys are always exact.
// ============================================================================

#include <cstdint>
#include <cmath>

using Price = int64_t;

// Scale factor: 100 = 2 decimal places (tick size = 0.01)
static constexpr int PRICE_SCALE = 100;

// Convert a floating-point price to integer ticks
inline Price toTicks(double p) {
    return static_cast<Price>(std::llround(p * PRICE_SCALE));
}

// Convert integer ticks back to a human-readable double
inline double fromTicks(Price t) {
    return static_cast<double>(t) / PRICE_SCALE;
}
