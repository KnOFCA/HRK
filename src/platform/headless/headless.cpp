#include "platform/headless/headless.h"
#include <algorithm>
namespace hrk { void FakeAudioBackend::advance(TimeNs delta) { require(validTime(delta),Error::InvalidArgument); if (running_) time_ = std::min(duration_, time_ + delta); } }
