#pragma once
#include "interface/platform/platform.h"
#include <ohaudio/native_audiostreambuilder.h>
#include <ohaudio/native_audiorenderer.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
namespace hrk::harmony {
class OHAudioBackend : public IAudioBackend {
  Pcm pcm_; OH_AudioRenderer* renderer_=nullptr;
  std::atomic<uint64_t> cursor_{0}; std::atomic<bool> interrupted_{false}, failed_{false};
  TimeNs offset_=0; mutable TimeNs lastPosition_=0; bool running_=false;
  Error create(); void release();
  static int32_t write(OH_AudioRenderer*,void*,void*,int32_t);
  static int32_t interrupt(OH_AudioRenderer*,void*,OH_AudioInterrupt_ForceType,OH_AudioInterrupt_Hint);
  static int32_t error(OH_AudioRenderer*,void*,OH_AudioStream_Result);
public:
  ~OHAudioBackend() override { release(); }
  Error load(const Pcm&) override; Error start() override; Error pause() override; Error resume() override;
  Error seek(TimeNs) override; void stop() override; TimeNs position() const override; TimeNs duration() const override {return pcm_.duration();}
  bool clockSample(TimeNs&,TimeNs&) const override;
  TimeNs inputDeliveryGrace() const override { return 40*ms; }
  bool needsPause() {return interrupted_.exchange(false) || failed_.load();}
};
class HarmonyInputBackend : public IInputBackend {
  FixedQueue<RawInputEvent,2048> queue_; std::atomic<uint64_t> dropped_{0};
public:
  void touch(OH_NativeXComponent*,void*);
  bool poll(RawInputEvent& e) override{return queue_.pop(e);} void clear() override{queue_.clear();}
  uint64_t dropped() const{return dropped_.load();}
};
class GlesRenderBackend : public IRenderBackend, public IRenderSurface {
  EGLDisplay display_=EGL_NO_DISPLAY; EGLContext context_=EGL_NO_CONTEXT; EGLSurface surface_=EGL_NO_SURFACE;
  GLuint program_=0,buffer_=0,vao_=0,texture_=0;GLint color_=-1;
  uint32_t width_=0,height_=0;
public:
  ~GlesRenderBackend() override{destroy();}
  bool create(void*,uint32_t,uint32_t);void destroy();void resize(uint32_t,uint32_t);
  bool begin() override;void end() override;void clear(Color) override;void sprite(const SpriteDrawCommand&) override;
  uint32_t width() const override{return width_;}uint32_t height() const override{return height_;}
};
class HarmonyFrameScheduler : public IFrameScheduler {
  std::function<void(TimeNs)> callback_;
public:
  void setCallback(std::function<void(TimeNs)> f) override{callback_=std::move(f);}void stop() override{callback_={};}
  void frame(TimeNs t){if(callback_)callback_(t);}
};
class HarmonyLifecycle : public ILifecycleBackend {
  std::function<void()> callback_;
public:
  void setBackgroundCallback(std::function<void()> f) override{callback_=std::move(f);}void background(){if(callback_)callback_();}
};
}
