#pragma once
#include "kernel/gameplay/session.h"
#include <memory>
namespace hrk {
class Composition {
  std::unique_ptr<IGameModule> game_; std::unique_ptr<GameplaySession> session_;
public:
  explicit Composition(IAudioBackend&);
  Error load(const std::string& chart,const std::vector<uint8_t>& audio,RuntimeMode mode,const std::vector<uint8_t>& replay);
  GameplaySession& session(){return *session_;}
};
}
