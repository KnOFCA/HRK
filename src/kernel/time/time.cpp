#include "kernel/time/time.h"
#include <algorithm>
namespace hrk {
TimeNs SongClock::now() const { if (state_ == ClockState::Playing) time_ = std::max(time_, audio_.now()); return time_; }
Error ClockMapper::map(TimeNs host, TimeNs& song) const {
  if (host < epoch_) return Error::StaleInput;
  if (host - host_ > maxTime - song_) return Error::InvalidArgument;
  song = song_ + (host - host_); return validTime(song) ? Error::Ok : Error::StaleInput;
}
}
