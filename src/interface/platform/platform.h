#pragma once
#include "interface/render/render.h"
#include <functional>
namespace hrk {
class IAudioBackend {
public:
  virtual ~IAudioBackend() = default;
  virtual Error load(const Pcm& pcm) = 0;
  virtual Error start() = 0;
  virtual Error pause() = 0;
  virtual Error resume() = 0;
  virtual Error seek(TimeNs time) = 0;
  virtual void stop() = 0;
  virtual TimeNs position() const = 0;
  virtual TimeNs duration() const = 0;
  // A matching device clock pair; false for manually driven backends.
  virtual bool clockSample(TimeNs&, TimeNs&) const { return false; }
  virtual TimeNs inputDeliveryGrace() const { return 0; }
};
class IInputBackend { public: virtual ~IInputBackend() = default; virtual bool poll(RawInputEvent&) = 0; virtual void clear() = 0; };
class IRenderBackend : public IRenderContext { public: virtual bool begin() = 0; virtual void end() = 0; };
class IRenderSurface { public: virtual ~IRenderSurface() = default; virtual uint32_t width() const = 0; virtual uint32_t height() const = 0; };
class IFrameScheduler { public: virtual ~IFrameScheduler() = default; virtual void setCallback(std::function<void(TimeNs)> callback) = 0; virtual void stop() = 0; };
class IFileSystem { public: virtual ~IFileSystem() = default; virtual std::vector<uint8_t> read(const std::string& path) = 0; virtual void write(const std::string& path, const std::vector<uint8_t>& data) = 0; };
class ILifecycleBackend { public: virtual ~ILifecycleBackend() = default; virtual void setBackgroundCallback(std::function<void()> callback) = 0; };
}
