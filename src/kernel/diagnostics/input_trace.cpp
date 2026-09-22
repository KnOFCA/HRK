#include "kernel/diagnostics/input_trace.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <thread>

namespace hrk {
namespace {
constexpr const char* errors[]={"Ok","CaptureInvalidState","CaptureAllocationFailed","CaptureFull","CaptureCounterOverflow","CaptureConcurrencyUnsupported","CaptureBusy","CaptureExportFailed","TraceIncomplete","TraceMissingReference"};
constexpr const char* stages[]={"receive","platform_enqueue","platform_poll","session_enqueue","map","pending","terminal"};
constexpr const char* dispositions[]={"observed","queued","pending","accepted","rejected","cleared"};
constexpr const char* reasons[]={nullptr,"before_epoch","mapped_out_of_range","before_watermark","platform_queue_full","session_queue_full","invalid_input","invalid_state","replay_full","lifecycle_clear","inactive_drain"};
constexpr const char* phases[]={"DOWN","MOVE","UP","CANCEL","UNKNOWN"};
constexpr const char* operations[]={"load","start","pause","resume","seek","stop","background","foreground","surface_create","surface_resize","surface_destroy","render","status","update","measure","request_120hz","unknown_command"};
size_t phaseIndex(InputPhase p){return std::min<size_t>(static_cast<uint32_t>(p),4);}
void quoted(std::ostream& o,const std::string& s){
  o<<'"';for(unsigned char c:s){switch(c){case '"':o<<"\\\"";break;case '\\':o<<"\\\\";break;default:if(c<32){const char* hex="0123456789abcdef";o<<"\\u00"<<hex[c>>4]<<hex[c&15];}else o<<static_cast<char>(c);}}o<<'"';
}
template<class T> void number(std::ostream& o,T n){o<<'"'<<n<<'"';}
template<class T> void nullable(std::ostream& o,T n,bool valid){if(valid)number(o,n);else o<<"null";}
void floating(std::ostream& o,float v){if(std::isfinite(v))o<<std::setprecision(std::numeric_limits<float>::max_digits10)<<v;else quoted(o,std::isnan(v)?"NaN":v>0?"+Infinity":"-Infinity");}
bool utf8(const std::string& s){
  for(size_t i=0;i<s.size();){const auto c=static_cast<unsigned char>(s[i++]);if(c<0x80)continue;
    unsigned n=0;uint32_t cp=0,min=0;if(c>=0xc2 && c<=0xdf){n=1;cp=c&31;min=0x80;}else if(c>=0xe0 && c<=0xef){n=2;cp=c&15;min=0x800;}else if(c>=0xf0 && c<=0xf4){n=3;cp=c&7;min=0x10000;}else return false;
    if(i+n>s.size())return false;
    while(n--){auto b=static_cast<unsigned char>(s[i++]);if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&63);}
    if(cp<min || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))return false;
  }return true;
}
bool validHeader(const TraceHeader& h){
  for(const auto* s:{&h.runId,&h.sourceRevision,&h.specRevision,&h.policyVersion,&h.buildMode,&h.deviceModel,&h.osVersion,&h.chartSha256,&h.audioSha256,&h.origin})if(s->empty() || s->size()>256 || !utf8(*s))return false;
  for(const auto* s:{&h.chartSha256,&h.audioSha256})if(s->size()!=64 || s->find_first_not_of("0123456789abcdef")!=std::string::npos)return false;
  return (h.buildMode=="Debug" || h.buildMode=="Release") && (h.origin=="human" || h.origin=="injected" || h.origin=="synthetic") && std::isfinite(h.actualRefreshHz) && h.actualRefreshHz>=0;
}
}
const char* captureErrorName(CaptureError e){return errors[static_cast<size_t>(e)];}
void InputTrace::issue(CaptureError e){issues_.fetch_or(uint32_t{1}<<static_cast<unsigned>(e),std::memory_order_relaxed);}
uint64_t InputTrace::increment(std::atomic<uint64_t>& value){
  auto v=value.load(std::memory_order_relaxed);
  for(;;){if(v==std::numeric_limits<uint64_t>::max()){issue(CaptureError::CaptureCounterOverflow);open_.store(false,std::memory_order_release);return 0;}if(value.compare_exchange_weak(v,v+1,std::memory_order_relaxed))return v+1;}
}
uint64_t InputTrace::next(uint64_t& v){if(v==std::numeric_limits<uint64_t>::max()){issue(CaptureError::CaptureCounterOverflow);open_.store(false,std::memory_order_release);return 0;}return ++v;}
InputTrace::Writer::Writer(InputTrace* ptr){
  if(!ptr)return;
  auto& t=*ptr;
  // Increment before rechecking the gate. Rejected entrants never access buffers.
  auto n=t.increment(t.writers_);if(!n)return;
  if(!t.open_.load(std::memory_order_acquire)){t.writers_.fetch_sub(1,std::memory_order_release);return;}
  trace_=&t;if(n>1)t.issue(CaptureError::CaptureConcurrencyUnsupported);
}
InputTrace::Writer::~Writer(){if(trace_)trace_->writers_.fetch_sub(1,std::memory_order_release);}
InputTrace::~InputTrace(){open_.store(false,std::memory_order_release);while(writers_.load(std::memory_order_acquire))std::this_thread::yield();}
CaptureError InputTrace::beginCapture(const TraceHeader& h,bool busy){
  if(busy || active() || writers_.load(std::memory_order_acquire) || (platform_ && !frozen_))return CaptureError::CaptureInvalidState;
  if(!validHeader(h))return CaptureError::CaptureInvalidState;
  if(generation_==std::numeric_limits<uint64_t>::max())return CaptureError::CaptureCounterOverflow;
  try{
    // No allocation in callbacks/update; release an old immutable snapshot here only.
    platform_.reset();session_.reset();eventStates_.reset();header_=h;
    platform_=std::make_unique<TraceSlot[]>(capacity);session_=std::make_unique<TraceSlot[]>(capacity);eventStates_=std::make_unique<uint8_t[]>(capacity);
  }catch(const std::bad_alloc&){platform_.reset();session_.reset();eventStates_.reset();frozen_=false;return CaptureError::CaptureAllocationFailed;}
  ++generation_;epoch_=0;events_=0;batches_=0;issues_=0;
  for(auto& v:used_)v=0;
  for(auto& v:dropped_)v=0;
  for(auto& p:counts_)for(auto& d:p)for(auto& r:d)r=0;
  actionCounter_=updateCounter_=action_=control_=update_=0;queryIndex_=0;frozen_=false;summary_={};open_.store(true,std::memory_order_release);return CaptureError::Ok;
}
size_t InputTrace::allocatedBytes() const{
  size_t strings=0;for(const auto* s:{&header_.runId,&header_.sourceRevision,&header_.specRevision,&header_.policyVersion,&header_.buildMode,&header_.deviceModel,&header_.osVersion,&header_.chartSha256,&header_.audioSha256,&header_.origin})strings+=s->capacity()+1;
  return platform_?2*capacity*sizeof(TraceSlot)+capacity+sizeof(*this)+strings:sizeof(*this);
}
uint64_t InputTrace::append(TraceSource s,const TraceRecord& r){
  if(!platform_)return 0;
  const auto source=static_cast<size_t>(s);const auto n=increment(used_[source]);if(!n)return 0;
  if(n>capacity){increment(dropped_[source]);issue(CaptureError::CaptureFull);return 0;}
  auto& slot=(s==TraceSource::Platform?platform_:session_)[static_cast<size_t>(n-1)];slot.record=r;slot.ready.store(true,std::memory_order_release);
  return n|(s==TraceSource::Session?sessionHandle:0);
}
uint64_t InputTrace::recordCount(TraceSource s) const{return std::min<uint64_t>(used_[static_cast<size_t>(s)].load(),capacity);}
const TraceRecord& InputTrace::record(TraceSource s,size_t i) const{
  const auto& slot=(s==TraceSource::Platform?platform_:session_)[i];(void)slot.ready.load(std::memory_order_acquire);return slot.record;
}
uint64_t InputTrace::sequence(uint64_t h) const{return !h?0:(h&sessionHandle)?recordCount(TraceSource::Platform)+(h&~sessionHandle)+1:h+1;}
InputEnvelope InputTrace::receive(const RawInputEvent& raw,TimeNs received,uint64_t batchId,uint32_t point,TraceSource source){
  InputEnvelope e{raw,{increment(events_),batchId,epoch(),generation_,received,point}};
  TraceInput i{};i.event=e;input(source,i);return e;
}
void InputTrace::input(TraceSource s,const TraceInput& i){
  if(!belongs(i.event.trace) || (s==TraceSource::Session && !active()))return;
  if(i.stage==TraceStage::Terminal){auto d=static_cast<size_t>(i.disposition);if(d>=3 && d<=5)increment(counts_[phaseIndex(i.event.raw.phase)][d-3][static_cast<size_t>(i.reason)]);else issue(CaptureError::TraceIncomplete);}
  TraceRecord r;r.epoch=s==TraceSource::Platform?i.event.trace.epoch:epoch();r.data.input=i;
  if(s==TraceSource::Session){r.action=action_;r.control=control_;r.update=update_;}
  append(s,r);
}
void InputTrace::platformEnqueue(const InputEnvelope& e,bool queued){
  TraceInput i{};i.event=e;i.stage=queued?TraceStage::PlatformEnqueue:TraceStage::Terminal;i.disposition=queued?TraceDisposition::Queued:TraceDisposition::Rejected;
  if(!queued){i.reason=TraceReason::PlatformQueueFull;i.error=Error::QueueFull;}input(TraceSource::Platform,i);
}
void InputTrace::platformPoll(const InputEnvelope& e){TraceInput i{};i.event=e;i.stage=TraceStage::PlatformPoll;input(TraceSource::Session,i);}
void InputTrace::cleared(const InputEnvelope& e,TraceReason reason){TraceInput i{};i.event=e;i.stage=TraceStage::Terminal;i.disposition=TraceDisposition::Cleared;i.reason=reason;input(TraceSource::Session,i);}
uint64_t InputTrace::clock(TraceQuery q,TimeNs host,TimeNs song,TimeNs position,bool success){
  if(!active())return 0;
  if(queryIndex_==std::numeric_limits<uint32_t>::max()){issue(CaptureError::CaptureCounterOverflow);open_=false;return 0;}
  TraceRecord r;r.kind=TraceKind::Clock;r.epoch=epoch();r.action=action_;r.data.clock={host,song,position,q,caller_,queryIndex_++,success};return append(TraceSource::Session,r);
}
uint64_t InputTrace::anchor(TimeNs host,TimeNs song){if(!active())return 0;increment(epoch_);return clock(TraceQuery::Anchor,host,song,0,true);}
InputTrace::Scope::Scope(InputTrace* t,TraceOperation op,TimeNs host,TimeNs target,bool isUpdate):trace(t && t->active()?t:nullptr),oldAction(0),oldControl(0),oldUpdate(0),oldCaller(TraceOperation::Status){
  if(!trace)return;
  oldAction=t->action_;oldControl=t->control_;oldUpdate=t->update_;oldCaller=t->caller_;
  if(!t->action_){t->action_=t->next(t->actionCounter_);t->queryIndex_=0;}
  t->caller_=op;
  begin.kind=isUpdate?TraceKind::Update:TraceKind::Control;begin.epoch=t->epoch();begin.action=t->action_;begin.data.boundary={};begin.data.boundary.host=host;begin.data.boundary.target=target;begin.data.boundary.operation=op;
  if(isUpdate){t->update_=t->next(t->updateCounter_);begin.update=t->update_;}else begin.update=t->update_;
  const auto handle=t->append(TraceSource::Session,begin);if(!isUpdate)t->control_=handle;
}
InputTrace::Scope::~Scope(){if(trace){trace->action_=oldAction;trace->control_=oldControl;trace->update_=oldUpdate;trace->caller_=oldCaller;}}
void InputTrace::Scope::finish(Error e,const std::array<uint32_t,32>& ids,uint32_t count,TimeNs before,TimeNs after,TimeNs now,bool hasTimes){
  if(!trace)return;
  auto r=begin;r.epoch=trace->epoch();auto& b=r.data.boundary;b.end=true;b.result=e;b.pointers=ids;b.pointerCount=count;b.before=before;b.after=after;b.now=now;b.valid=hasTimes?WatermarkValue|SongValue:0;trace->append(TraceSource::Session,r);
}
InputTrace::Transfer::Transfer(InputTrace* t):trace(t && t->active()?t:nullptr),old(0){if(trace){old=trace->action_;if(!old){trace->action_=trace->next(trace->actionCounter_);trace->queryIndex_=0;}}}
InputTrace::Transfer::~Transfer(){if(trace)trace->action_=old;}
TimeNs TraceAudioBackend::position() const{auto result=backend_.position();if(trace_)trace_->clock(TraceQuery::Position,0,0,result,true);return result;}
bool TraceAudioBackend::clockSample(TimeNs& host,TimeNs& song) const{const bool ok=backend_.clockSample(host,song);if(trace_){auto h=trace_->clock(TraceQuery::ClockSample,ok?host:0,ok?song:0,0,ok);if(ok)lastSample_=h;}return ok;}
CaptureError InputTrace::endCapture(bool playing){
  if(playing || !platform_)return CaptureError::CaptureInvalidState;
  if(frozen_)return CaptureError::Ok;
  open_.store(false,std::memory_order_release);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1000);
  while(writers_.load(std::memory_order_acquire)){if(std::chrono::steady_clock::now()>=deadline)return CaptureError::CaptureBusy;std::this_thread::yield();}
  reconcile();frozen_=true;return CaptureError::Ok;
}
void InputTrace::reconcile(){
  summary_={};summary_.ingress=events_.load();summary_.platformDropped=dropped_[0];summary_.sessionDropped=dropped_[1];
  for(auto& p:counts_)for(size_t d=0;d<3;++d)for(size_t r=0;r<static_cast<size_t>(TraceReason::Count);++r){const auto n=p[d][r].load();if(d==0)summary_.accepted+=n;else if(d==1)summary_.rejected+=n;else summary_.cleared+=n;if(r==static_cast<size_t>(TraceReason::PlatformQueueFull))summary_.platformQueueDropped+=n;if(r==static_cast<size_t>(TraceReason::SessionQueueFull))summary_.sessionQueueDropped+=n;}
  const auto terminal=summary_.accepted+summary_.rejected+summary_.cleared;
  if(terminal>summary_.ingress)issue(CaptureError::TraceIncomplete);else summary_.pending=summary_.ingress-terminal;
  std::fill_n(eventStates_.get(),capacity,uint8_t{0});uint64_t received=0,ended=0;
  std::array<TraceRecord,32> stack{};size_t depth=0;
  uint64_t observedCounts[5][3][static_cast<size_t>(TraceReason::Count)]{};
  for(auto source:{TraceSource::Platform,TraceSource::Session})for(size_t j=0;j<recordCount(source);++j){
    const auto& slot=(source==TraceSource::Platform?platform_:session_)[j];if(!slot.ready.load(std::memory_order_acquire)){issue(CaptureError::TraceIncomplete);continue;}
    const auto& r=slot.record;
    if(source==TraceSource::Session && !r.action)issue(CaptureError::TraceMissingReference);
    if(r.kind==TraceKind::Control || r.kind==TraceKind::Update){
      if(!r.data.boundary.end){if(depth<stack.size())stack[depth++]=r;else issue(CaptureError::TraceIncomplete);}
      else if(!depth)issue(CaptureError::TraceIncomplete);else{const auto& b=stack[--depth];if(b.action!=r.action || b.kind!=r.kind || b.update!=r.update || b.data.boundary.operation!=r.data.boundary.operation)issue(CaptureError::TraceIncomplete);}
    }
    if(r.kind!=TraceKind::Input)continue;
    const auto& i=r.data.input;const auto id=i.event.trace.event;
    if(!id || id>capacity){issue(CaptureError::TraceIncomplete);continue;}
    auto& state=eventStates_[static_cast<size_t>(id-1)];
    if(i.stage==TraceStage::Receive){if(state&1)issue(CaptureError::TraceIncomplete);state|=1;++received;}
    else if(!(state&1))issue(CaptureError::TraceMissingReference);
    if(state&2)issue(CaptureError::TraceIncomplete);
    if(i.stage==TraceStage::PlatformEnqueue)state|=8;
    if(i.stage==TraceStage::PlatformPoll){if(!(state&8))issue(CaptureError::TraceMissingReference);state|=16;}
    if(i.stage==TraceStage::SessionEnqueue){if(i.event.trace.batch && !(state&16))issue(CaptureError::TraceMissingReference);state|=32;}
    if(i.stage==TraceStage::Map){if(!(state&32))issue(CaptureError::TraceMissingReference);state|=4;}
    if(i.sample || (i.valid&MappedValue)){
      const auto index=(i.sample&~sessionHandle);if(!(i.sample&sessionHandle) || !index || index>recordCount(TraceSource::Session))issue(CaptureError::TraceMissingReference);
      else{const auto& c=record(TraceSource::Session,static_cast<size_t>(index-1));if(c.kind!=TraceKind::Clock || !c.data.clock.success || c.data.clock.query==TraceQuery::Position)issue(CaptureError::TraceMissingReference);}
    }
    if(i.stage==TraceStage::Terminal){if(i.disposition==TraceDisposition::Accepted && !(state&4))issue(CaptureError::TraceMissingReference);state|=2;++ended;const auto d=static_cast<size_t>(i.disposition),reason=static_cast<size_t>(i.reason);if(d>=3 && d<=5 && reason<static_cast<size_t>(TraceReason::Count))++observedCounts[phaseIndex(i.event.raw.phase)][d-3][reason];else issue(CaptureError::TraceIncomplete);}
  }
  if(depth || received!=summary_.ingress || ended!=terminal)issue(CaptureError::TraceIncomplete);
  for(size_t p=0;p<5;++p)for(size_t d=0;d<3;++d)for(size_t r=0;r<static_cast<size_t>(TraceReason::Count);++r)if(observedCounts[p][d][r]!=counts_[p][d][r].load())issue(CaptureError::TraceIncomplete);
  summary_.complete=issues_.load()==0;
}

