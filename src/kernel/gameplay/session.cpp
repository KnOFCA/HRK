#include "kernel/gameplay/session.h"
#include <algorithm>
namespace hrk {
struct GameplaySession::Observation {
  GameplaySession& session; InputTrace::Scope scope; Error error=Error::Ok;
  TimeNs before,now=0;bool hasTimes=false;
  Observation(GameplaySession& s,TraceOperation op,TimeNs host,TimeNs target=0,bool update=false):session(s),scope(s.trace_,op,host,target,update),before(s.watermark_){}
  ~Observation(){if(scope.trace){std::array<uint32_t,32> ids{};auto n=session.pointers_.snapshot(ids);scope.finish(error,ids,n,before,session.watermark_,now,hasTimes);}}
  Error result(Error e){error=e;return e;}
};
void GameplaySession::finishTrace(InputTrace::Scope& scope,Error result) const{if(scope.trace){std::array<uint32_t,32> ids{};auto n=pointers_.snapshot(ids);scope.finish(result,ids,n);}}
TimeNs GameplaySession::time() const{InputTrace::Scope scope(trace_,TraceOperation::Status,trace_ && trace_->active()?monotonicNow():0);auto t=clock_.now();finishTrace(scope);return t;}
void GameplaySession::clearInputs(){
  if(trace_ && trace_->active()){
    queue_.clearObserved([&](const InputEnvelope& e){trace_->cleared(e);});
    if(hasPending_){auto i=pendingTrace_;i.before=i.after=watermark_;i.valid&=~SongValue;i.stage=TraceStage::Terminal;i.disposition=TraceDisposition::Cleared;i.reason=TraceReason::LifecycleClear;trace_->input(TraceSource::Session,i);}
  }else queue_.clear();
  hasPending_=false;
}
GameplaySession::GameplaySession(IGameModule& g,IAudioBackend& a):game_(g),audio_(a),rules_(g.createRules()),score_(g.createScoreSystem()),renderer_(g.createRenderer()),clock_(audio_) {}
GameplaySession::~GameplaySession(){audio_.stop();}
void GameplaySession::resetResults(){rules_->reset(*chart_);score_->reset();judgments_.clear();pointers_.clear();replayCursor_=0;watermark_=0;diagnostics_.inputs=diagnostics_.judgments=0;}
Error GameplaySession::load(std::shared_ptr<const IRuntimeChart> c,const Pcm& pcm,RuntimeMode mode,const Replay* replay){
  Observation observe(*this,TraceOperation::Load,trace_ && trace_->active()?monotonicNow():0,0,false);
  if(state_==SessionState::Playing || state_==SessionState::Paused) return observe.result(Error::InvalidState);
  stop(); state_=SessionState::Loading;
  try {
    require(c!=nullptr && pcm.frames()>0 && pcm.duration()>=c->endTime(),Error::InvalidArgument);
    Replay candidate; const auto descriptor=game_.descriptor(); candidate.header.gameId=descriptor.id; candidate.header.ruleVersion=descriptor.ruleVersion; candidate.header.chartHash=c->hash();
    if(mode==RuntimeMode::Replay){require(replay!=nullptr,Error::ReplayInvalid);validateReplay(*replay,c->hash(),descriptor.id,descriptor.ruleVersion);candidate=*replay;}
    else if(mode==RuntimeMode::AutoPlay) candidate.events=game_.createInput()->autoPlay(*c);
    require(candidate.events.empty() || candidate.events.back().songTime<=pcm.duration(),Error::ReplayInvalid);
    auto result=audio_.load(pcm);require(result==Error::Ok,result);
    chart_=std::move(c);mode_=mode;replay_=std::move(candidate);replay_.events.reserve(maxEvents);judgments_.reserve(chart_->size());
    diagnostics_={};previousFrame_=0;resetResults();state_=SessionState::Ready;return observe.result(Error::Ok);
  }catch(const Failure& e){audio_.stop();state_=SessionState::Created;return observe.result(e.error);}
}
Error GameplaySession::start(TimeNs host){
  Observation observe(*this,TraceOperation::Start,host,0,false);
  if(host<0)return observe.result(Error::InvalidArgument);
  if(state_!=SessionState::Ready) return observe.result(Error::InvalidState);
  auto e=audio_.start();if(e!=Error::Ok)return observe.result(e);clock_.start();const auto song=clock_.now();mapper_.anchor(host,song);epochHost_=host;if(trace_)mappingSample_=trace_->anchor(host,song);state_=SessionState::Playing;return observe.result(Error::Ok);
}
Error GameplaySession::pause(){
  Observation observe(*this,TraceOperation::Pause,trace_ && trace_->active()?monotonicNow():0,0,false);
  if(state_!=SessionState::Playing)return observe.result(Error::InvalidState);
  auto e=audio_.pause();if(e!=Error::Ok)return observe.result(e);clock_.pause();clearInputs();pointers_.clear();state_=SessionState::Paused;return observe.result(Error::Ok);
}
Error GameplaySession::resume(TimeNs host){
  Observation observe(*this,TraceOperation::Resume,host,0,false);
  if(host<0)return observe.result(Error::InvalidArgument);
  if(state_!=SessionState::Paused)return observe.result(Error::InvalidState);
  auto e=audio_.resume();if(e!=Error::Ok)return observe.result(e);const auto song=clock_.now();mapper_.anchor(host,song);epochHost_=host;if(trace_)mappingSample_=trace_->anchor(host,song);clock_.resume();state_=SessionState::Playing;return observe.result(Error::Ok);
}
void GameplaySession::stop(){Observation observe(*this,TraceOperation::Stop,trace_ && trace_->active()?monotonicNow():0);audio_.stop();clock_.stop();clearInputs();pointers_.clear();state_=SessionState::Finished;}
Error GameplaySession::push(const RawInputEvent& event){
  InputTrace::Transfer action(trace_);
  if(trace_ && trace_->active())return pushTraced(trace_->receive(event,monotonicNow(),0,0,TraceSource::Session));
  return pushTraced(InputEnvelope{event,{}});
}
Error GameplaySession::pushTraced(const InputEnvelope& event){
  InputTrace::Transfer action(trace_);TraceInput i{};i.event=event;
  Error e=(state_!=SessionState::Playing || mode_!=RuntimeMode::Play)?Error::InvalidState:queue_.pushTraced(event);
  if(trace_ && trace_->active()){
    i.stage=e==Error::Ok?TraceStage::SessionEnqueue:TraceStage::Terminal;i.disposition=e==Error::Ok?TraceDisposition::Queued:TraceDisposition::Rejected;i.error=e;
    if(e!=Error::Ok)i.reason=e==Error::InvalidState?TraceReason::InvalidState:e==Error::QueueFull?TraceReason::SessionQueueFull:TraceReason::InvalidInput;
    trace_->input(TraceSource::Session,i);
  }return e;
}
void GameplaySession::emit(const JudgmentResult& j){judgments_.push_back(j);score_->add(j);++diagnostics_.judgments;}
void GameplaySession::feed(const InputEvent& e){pointers_.apply(e);rules_->input(e,*this);++diagnostics_.inputs;}
Error GameplaySession::seek(TimeNs t,TimeNs host){
  Observation observe(*this,TraceOperation::Seek,host,t,false);
  if(state_!=SessionState::Ready && state_!=SessionState::Playing && state_!=SessionState::Paused)return observe.result(Error::InvalidState);
  if(host<0 || !validTime(t) || t>audio_.duration())return observe.result(Error::InvalidArgument);
  auto result=audio_.seek(t);if(result!=Error::Ok)return observe.result(result);
  clearInputs();
  if(mode_==RuntimeMode::Play) replay_.events.erase(std::upper_bound(replay_.events.begin(),replay_.events.end(),t,[](TimeNs time,const InputEvent& e){return time<e.songTime;}),replay_.events.end());
  resetResults();
  while(replayCursor_<replay_.events.size() && replay_.events[replayCursor_].songTime<=t)feed(replay_.events[replayCursor_++]);
  rules_->advance(t,*this);watermark_=t;clock_.seek(t);mapper_.anchor(host,t);epochHost_=host;if(trace_)mappingSample_=trace_->anchor(host,t);pointers_.clear();return observe.result(Error::Ok);
}
Error GameplaySession::update(){
  Observation observe(*this,TraceOperation::Update,trace_ && trace_->active()?monotonicNow():0,0,true);
  if(state_!=SessionState::Playing)return observe.result(Error::InvalidState);
  const TimeNs now=clock_.now();observe.now=now;observe.hasTimes=true; Error result=Error::Ok;
  TimeNs sampleHost=0,sampleSong=0;
  const bool sampled=audio_.clockSample(sampleHost,sampleSong);
  if(sampled){mapper_.synchronize(sampleHost,sampleSong);mappingSample_=audio_.lastSample();}
  const TimeNs grace=mode_==RuntimeMode::Play?audio_.inputDeliveryGrace():0;
  const bool finished=now>=audio_.duration() && (grace==0 || (sampled && sampleSong>=audio_.duration()+grace));
  if(mode_==RuntimeMode::Play){
    for(;;){
      InputEvent event;TraceInput i{};
      if(hasPending_){event=pending_;i=pendingTrace_;hasPending_=false;}
      else{
        InputEnvelope envelope;if(!queue_.popTraced(envelope))break;const auto& raw=envelope.raw;
        i.event=envelope;i.consumed=trace_ && trace_->active()?monotonicNow():0;i.valid=ConsumeValue|WatermarkValue|SongValue;i.before=i.after=watermark_;i.now=now;i.sample=mappingSample_;
        event={raw.pointerId,raw.phase,raw.position,0};auto error=mapper_.map(raw.hostTime,event.songTime);
        if(error!=Error::Ok){
          ++diagnostics_.rejectedInputs;result=error;i.stage=TraceStage::Terminal;i.disposition=TraceDisposition::Rejected;i.error=error;i.reason=raw.hostTime<epochHost_?TraceReason::BeforeEpoch:TraceReason::MappedOutOfRange;
          if(trace_)trace_->input(TraceSource::Session,i);
          continue;
        }
        i.mapped=event.songTime;i.sample=mappingSample_;i.valid|=MappedValue;i.stage=TraceStage::Map;if(trace_)trace_->input(TraceSource::Session,i);
      }
      i.before=i.after=watermark_;i.now=now;
      diagnostics_.maxInputDelay=std::max(diagnostics_.maxInputDelay,now-event.songTime);
      if(event.songTime<watermark_){++diagnostics_.rejectedInputs;result=Error::LateInput;i.stage=TraceStage::Terminal;i.disposition=TraceDisposition::Rejected;i.reason=TraceReason::BeforeWatermark;i.error=result;if(trace_)trace_->input(TraceSource::Session,i);continue;}
      if(event.songTime>now){pending_=event;hasPending_=true;i.stage=TraceStage::Pending;i.disposition=TraceDisposition::Pending;pendingTrace_=i;if(trace_)trace_->input(TraceSource::Session,i);break;}
      if(replay_.events.size()==maxEvents){i.stage=TraceStage::Terminal;i.disposition=TraceDisposition::Rejected;i.reason=TraceReason::ReplayFull;i.error=Error::ReplayFull;if(trace_)trace_->input(TraceSource::Session,i);pause();return observe.result(Error::ReplayFull);}
      replay_.events.push_back(event);feed(event);watermark_=event.songTime;i.after=watermark_;i.stage=TraceStage::Terminal;i.disposition=TraceDisposition::Accepted;if(trace_)trace_->input(TraceSource::Session,i);
    }
  }else while(replayCursor_<replay_.events.size() && replay_.events[replayCursor_].songTime<=now)feed(replay_.events[replayCursor_++]);
  const TimeNs horizon=finished?now:std::max(TimeNs{0},now-grace);
  rules_->advance(horizon,*this);watermark_=std::max(watermark_,horizon);
  if(finished){audio_.pause();clock_.pause();state_=SessionState::Finished;}
  return observe.result(result);
}
void GameplaySession::render(IRenderBackend& output,TimeNs host){
  Observation observe(*this,TraceOperation::Render,host);
  if(!chart_ || !output.begin())return;
  renderer_->render(*chart_,clock_.now(),output);output.end();++diagnostics_.frames;
  if(previousFrame_ && host>previousFrame_)diagnostics_.fps=static_cast<double>(second)/static_cast<double>(host-previousFrame_);
  previousFrame_=host;
}
}
