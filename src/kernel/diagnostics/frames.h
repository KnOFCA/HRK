#pragma once
#include "kernel/base/base.h"
#include <algorithm>
namespace hrk {
// Capture storage is fixed; sorting/export only runs on the control path.
class FrameCapture {
  std::array<TimeNs,16384> intervals_{};std::array<uint32_t,60> buckets_{};
  size_t count_=0;TimeNs first_=0,last_=0;bool running_=false,done_=false;
public:
  void start(){count_=0;first_=last_=0;running_=true;done_=false;buckets_.fill(0);}
  void observe(TimeNs now){
    if(!running_)return;
    if(!first_){first_=last_=now;return;}
    if(now<=last_)return;
    const auto elapsed=now-first_;
    if(count_<intervals_.size())intervals_[count_++]=now-last_;
    if(elapsed<60*second)++buckets_[static_cast<size_t>(elapsed/second)];
    last_=now;if(elapsed>=60*second){running_=false;done_=true;}
  }
  double elapsed() const{return seconds(last_-first_);}
  bool complete() const{return done_;}
  size_t count() const{return count_;}
  double p95ms() const{if(!count_)return 0;auto copy=intervals_;std::sort(copy.begin(),copy.begin()+static_cast<ptrdiff_t>(count_));return milliseconds(copy[(count_*95+99)/100-1]);}
  const std::array<uint32_t,60>& buckets() const{return buckets_;}
  double maxGapMs() const{return count_?milliseconds(*std::max_element(intervals_.begin(),intervals_.begin()+static_cast<ptrdiff_t>(count_))):0;}
  uint32_t lowestFps() const{return *std::min_element(buckets_.begin(),buckets_.end());}
};
}
