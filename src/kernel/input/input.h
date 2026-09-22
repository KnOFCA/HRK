#pragma once
#include "kernel/time/time.h"
#include "kernel/diagnostics/input_trace.h"
namespace hrk {
class InputQueue {
  FixedQueue<InputEnvelope, 2048> queue_;
  std::atomic<uint64_t> overflow_{0};
public:
  Error push(const RawInputEvent& e);
  Error pushTraced(const InputEnvelope& e);
  bool pop(RawInputEvent& e) { InputEnvelope value;if(!queue_.pop(value))return false;e=value.raw;return true; }
  bool popTraced(InputEnvelope& e) {return queue_.pop(e);}
  void clear() { queue_.clear(); }
  template<class Observe> void clearObserved(Observe observe){queue_.clearObserved(observe);}
  uint64_t overflow() const { return overflow_.load(); }
};
class PointerState {
  std::array<uint32_t, 32> ids_{}; std::array<bool, 32> used_{};
public:
  Error apply(const InputEvent& e);
  bool active(uint32_t id) const;
  void clear() { used_.fill(false); }
  uint32_t snapshot(std::array<uint32_t,32>& ids) const;
};
}
