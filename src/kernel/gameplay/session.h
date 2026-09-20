#pragma once
#include "interface/game/game.h"
#include "kernel/input/input.h"
#include "kernel/replay/replay.h"
namespace hrk {
enum class RuntimeMode { Play, Replay, AutoPlay };
enum class SessionState { Created, Loading, Ready, Playing, Paused, Finished };
struct Diagnostics { uint64_t frames=0, inputs=0, judgments=0, rejectedInputs=0; double fps=0; TimeNs maxInputDelay=0; };
class GameplaySession : public IJudgmentSink {
  IGameModule& game_; IAudioBackend& audio_;
  std::shared_ptr<const IRuntimeChart> chart_;
  std::unique_ptr<IGameRules> rules_; std::unique_ptr<IScoreSystem> score_; std::unique_ptr<IGameRenderer> renderer_;
  SongClock clock_; ClockMapper mapper_; InputQueue queue_; PointerState pointers_;
  RuntimeMode mode_=RuntimeMode::Play; SessionState state_=SessionState::Created;
  Replay replay_; size_t replayCursor_=0; std::vector<JudgmentResult> judgments_;
  Diagnostics diagnostics_; TimeNs watermark_=0, previousFrame_=0;
  InputEvent pending_{}; bool hasPending_=false;
  void resetResults(); void feed(const InputEvent& event);
public:
  GameplaySession(IGameModule& game,IAudioBackend& audio);
  ~GameplaySession() override;
  Error load(std::shared_ptr<const IRuntimeChart> chart,const Pcm& pcm,RuntimeMode mode,const Replay* replay=nullptr);
  Error start(TimeNs host); Error pause(); Error resume(TimeNs host); Error seek(TimeNs time,TimeNs host); void stop();
  Error push(const RawInputEvent& event);
  Error update(); void render(IRenderBackend& output,TimeNs host);
  void emit(const JudgmentResult& judgment) override;
  SessionState state() const { return state_; } TimeNs time() const { return clock_.now(); }
  Score score() const { return score_->score(); }
  const Diagnostics& diagnostics() const { return diagnostics_; }
  const std::vector<JudgmentResult>& judgments() const { return judgments_; }
  const Replay& replay() const { return replay_; }
  uint64_t overflowCount() const { return queue_.overflow(); }
};
}
