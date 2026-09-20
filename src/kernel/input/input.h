#pragma once
#include "kernel/time/time.h"
namespace hrk {
class InputQueue {
  FixedQueue<RawInputEvent, 2048> queue_;
  std::atomic<uint64_t> overflow_{0};
public:
  Error push(const RawInputEvent& e);
  bool pop(RawInputEvent& e) { return queue_.pop(e); }
  void clear() { queue_.clear(); }
  uint64_t overflow() const { return overflow_.load(); }
};
class PointerState {
  std::array<uint32_t, 32> ids_{}; std::array<bool, 32> used_{};
public:
  Error apply(const InputEvent& e);
  bool active(uint32_t id) const;
  void clear() { used_.fill(false); }
};
}
