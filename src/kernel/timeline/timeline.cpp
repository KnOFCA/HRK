#include "kernel/timeline/timeline.h"
#include <algorithm>
namespace hrk {
TimingMap::TimingMap(std::vector<TempoPoint> t) : tempos_(std::move(t)) {
  require(!tempos_.empty() && tempos_.size() <= 4096 && tempos_[0].beat == 0, Error::InvalidArgument);
  times_.push_back(0);
  for (size_t i = 0; i < tempos_.size(); ++i) {
    const auto& p = tempos_[i];
    require(std::isfinite(p.beat) && p.beat >= 0 && p.beat <= 1000000 && std::isfinite(p.bpm) && p.bpm > 0 && p.bpm <= 1000, Error::InvalidArgument);
    if (i) {
      require(p.beat > tempos_[i-1].beat, Error::InvalidArgument);
      times_.push_back(times_.back() + (p.beat - tempos_[i-1].beat) * (60.0L * second / tempos_[i-1].bpm));
      require(times_.back() <= maxTime, Error::InvalidArgument);
    }
  }
}
TimeNs TimingMap::beatToTime(Beat b) const {
  require(std::isfinite(b) && b >= 0 && b <= 1000000, Error::InvalidArgument);
  auto it = std::upper_bound(tempos_.begin(), tempos_.end(), b, [](Beat v, const TempoPoint& p) { return v < p.beat; });
  const auto i = static_cast<size_t>(it - tempos_.begin() - 1);
  const auto value = times_[i] + (b - tempos_[i].beat) * (60.0L * second / tempos_[i].bpm);
  require(value <= maxTime, Error::InvalidArgument); return static_cast<TimeNs>(std::llround(value));
}
Beat TimingMap::timeToBeat(TimeNs t) const {
  require(validTime(t), Error::InvalidArgument);
  auto it = std::upper_bound(times_.begin(), times_.end(), static_cast<long double>(t));
  const auto i = static_cast<size_t>(it - times_.begin() - 1);
  return tempos_[i].beat + static_cast<double>((t - times_[i]) * tempos_[i].bpm / (60.0L * second));
}
}
