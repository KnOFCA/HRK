#pragma once
#include "interface/platform/platform.h"
namespace hrk {
class MonotonicClock { public: TimeNs now() const { return monotonicNow(); } };
class AudioClock { IAudioBackend& backend_; public: explicit AudioClock(IAudioBackend& b) : backend_(b) {} TimeNs now() const { return backend_.position(); } };
enum class ClockState { Stopped, Playing, Paused };
class SongClock {
  AudioClock audio_; ClockState state_ = ClockState::Stopped; mutable TimeNs time_ = 0;
public:
  explicit SongClock(IAudioBackend& backend) : audio_(backend) {}
  TimeNs now() const;
  void start() { time_ = audio_.now(); state_ = ClockState::Playing; }
  void pause() { time_ = now(); state_ = ClockState::Paused; }
  void resume() { state_ = ClockState::Playing; }
  void seek(TimeNs time) { time_ = time; }
  void stop() { time_ = 0; state_ = ClockState::Stopped; }
  ClockState state() const { return state_; }
};
class ClockMapper {
  TimeNs host_ = 0, song_ = 0, epoch_ = 0;
public:
  void anchor(TimeNs host, TimeNs song) { require(host >= 0 && validTime(song), Error::InvalidArgument); host_ = epoch_ = host; song_ = song; }
  void synchronize(TimeNs host, TimeNs song) { require(host >= 0 && validTime(song), Error::InvalidArgument); host_ = host; song_ = song; }
  Error map(TimeNs host, TimeNs& song) const;
};
}
