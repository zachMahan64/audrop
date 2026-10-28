#pragma once

namespace audrop {

// Monotonic wall clock in seconds.
double now_seconds();

// Sleeps for roughly `seconds` (sub-millisecond requests may busy-wait briefly).
void sleep_seconds(double seconds);

}  // namespace audrop
