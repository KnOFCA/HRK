#include "kernel/input/input.h"
#include <algorithm>
namespace hrk {
Error InputQueue::push(const RawInputEvent& e) {
  return pushTraced(InputEnvelope{e,{}});
}
Error InputQueue::pushTraced(const InputEnvelope& envelope) {
  const auto& e=envelope.raw;
  if (!validPosition(e.position) || static_cast<uint32_t>(e.phase) > 3 || e.hostTime < 0) return Error::InvalidArgument;
  if (!queue_.push(envelope)) { ++overflow_; return Error::QueueFull; } return Error::Ok;
}
uint32_t PointerState::snapshot(std::array<uint32_t,32>& ids) const {uint32_t n=0;for(size_t i=0;i<ids_.size();++i)if(used_[i])ids[n++]=ids_[i];std::sort(ids.begin(),ids.begin()+n);return n;}
bool PointerState::active(uint32_t id) const { for (size_t i = 0; i < ids_.size(); ++i) if (used_[i] && ids_[i] == id) return true; return false; }
Error PointerState::apply(const InputEvent& e) {
  for (size_t i = 0; i < ids_.size(); ++i) if (used_[i] && ids_[i] == e.pointerId) {
    if (e.phase == InputPhase::Up || e.phase == InputPhase::Cancel) used_[i] = false;
    return Error::Ok;
  }
  if (e.phase == InputPhase::Down) {
    for (size_t i = 0; i < ids_.size(); ++i) if (!used_[i]) { used_[i] = true; ids_[i] = e.pointerId; return Error::Ok; }
    return Error::QueueFull;
  }
  return Error::Ok;
}
}
