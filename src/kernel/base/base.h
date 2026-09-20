#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace hrk {
using TimeNs = int64_t;
using DurationNs = int64_t;
using Beat = double;
using EntityId = uint64_t;
using ResourceId = uint32_t;
using GameId = uint32_t;
using JudgmentId = uint32_t;
constexpr TimeNs ms = 1000000;
constexpr TimeNs second = 1000000000;
constexpr TimeNs maxTime = 86400 * second;
constexpr size_t maxEvents = 1000000;
double seconds(TimeNs value);
double milliseconds(TimeNs value);
TimeNs monotonicNow();
uint64_t hashBytes(const std::string& bytes);
enum class Error { Ok, InvalidArgument, InvalidState, ChartParseError, ResourceNotFound,
  AudioInvalid, BackendFailure, UnsupportedVersion, ReplayInvalid, ReplayChartMismatch,
  ReplayIncompatible, QueueFull, StaleInput, LateInput, ReplayFull };
const char* errorName(Error error);
struct Failure : std::runtime_error {
  Error error;
  explicit Failure(Error e) : std::runtime_error(errorName(e)), error(e) {}
};
inline void require(bool condition, Error error) { if (!condition) throw Failure(error); }
inline bool validTime(TimeNs t) { return t >= 0 && t <= maxTime; }
struct Vec2 { float x = 0; float y = 0; };
inline bool validPosition(Vec2 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && p.x >= 0 && p.x <= 1 && p.y >= 0 && p.y <= 1;
}
enum class InputPhase : uint32_t { Down, Move, Up, Cancel };
struct InputEvent { uint32_t pointerId = 0; InputPhase phase = InputPhase::Down; Vec2 position; TimeNs songTime = 0; };
struct RawInputEvent { uint32_t pointerId = 0; InputPhase phase = InputPhase::Down; Vec2 position; TimeNs hostTime = 0; };
struct JudgmentResult { EntityId entity; JudgmentId type; TimeNs targetTime; TimeNs inputTime; TimeNs error; };
inline bool operator==(const JudgmentResult& a, const JudgmentResult& b) {
  return a.entity == b.entity && a.type == b.type && a.targetTime == b.targetTime && a.inputTime == b.inputTime && a.error == b.error;
}
inline bool operator==(const InputEvent& a, const InputEvent& b) {
  return a.pointerId == b.pointerId && a.phase == b.phase && a.position.x == b.position.x && a.position.y == b.position.y && a.songTime == b.songTime;
}
struct Score { uint64_t value = 0; uint32_t perfect = 0; uint32_t miss = 0; };
struct Pcm { uint32_t sampleRate = 48000; uint32_t channels = 2; std::vector<int16_t> samples;
  uint64_t frames() const { return channels ? samples.size() / channels : 0; }
  TimeNs duration() const { return static_cast<TimeNs>(frames()) * second / sampleRate; }
};
class EntityIds { EntityId next_ = 1; public: EntityId next() { return next_++; } };
// Exactly one producer and consumer. Clear only after the producer is quiescent.
template<class T, size_t Capacity> class FixedQueue {
  std::array<T, Capacity + 1> data_{};
  std::atomic<size_t> read_{0}, write_{0};
public:
  bool push(const T& item) {
    const auto w = write_.load(std::memory_order_relaxed), next = (w + 1) % data_.size();
    if (next == read_.load(std::memory_order_acquire)) return false;
    data_[w] = item; write_.store(next, std::memory_order_release); return true;
  }
  bool pop(T& item) {
    const auto r = read_.load(std::memory_order_relaxed);
    if (r == write_.load(std::memory_order_acquire)) return false;
    item = data_[r]; read_.store((r + 1) % data_.size(), std::memory_order_release); return true;
  }
  void clear() { read_.store(write_.load(std::memory_order_acquire), std::memory_order_release); }
};
}
