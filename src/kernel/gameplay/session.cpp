#include "kernel/gameplay/session.h"
#include <algorithm>
namespace hrk {
GameplaySession::GameplaySession(IGameModule& g,IAudioBackend& a):game_(g),audio_(a),rules_(g.createRules()),score_(g.createScoreSystem()),renderer_(g.createRenderer()),clock_(a) {}
GameplaySession::~GameplaySession(){audio_.stop();}
void GameplaySession::resetResults(){rules_->reset(*chart_);score_->reset();judgments_.clear();pointers_.clear();replayCursor_=0;watermark_=0;diagnostics_.inputs=diagnostics_.judgments=0;}
Error GameplaySession::load(std::shared_ptr<const IRuntimeChart> c,const Pcm& pcm,RuntimeMode mode,const Replay* replay){
  if(state_==SessionState::Playing || state_==SessionState::Paused) return Error::InvalidState;
  stop(); state_=SessionState::Loading;
  try {
    require(c!=nullptr && pcm.frames()>0 && pcm.duration()>=c->endTime(),Error::InvalidArgument);
    Replay candidate; const auto descriptor=game_.descriptor(); candidate.header.gameId=descriptor.id; candidate.header.ruleVersion=descriptor.ruleVersion; candidate.header.chartHash=c->hash();
    if(mode==RuntimeMode::Replay){require(replay!=nullptr,Error::ReplayInvalid);validateReplay(*replay,c->hash(),descriptor.id,descriptor.ruleVersion);candidate=*replay;}
    else if(mode==RuntimeMode::AutoPlay) candidate.events=game_.createInput()->autoPlay(*c);
    require(candidate.events.empty() || candidate.events.back().songTime<=pcm.duration(),Error::ReplayInvalid);
    auto result=audio_.load(pcm);require(result==Error::Ok,result);
    chart_=std::move(c);mode_=mode;replay_=std::move(candidate);replay_.events.reserve(maxEvents);judgments_.reserve(chart_->size());
    diagnostics_={};previousFrame_=0;resetResults();state_=SessionState::Ready;return Error::Ok;
  }catch(const Failure& e){audio_.stop();state_=SessionState::Created;return e.error;}
}
Error GameplaySession::start(TimeNs host){
  if(host<0)return Error::InvalidArgument;
  if(state_!=SessionState::Ready) return Error::InvalidState;
  auto e=audio_.start();if(e!=Error::Ok)return e;clock_.start();mapper_.anchor(host,clock_.now());state_=SessionState::Playing;return Error::Ok;
}
Error GameplaySession::pause(){
  if(state_!=SessionState::Playing)return Error::InvalidState;
  auto e=audio_.pause();if(e!=Error::Ok)return e;clock_.pause();queue_.clear();hasPending_=false;pointers_.clear();state_=SessionState::Paused;return Error::Ok;
}
Error GameplaySession::resume(TimeNs host){
  if(host<0)return Error::InvalidArgument;
  if(state_!=SessionState::Paused)return Error::InvalidState;
  auto e=audio_.resume();if(e!=Error::Ok)return e;mapper_.anchor(host,clock_.now());clock_.resume();state_=SessionState::Playing;return Error::Ok;
}
void GameplaySession::stop(){audio_.stop();clock_.stop();queue_.clear();hasPending_=false;pointers_.clear();state_=SessionState::Finished;}
Error GameplaySession::push(const RawInputEvent& event){
  if(state_!=SessionState::Playing || mode_!=RuntimeMode::Play)return Error::InvalidState;
  return queue_.push(event);
}
void GameplaySession::emit(const JudgmentResult& j){judgments_.push_back(j);score_->add(j);++diagnostics_.judgments;}
void GameplaySession::feed(const InputEvent& e){pointers_.apply(e);rules_->input(e,*this);++diagnostics_.inputs;}
Error GameplaySession::seek(TimeNs t,TimeNs host){
  if(state_!=SessionState::Ready && state_!=SessionState::Playing && state_!=SessionState::Paused)return Error::InvalidState;
  if(host<0 || !validTime(t) || t>audio_.duration())return Error::InvalidArgument;
  auto result=audio_.seek(t);if(result!=Error::Ok)return result;
  queue_.clear();hasPending_=false;
  if(mode_==RuntimeMode::Play) replay_.events.erase(std::upper_bound(replay_.events.begin(),replay_.events.end(),t,[](TimeNs time,const InputEvent& e){return time<e.songTime;}),replay_.events.end());
  resetResults();
  while(replayCursor_<replay_.events.size() && replay_.events[replayCursor_].songTime<=t)feed(replay_.events[replayCursor_++]);
  rules_->advance(t,*this);watermark_=t;clock_.seek(t);mapper_.anchor(host,t);pointers_.clear();return Error::Ok;
}
Error GameplaySession::update(){
  if(state_!=SessionState::Playing)return Error::InvalidState;
  const TimeNs now=clock_.now(); Error result=Error::Ok;
  TimeNs sampleHost=0,sampleSong=0;
  const bool sampled=audio_.clockSample(sampleHost,sampleSong);
  if(sampled)mapper_.synchronize(sampleHost,sampleSong);
  const TimeNs grace=mode_==RuntimeMode::Play?audio_.inputDeliveryGrace():0;
  const bool finished=now>=audio_.duration() && (grace==0 || (sampled && sampleSong>=audio_.duration()+grace));
  if(mode_==RuntimeMode::Play){
    for(;;){
      InputEvent event;
      if(hasPending_){event=pending_;hasPending_=false;}
      else{
        RawInputEvent raw;if(!queue_.pop(raw))break;
        event={raw.pointerId,raw.phase,raw.position,0};auto error=mapper_.map(raw.hostTime,event.songTime);
        if(error!=Error::Ok){++diagnostics_.rejectedInputs;result=error;continue;}
      }
      diagnostics_.maxInputDelay=std::max(diagnostics_.maxInputDelay,now-event.songTime);
      if(event.songTime<watermark_){++diagnostics_.rejectedInputs;result=Error::LateInput;continue;}
      if(event.songTime>now){pending_=event;hasPending_=true;break;}
      if(replay_.events.size()==maxEvents){pause();return Error::ReplayFull;}
      replay_.events.push_back(event);feed(event);watermark_=event.songTime;
    }
  }else while(replayCursor_<replay_.events.size() && replay_.events[replayCursor_].songTime<=now)feed(replay_.events[replayCursor_++]);
  const TimeNs horizon=finished?now:std::max(TimeNs{0},now-grace);
  rules_->advance(horizon,*this);watermark_=std::max(watermark_,horizon);
  if(finished){audio_.pause();clock_.pause();state_=SessionState::Finished;}
  return result;
}
void GameplaySession::render(IRenderBackend& output,TimeNs host){
  if(!chart_ || !output.begin())return;
  renderer_->render(*chart_,clock_.now(),output);output.end();++diagnostics_.frames;
  if(previousFrame_ && host>previousFrame_)diagnostics_.fps=static_cast<double>(second)/static_cast<double>(host-previousFrame_);
  previousFrame_=host;
}
}
