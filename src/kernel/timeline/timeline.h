#pragma once
#include "kernel/base/base.h"
namespace hrk {
struct TempoPoint { Beat beat; double bpm; };
class TimingMap {
  std::vector<TempoPoint> tempos_; std::vector<long double> times_;
public:
  explicit TimingMap(std::vector<TempoPoint> tempos);
  TimeNs beatToTime(Beat beat) const;
  Beat timeToBeat(TimeNs time) const;
};
}
