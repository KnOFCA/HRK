#include "platform/headless/recorded_audio.h"
namespace hrk {
void RecordedAudioBackend::action(std::vector<RecordedQuery> queries,
                                  Error control) {
  queries_ = std::move(queries);
  cursor_ = 0;
  mismatch_ = false;
  control_ = control;
}
const RecordedQuery *RecordedAudioBackend::next(bool position) const {
  if (cursor_ >= queries_.size() || queries_[cursor_].position != position) {
    mismatch_ = true;
    return nullptr;
  }
  return &queries_[cursor_++];
}
TimeNs RecordedAudioBackend::position() const {
  const auto *q = next(true);
  return q ? q->value : 0;
}
bool RecordedAudioBackend::clockSample(TimeNs &host, TimeNs &song) const {
  const auto *q = next(false);
  if (!q || !q->success)
    return false;
  host = q->host;
  song = q->song;
  return true;
}
} // namespace hrk
