#include "kernel/base/base.h"
#include <chrono>
namespace hrk {
double seconds(TimeNs v) { return static_cast<double>(v) / second; }
double milliseconds(TimeNs v) { return static_cast<double>(v) / ms; }
TimeNs monotonicNow() { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
uint64_t hashBytes(const std::string& s) { uint64_t h = 14695981039346656037ULL; for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; } return h; }
const char* errorName(Error e) {
  static constexpr const char* names[] = {"Ok", "InvalidArgument", "InvalidState", "ChartParseError", "ResourceNotFound", "AudioInvalid", "BackendFailure", "UnsupportedVersion", "ReplayInvalid", "ReplayChartMismatch", "ReplayIncompatible", "QueueFull", "StaleInput", "LateInput", "ReplayFull"};
  return names[static_cast<size_t>(e)];
}
}
