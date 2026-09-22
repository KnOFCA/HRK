#pragma once
#include "interface/platform/platform.h"
namespace hrk {
// Host-only exact public-query playback. Never advances or extrapolates a
// clock.
struct RecordedQuery {
  bool position = false, success = false;
  TimeNs host = 0, song = 0, value = 0;
};
class RecordedAudioBackend : public IAudioBackend {
  std::vector<RecordedQuery> queries_;
  mutable size_t cursor_ = 0;
  mutable bool mismatch_ = false;
  TimeNs duration_ = 0, grace_ = 0;
  Error control_ = Error::Ok;
  const RecordedQuery *next(bool position) const;

public:
  explicit RecordedAudioBackend(TimeNs grace) : grace_(grace) {}
  void action(std::vector<RecordedQuery> queries, Error control = Error::Ok);
  bool matched() const { return !mismatch_ && cursor_ == queries_.size(); }
  Error load(const Pcm &pcm) override {
    duration_ = pcm.duration();
    return control_;
  }
  Error start() override { return control_; }
  Error pause() override { return control_; }
  Error resume() override { return control_; }
  Error seek(TimeNs) override { return control_; }
  void stop() override {}
  TimeNs position() const override;
  bool clockSample(TimeNs &host, TimeNs &song) const override;
  TimeNs duration() const override { return duration_; }
  TimeNs inputDeliveryGrace() const override { return grace_; }
};
} // namespace hrk