void InputTrace::writeJson(std::ostream& o) const{
  o.imbue(std::locale::classic());
  o<<"{\"kind\":\"header\",\"recordSequence\":\"1\",\"traceVersion\":1,\"runId\":";quoted(o,header_.runId);o<<",\"provenance\":{";
  bool comma=false;auto textField=[&](const char* key,const std::string& value){if(comma)o<<',';comma=true;quoted(o,key);o<<':';quoted(o,value);};
  textField("sourceRevision",header_.sourceRevision);textField("specRevision",header_.specRevision);textField("policyVersion",header_.policyVersion);textField("buildMode",header_.buildMode);textField("deviceModel",header_.deviceModel);textField("osVersion",header_.osVersion);textField("chartSha256",header_.chartSha256);textField("audioSha256",header_.audioSha256);textField("origin",header_.origin);
  o<<",\"actualRefreshHz\":";if(header_.actualRefreshHz>0)o<<std::setprecision(17)<<header_.actualRefreshHz;else o<<"null";
  o<<"},\"captureConfig\":{\"enabled\":true,\"platformCapacity\":"<<capacity<<",\"sessionCapacity\":"<<capacity<<",\"slotBytes\":"<<sizeof(TraceSlot)<<",\"memoryBudgetBytes\":"<<memoryBudget<<",\"hostClock\":\"CLOCK_MONOTONIC\",\"timeUnit\":\"ns\",\"inputDeliveryGrace\":";number(o,header_.inputDeliveryGrace);o<<",\"policy\":\"v0.1.0-observe\"}}\n";
  uint64_t seq=1;
  for(auto source:{TraceSource::Platform,TraceSource::Session})for(size_t j=0;j<recordCount(source);++j){
    const auto& r=record(source,j);o<<"{\"kind\":";quoted(o,r.kind==TraceKind::Input?"input":r.kind==TraceKind::Clock?"clock":r.kind==TraceKind::Control?"control":"update");o<<",\"recordSequence\":";number(o,++seq);o<<",\"source\":";quoted(o,source==TraceSource::Platform?"platform":"session");o<<",\"sourceSequence\":";number(o,j+1);o<<",\"epoch\":";number(o,r.epoch);
    if(r.kind==TraceKind::Input){const auto& i=r.data.input;const auto& e=i.event;
      o<<",\"eventSequence\":";number(o,e.trace.event);o<<",\"batchSequence\":";nullable(o,e.trace.batch,e.trace.batch!=0);o<<",\"pointIndex\":";if(e.trace.batch)o<<e.trace.point;else o<<"null";
      o<<",\"pointerId\":"<<e.raw.pointerId<<",\"phase\":";quoted(o,phases[phaseIndex(e.raw.phase)]);o<<",\"rawPhase\":"<<static_cast<uint32_t>(e.raw.phase)<<",\"position\":{\"x\":";floating(o,e.raw.position.x);o<<",\"y\":";floating(o,e.raw.position.y);o<<"},\"rawHostTime\":";number(o,e.raw.hostTime);o<<",\"receiveHostTime\":";number(o,e.trace.received);
      o<<",\"consumeHostTime\":";nullable(o,i.consumed,i.valid&ConsumeValue);o<<",\"mappedSongTime\":";nullable(o,i.mapped,i.valid&MappedValue);o<<",\"mappingSampleSequence\":";nullable(o,sequence(i.sample),i.sample!=0);
      o<<",\"watermarkBefore\":";nullable(o,i.before,i.valid&WatermarkValue);o<<",\"watermarkAfter\":";nullable(o,i.after,i.valid&WatermarkValue);o<<",\"songNow\":";nullable(o,i.now,i.valid&SongValue);
      o<<",\"stage\":";quoted(o,stages[static_cast<size_t>(i.stage)]);o<<",\"disposition\":";quoted(o,dispositions[static_cast<size_t>(i.disposition)]);o<<",\"error\":";quoted(o,errorName(i.error));o<<",\"reason\":";if(i.reason==TraceReason::None)o<<"null";else quoted(o,reasons[static_cast<size_t>(i.reason)]);
      o<<",\"actionSequence\":";nullable(o,r.action,r.action!=0);o<<",\"controlSequence\":";nullable(o,sequence(r.control),r.control!=0);o<<",\"updateSequence\":";nullable(o,r.update,r.update!=0);
    }else if(r.kind==TraceKind::Clock){const auto& c=r.data.clock;
      o<<",\"sampleHostTime\":";nullable(o,c.host,c.success && c.query!=TraceQuery::Position);o<<",\"sampleSongTime\":";nullable(o,c.song,c.success && c.query!=TraceQuery::Position);o<<",\"sampleSuccess\":"<<(c.success?"true":"false")<<",\"positionResult\":";nullable(o,c.position,c.query==TraceQuery::Position);o<<",\"query\":";quoted(o,c.query==TraceQuery::Position?"position":c.query==TraceQuery::ClockSample?"clockSample":"anchor");o<<",\"caller\":";quoted(o,operations[static_cast<size_t>(c.caller)]);o<<",\"actionSequence\":";number(o,r.action);o<<",\"queryIndex\":"<<c.index;
    }else{const auto& b=r.data.boundary;
      o<<",\"hostTime\":";number(o,b.host);o<<",\"actionSequence\":";number(o,r.action);o<<",\"boundary\":";quoted(o,b.end?"end":"begin");o<<",\"result\":";if(b.end)quoted(o,errorName(b.result));else o<<"null";
      if(r.kind==TraceKind::Control){o<<",\"operation\":";quoted(o,operations[static_cast<size_t>(b.operation)]);o<<",\"targetSongTime\":";nullable(o,b.target,b.operation==TraceOperation::Seek);}else{o<<",\"updateSequence\":";number(o,r.update);o<<",\"watermarkBefore\":";nullable(o,b.before,b.valid&WatermarkValue);o<<",\"watermarkAfter\":";nullable(o,b.after,b.valid&WatermarkValue);o<<",\"songNow\":";nullable(o,b.now,b.valid&SongValue);}
      o<<",\"activePointers\":";if(!b.end)o<<"null";else{o<<'[';for(uint32_t k=0;k<b.pointerCount;++k){if(k)o<<',';o<<b.pointers[k];}o<<']';}
    }o<<"}\n";
  }
  o<<"{\"kind\":\"summary\",\"recordSequence\":";number(o,++seq);o<<",\"countsByPhaseAndReason\":[";comma=false;
  for(size_t p=0;p<5;++p)for(size_t d=0;d<3;++d)for(size_t r=0;r<static_cast<size_t>(TraceReason::Count);++r){auto n=counts_[p][d][r].load();if(!n)continue;if(comma)o<<',';comma=true;o<<"{\"phase\":";quoted(o,phases[p]);o<<",\"disposition\":";quoted(o,dispositions[d+3]);o<<",\"reason\":";if(!r)o<<"null";else quoted(o,reasons[r]);o<<",\"count\":";number(o,n);o<<'}';}
  o<<"],\"captureDropped\":";number(o,summary_.platformDropped+summary_.sessionDropped);o<<",\"captureDroppedBySource\":{\"platform\":";number(o,summary_.platformDropped);o<<",\"session\":";number(o,summary_.sessionDropped);o<<"},\"platformQueueDropped\":";number(o,summary_.platformQueueDropped);o<<",\"sessionQueueDropped\":";number(o,summary_.sessionQueueDropped);
  o<<",\"ingressCount\":";number(o,summary_.ingress);o<<",\"acceptedCount\":";number(o,summary_.accepted);o<<",\"rejectedCount\":";number(o,summary_.rejected);o<<",\"clearedCount\":";number(o,summary_.cleared);o<<",\"pendingAtEnd\":";number(o,summary_.pending);o<<",\"pendingEventSequences\":[";comma=false;
  for(size_t id=0;id<capacity;++id)if((eventStates_[id]&3)==1){if(comma)o<<',';comma=true;number(o,id+1);}
  o<<"],\"complete\":"<<(summary_.complete?"true":"false")<<",\"incompleteReasons\":[";comma=false;
  const auto flags=issues_.load();for(unsigned i=1;i<sizeof(errors)/sizeof(*errors);++i)if(flags&(uint32_t{1}<<i)){if(comma)o<<',';comma=true;quoted(o,errors[i]);}o<<"]}\n";
}
CaptureError InputTrace::exportCapture(const std::string& path) const{
  if(!frozen_)return CaptureError::CaptureInvalidState;
  if(path.empty() || path.size()>4096 || path.find('\0')!=std::string::npos || !utf8(path))return CaptureError::CaptureExportFailed;
  namespace fs=std::filesystem;fs::path temporary;bool owned=false;
  try{
    const auto destination=fs::u8path(path);temporary=destination;temporary+=".capture-tmp";
    if(fs::exists(destination) || !fs::create_directory(temporary))return CaptureError::CaptureExportFailed;
    owned=true;const auto file=temporary/"data";
    {std::ofstream out(file,std::ios::binary);out.exceptions(std::ios::badbit|std::ios::failbit);writeJson(out);out.flush();out.close();}
    // Atomic, no-clobber publication: an existing destination makes link fail.
    fs::create_hard_link(file,destination);std::error_code ignored;fs::remove(file,ignored);fs::remove(temporary,ignored);return CaptureError::Ok;
  }catch(const std::exception&){if(owned){std::error_code ignored;fs::remove(temporary/"data",ignored);fs::remove(temporary,ignored);}return CaptureError::CaptureExportFailed;}
}
}
