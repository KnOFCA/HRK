#pragma once
#include "kernel/gameplay/session.h"
#include <memory>
namespace hrk {
class Composition {
  InputTrace trace_;
  IAudioBackend& audio_;
  std::unique_ptr<IGameModule> game_; std::unique_ptr<GameplaySession> session_;
public:
  explicit Composition(IAudioBackend&);
  Error load(const std::string& chart,const std::vector<uint8_t>& audio,RuntimeMode mode,const std::vector<uint8_t>& replay);
  GameplaySession& session(){return *session_;}
  InputTrace& inputTrace(){return trace_;}
  CaptureError beginCapture(TraceHeader header);
  CaptureError endCapture(){return trace_.endCapture(session_->state()==SessionState::Playing);}
  CaptureError exportCapture(const std::string& path) const{return trace_.exportCapture(path);}
};
}
