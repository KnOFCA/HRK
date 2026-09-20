#pragma once
#include "interface/platform/platform.h"
namespace hrk {
class FakeAudioBackend : public IAudioBackend {
  TimeNs time_ = 0, duration_ = 0; bool running_ = false;
public:
  bool fail = false;
  Error load(const Pcm& p) override { if (fail || !p.frames()) return Error::BackendFailure; duration_ = p.duration(); time_ = 0; running_ = false; return Error::Ok; }
  Error start() override { if (fail || !duration_) return Error::BackendFailure; running_ = true; return Error::Ok; }
  Error pause() override { running_ = false; return Error::Ok; }
  Error resume() override { return start(); }
  Error seek(TimeNs t) override { if (!validTime(t) || t > duration_) return Error::InvalidArgument; time_ = t; return Error::Ok; }
  void stop() override { running_ = false; time_ = 0; }
  TimeNs position() const override { return time_; }
  TimeNs duration() const override { return duration_; }
  void advance(TimeNs delta);
};
class FakeInputBackend : public IInputBackend {
  FixedQueue<RawInputEvent,2048> queue_;
public: bool push(const RawInputEvent& e) { return queue_.push(e); } bool poll(RawInputEvent& e) override { return queue_.pop(e); } void clear() override { queue_.clear(); }
};
class NullRenderBackend : public IRenderBackend {
public: uint64_t frames = 0, sprites = 0; Color background{};
  bool begin() override { ++frames; return true; } void end() override {}
  void clear(Color c) override { background = c; } void sprite(const SpriteDrawCommand&) override { ++sprites; }
};
class FakeFrameScheduler : public IFrameScheduler {
  std::function<void(TimeNs)> callback_;
public: void setCallback(std::function<void(TimeNs)> f) override { callback_ = std::move(f); }
  void stop() override { callback_ = {}; } void frame(TimeNs t) { if (callback_) callback_(t); }
};
class HeadlessLifecycle : public ILifecycleBackend {
  std::function<void()> callback_;
public: void setBackgroundCallback(std::function<void()> f) override { callback_ = std::move(f); } void background() { if (callback_) callback_(); }
};
}
