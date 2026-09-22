#include "app/composition.h"
#include "games/reference/reference.h"
#include "platform/headless/headless.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <thread>

std::atomic<uint64_t> allocations{0}, allocatedBytes{0};
std::atomic<bool> failAllocation{false};
void* operator new(std::size_t n){if(failAllocation)throw std::bad_alloc();if(auto* p=std::malloc(n?n:1)){++allocations;allocatedBytes+=n;return p;}throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
void* operator new(std::size_t n,const std::nothrow_t&) noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void operator delete(void* p,const std::nothrow_t&) noexcept{std::free(p);}void operator delete[](void* p,const std::nothrow_t&) noexcept{std::free(p);}

using namespace hrk;
namespace hrk {
struct InputTraceTestAccess {
  static void exhaustEvents(InputTrace& t){t.events_=std::numeric_limits<uint64_t>::max();}
  static void reserveAndCommit(InputTrace& t,std::atomic<bool>& reserved,std::atomic<bool>& release){
    InputTrace::Writer writer(t);auto n=t.increment(t.used_[0]);reserved=true;
    while(!release.load())std::this_thread::yield();
    auto& slot=t.platform_[static_cast<size_t>(n-1)];slot.record={};slot.ready.store(true,std::memory_order_release);
  }
};
}
namespace {
void check(bool ok,const char* what="assertion"){if(!ok)throw std::runtime_error(what);}
int passed=0,failed=0;
template<class F> void test(const char* name,F f){try{f();++passed;std::cout<<name<<" PASS\n";}catch(const std::exception& e){++failed;std::cout<<name<<" FAIL "<<e.what()<<'\n';}}
TraceHeader header(){
  auto env=[](const char* key){
#ifdef _MSC_VER
    char value[512]{};size_t required=0;check(getenv_s(&required,value,sizeof(value),key)==0 && required>1,"run via check-input-trace.mjs for asset provenance");return std::string(value);
#else
    const char* value=std::getenv(key);check(value!=nullptr,"run via check-input-trace.mjs for asset provenance");return std::string(value);
#endif
  };
  TraceHeader h;h.runId=env("HRK_TRACE_RUN_ID");h.sourceRevision=env("HRK_TRACE_SOURCE");h.specRevision=env("HRK_TRACE_SPEC");
#ifdef NDEBUG
  h.buildMode="Release";
#else
  h.buildMode="Debug";
#endif
  h.deviceModel=env("HRK_TRACE_MACHINE");h.osVersion=env("HRK_TRACE_OS");h.origin="synthetic";h.chartSha256=env("HRK_TRACE_CHART");h.audioSha256=env("HRK_TRACE_AUDIO");return h;
}
struct Audio:FakeAudioBackend {
  bool sampled=false;TimeNs sampleHost=10*second,sampleSong=0;
  mutable uint64_t positions=0,samples=0;
  TimeNs position() const override{++positions;return FakeAudioBackend::position();}
  bool clockSample(TimeNs& h,TimeNs& s) const override{++samples;if(!sampled)return false;h=sampleHost;s=sampleSong;return true;}
};
struct Fixture {
  InputTrace trace;reference::ReferenceGame game;Audio audio;GameplaySession session{game,audio};TracedPlatformQueue platform;
  Fixture(bool enabled=true){session.setTrace(&trace);platform.setTrace(&trace);if(enabled)check(trace.beginCapture(header(),false)==CaptureError::Ok);}
  void load(){auto chart=game.createChartLoader()->load(R"({"bpm":120,"notes":[{"beat":2,"x":0.5}]})");Pcm pcm;pcm.samples.resize(48000*2*8);check(session.load(chart,pcm,RuntimeMode::Play)==Error::Ok);check(session.start(10*second)==Error::Ok);}
  void submit(InputPhase phase,TimeNs song=second,uint32_t id=1){InputTrace::Writer writer(trace);platform.submit({id,phase,{.5f,.8f},10*second+song},12*second,writer?trace.batch():0,0,static_cast<bool>(writer));}
  void poll(){for(;;){InputEnvelope e;if(!platform.poll(e))break;InputTrace::Transfer action(&trace);trace.platformPoll(e);if(session.state()==SessionState::Playing)session.pushTraced(e);else trace.cleared(e,TraceReason::InactiveDrain);}}
  void clear(){InputTrace::Scope scope(&trace,TraceOperation::Background,monotonicNow());platform.clear();session.finishTrace(scope);}
  void end(){check(trace.endCapture(session.state()==SessionState::Playing)==CaptureError::Ok);}
};
std::vector<TraceInput> inputs(const InputTrace& t,TraceStage stage){std::vector<TraceInput> result;for(auto source:{TraceSource::Platform,TraceSource::Session})for(size_t i=0;i<t.recordCount(source);++i){const auto& r=t.record(source,i);if(r.kind==TraceKind::Input && r.data.input.stage==stage)result.push_back(r.data.input);}return result;}
uint64_t reasons(const InputTrace& t,TraceReason reason){uint64_t n=0;for(const auto& i:inputs(t,TraceStage::Terminal))if(i.reason==reason)++n;return n;}
std::filesystem::path output;
void save(InputTrace& trace,const char* name){check(trace.exportCapture((output/name).string())==CaptureError::Ok,"export");}
}
int main(int argc,char** argv){
  output=argc>1?argv[1]:"trace-test-output";std::filesystem::create_directories(output);
  test("TRACE-DEFAULT-AND-STATE",[]{Fixture f(false);check(!f.trace.active());check(f.trace.endCapture(false)==CaptureError::CaptureInvalidState);check(f.trace.exportCapture("unused")==CaptureError::CaptureInvalidState);f.load();check(f.trace.beginCapture(header(),true)==CaptureError::CaptureInvalidState);f.session.pause();check(f.trace.beginCapture(header(),true)==CaptureError::CaptureInvalidState);f.session.stop();check(f.trace.beginCapture(header(),false)==CaptureError::Ok);check(f.trace.beginCapture(header(),false)==CaptureError::CaptureInvalidState);check(f.trace.endCapture(true)==CaptureError::CaptureInvalidState);f.end();check(f.trace.summary().complete);save(f.trace,"empty.jsonl");});
  test("TRACE-ALLOCATION-FAILURE",[]{InputTrace trace;auto h=header();failAllocation=true;const auto e=trace.beginCapture(h,false);failAllocation=false;check(e==CaptureError::CaptureAllocationFailed);check(!trace.active());check(trace.beginCapture(h,false)==CaptureError::Ok);check(trace.endCapture(false)==CaptureError::Ok);});
  test("TRACE-MEMORY-AND-NO-HOT-ALLOCATION",[]{Fixture f(false);auto h=header();const auto bytes=allocatedBytes.load();check(f.trace.beginCapture(h,false)==CaptureError::Ok);const auto added=allocatedBytes.load()-bytes;check(added+sizeof(InputTrace)+4098*sizeof(TraceContext)<InputTrace::memoryBudget);check(f.trace.allocatedBytes()+4098*sizeof(TraceContext)<InputTrace::memoryBudget);f.load();const auto before=allocations.load();for(uint32_t p=0;p<4;++p)f.submit(static_cast<InputPhase>(p));f.poll();f.audio.advance(second);check(f.session.update()==Error::Ok);f.session.pause();check(allocations.load()==before,"hot path allocated");f.end();check(f.trace.summary().complete);std::cout<<"slotBytes="<<sizeof(TraceSlot)<<" captureAllocated="<<f.trace.allocatedBytes()<<" measuredBeginBytes="<<added<<'\n';});
  test("TRACE-PHASES-AND-BRANCHES",[]{Fixture f;f.load();for(uint32_t p=0;p<4;++p)f.submit(static_cast<InputPhase>(p));f.poll();f.audio.advance(second);check(f.session.update()==Error::Ok);
    for(uint32_t p=0;p<4;++p)f.submit(static_cast<InputPhase>(p),second-1);
    f.poll();check(f.session.update()==Error::LateInput);
    for(uint32_t p=0;p<4;++p)f.submit(static_cast<InputPhase>(p),2*second);
    f.poll();check(f.session.update()==Error::Ok);f.session.pause();f.end();
    const auto& s=f.trace.summary();check(s.complete && s.ingress==12 && s.accepted==4 && s.rejected==4 && s.cleared==4 && !s.pending);check(reasons(f.trace,TraceReason::BeforeWatermark)==4);save(f.trace,"phases.jsonl");});
  test("TRACE-PENDING-RETAINS-MAPPING",[]{Fixture f;f.load();f.submit(InputPhase::Down,2*second);f.poll();f.audio.advance(second);check(f.session.update()==Error::Ok);auto pending=inputs(f.trace,TraceStage::Pending).front();f.audio.sampled=true;f.audio.sampleHost=12*second;f.audio.sampleSong=3*second;f.audio.advance(second);check(f.session.update()==Error::Ok);auto accepted=inputs(f.trace,TraceStage::Terminal).front();check(accepted.mapped==pending.mapped && accepted.sample==pending.sample && accepted.consumed==pending.consumed);check(f.session.replay().events.front().songTime==2*second);f.session.pause();f.end();check(f.trace.summary().complete);save(f.trace,"pending.jsonl");});
  test("TRACE-PENDING-EACH-PHASE-AT-EOF",[]{for(uint32_t p=0;p<4;++p){Fixture f;f.load();f.submit(static_cast<InputPhase>(p),9*second);f.poll();f.audio.advance(8*second);check(f.session.update()==Error::Ok);check(f.session.state()==SessionState::Finished);f.end();check(f.trace.summary().complete && f.trace.summary().pending==1 && !f.trace.summary().cleared);if(p==3)save(f.trace,"eof-pending.jsonl");}});
  test("TRACE-STALE-AND-MAPPED-RANGE",[]{Fixture f;f.load();f.submit(InputPhase::Down,-1);f.poll();check(f.session.update()==Error::StaleInput);f.audio.sampled=true;f.audio.sampleHost=20*second;f.audio.sampleSong=0;f.submit(InputPhase::Up,second);f.poll();check(f.session.update()==Error::StaleInput);check(reasons(f.trace,TraceReason::BeforeEpoch)==1 && reasons(f.trace,TraceReason::MappedOutOfRange)==1);f.session.pause();f.end();check(f.trace.summary().complete);});
  test("TRACE-INVALID-INPUT-AND-STATE",[]{Fixture f;f.load();for(auto raw:std::array<RawInputEvent,4>{{{1,static_cast<InputPhase>(99),{},10*second},{2,InputPhase::Down,{std::numeric_limits<float>::quiet_NaN(),.5f},10*second},{3,InputPhase::Move,{.5f,std::numeric_limits<float>::infinity()},10*second},{4,InputPhase::Cancel,{},-1}}})check(f.session.push(raw)==Error::InvalidArgument);{InputTrace::Writer writer(f.trace);f.platform.rejectInvalid({5,static_cast<InputPhase>(4),{},10*second},10*second,f.trace.batch(),0,true);}f.session.pause();check(f.session.push({})==Error::InvalidState);f.end();check(f.trace.summary().complete && f.trace.summary().rejected==6);save(f.trace,"invalid-input.jsonl");});
  test("TRACE-QUEUE-CAPACITIES",[]{Fixture f;f.load();for(size_t n=0;n<2049;++n)f.submit(InputPhase::Move);f.poll();check(f.session.push({1,InputPhase::Up,{},11*second})==Error::QueueFull);f.session.pause();f.end();const auto& s=f.trace.summary();check(s.complete && s.ingress==2050 && s.rejected==2 && s.cleared==2048 && s.platformQueueDropped==1 && s.sessionQueueDropped==1);save(f.trace,"queues.jsonl");});
  test("TRACE-FAILED-CONTROL-CLEAR-AND-INACTIVE-DRAIN",[]{Fixture f;f.load();f.submit(InputPhase::Down);{InputTrace::Scope scope(&f.trace,TraceOperation::Start,monotonicNow());auto result=f.session.start(10*second);check(result==Error::InvalidState);f.platform.clear();f.session.finishTrace(scope,result);}f.session.pause();for(uint32_t p=0;p<4;++p)f.submit(static_cast<InputPhase>(p));f.poll();f.end();check(f.trace.summary().complete && f.trace.summary().cleared==5);check(reasons(f.trace,TraceReason::LifecycleClear)==1 && reasons(f.trace,TraceReason::InactiveDrain)==4);save(f.trace,"clears.jsonl");});
  test("TRACE-CLEAR-SNAPSHOT-BOUNDARY",[]{FixedQueue<int,4> q;q.push(1);int observed=0;q.clearObserved([&](int i){observed=i;check(q.push(2));});int actual=0;check(observed==1 && q.pop(actual) && actual==2 && !q.pop(actual));});
  test("TRACE-EPOCHS-AND-POINTER-BOUNDARIES",[]{Fixture f;f.load();f.submit(InputPhase::Down,second,8);f.submit(InputPhase::Down,second,2);f.poll();f.audio.advance(second);f.session.update();f.session.pause();f.session.resume(20*second);f.submit(InputPhase::Up,second);f.poll();check(f.session.update()==Error::StaleInput);check(f.session.seek(2*second,30*second)==Error::Ok);check(f.trace.epoch()==3);f.session.pause();check(f.session.resume(-1)==Error::InvalidArgument);check(f.trace.epoch()==3);f.end();check(f.trace.summary().complete);bool found=false;for(size_t i=0;i<f.trace.recordCount(TraceSource::Session);++i){const auto& r=f.trace.record(TraceSource::Session,i);if(r.kind==TraceKind::Update && r.data.boundary.end && r.data.boundary.pointerCount==2){check(r.data.boundary.pointers[0]==2 && r.data.boundary.pointers[1]==8);found=true;}}check(found);save(f.trace,"epochs.jsonl");});
  test("TRACE-AUDIO-EXACT-QUERY-OBSERVATION",[]{Fixture f;f.load();f.audio.sampled=true;f.audio.sampleHost=11*second;f.audio.sampleSong=second;f.audio.advance(second);f.session.update();f.session.time();NullRenderBackend render;f.session.render(render,11*second);f.audio.sampled=false;f.session.update();f.session.pause();f.end();uint64_t positions=0,samples=0;bool failure=false;for(size_t i=0;i<f.trace.recordCount(TraceSource::Session);++i){const auto& r=f.trace.record(TraceSource::Session,i);if(r.kind==TraceKind::Clock){positions+=r.data.clock.query==TraceQuery::Position;samples+=r.data.clock.query==TraceQuery::ClockSample;if(r.data.clock.query==TraceQuery::ClockSample && !r.data.clock.success)failure=true;}}check(positions==f.audio.positions && samples==f.audio.samples && failure && f.trace.summary().complete);save(f.trace,"clocks.jsonl");});
  test("TRACE-PLATFORM-CAPACITY",[]{InputTrace t;check(t.beginCapture(header(),false)==CaptureError::Ok);{InputTrace::Writer writer(t);for(size_t n=0;n<InputTrace::capacity/2;++n){auto e=t.receive({},0,t.batch());t.platformEnqueue(e,false);}check(t.recordCount(TraceSource::Platform)==InputTrace::capacity);auto e=t.receive({},0,t.batch());t.platformEnqueue(e,false);}check(t.endCapture(false)==CaptureError::Ok);check(t.summary().platformDropped==2 && !t.summary().complete && t.summary().rejected==InputTrace::capacity/2+1);save(t,"platform-full.jsonl");});
  test("TRACE-SESSION-CAPACITY",[]{InputTrace t;check(t.beginCapture(header(),false)==CaptureError::Ok);{InputTrace::Scope scope(&t,TraceOperation::Status,0);for(size_t n=0;n<InputTrace::capacity-1;++n)t.clock(TraceQuery::Position,0,0,0,true);check(t.recordCount(TraceSource::Session)==InputTrace::capacity);t.clock(TraceQuery::Position,0,0,0,true);scope.finish(Error::Ok,{},0);}check(t.endCapture(false)==CaptureError::Ok);check(t.summary().sessionDropped==2 && !t.summary().complete);save(t,"session-full.jsonl");});
  test("TRACE-DIAGNOSTIC-FAILURE-PRESERVES-GAME",[]{std::vector<JudgmentResult> expected;std::vector<InputEvent> replay;Score score;for(int mode=0;mode<3;++mode){Fixture f(mode!=0);f.load();if(mode==2){InputTrace::Scope scope(&f.trace,TraceOperation::Status,0);for(size_t n=0;n<InputTrace::capacity;++n)f.trace.clock(TraceQuery::Position,0,0,0,true);scope.finish(Error::Ok,{},0);}for(uint32_t p=0;p<4;++p)f.submit(static_cast<InputPhase>(p));f.poll();f.audio.advance(2*second);check(f.session.update()==Error::Ok);f.session.pause();if(!mode){expected=f.session.judgments();replay=f.session.replay().events;score=f.session.score();}else{f.end();check(f.session.judgments()==expected && f.session.replay().events==replay && f.session.score().value==score.value && f.session.score().perfect==score.perfect && f.session.score().miss==score.miss);check(f.trace.summary().complete==(mode==1));}}});
  test("TRACE-EXPORT-FAILURE-RETRY-NO-CLOBBER",[]{Fixture f;f.load();check(f.trace.exportCapture((output/"playing.jsonl").string())==CaptureError::CaptureInvalidState);f.session.pause();f.end();check(f.trace.exportCapture((output/"missing"/"file").string())==CaptureError::CaptureExportFailed);save(f.trace,"retry.jsonl");const auto size=std::filesystem::file_size(output/"retry.jsonl");check(f.trace.exportCapture((output/"retry.jsonl").string())==CaptureError::CaptureExportFailed);check(std::filesystem::file_size(output/"retry.jsonl")==size);check(f.trace.summary().complete);});
  test("TRACE-FREEZE-WRITER-TIMEOUT-RETRY",[]{InputTrace t;check(t.beginCapture(header(),false)==CaptureError::Ok);std::atomic<bool> reserved{false},release{false};std::thread writer([&]{InputTraceTestAccess::reserveAndCommit(t,reserved,release);});while(!reserved)std::this_thread::yield();auto result=t.endCapture(false);release=true;writer.join();check(result==CaptureError::CaptureBusy && !t.frozen());check(t.endCapture(false)==CaptureError::Ok && t.frozen());check(!t.summary().complete);});
  test("TRACE-FREEZE-COMMIT-BEFORE-DEADLINE",[]{InputTrace t;check(t.beginCapture(header(),false)==CaptureError::Ok);std::atomic<bool> reserved{false},release{false};std::thread writer([&]{InputTraceTestAccess::reserveAndCommit(t,reserved,release);});while(!reserved)std::this_thread::yield();std::thread releaseThread([&]{while(t.active())std::this_thread::yield();release=true;});auto result=t.endCapture(false);writer.join();releaseThread.join();check(result==CaptureError::Ok && t.recordCount(TraceSource::Platform)==1);});
  test("TRACE-CONCURRENT-PRODUCERS-INCOMPLETE",[]{InputTrace t;check(t.beginCapture(header(),false)==CaptureError::Ok);{InputTrace::Writer first(t);InputTrace::Writer other(t);check(static_cast<bool>(first)&&static_cast<bool>(other));}check(t.endCapture(false)==CaptureError::Ok && !t.summary().complete);save(t,"concurrency.jsonl");});
  test("TRACE-CONCURRENT-STORAGE",[]{InputTrace t;check(t.beginCapture(header(),false)==CaptureError::Ok);std::atomic<unsigned> ready{0};std::atomic<bool> go{false};std::array<std::thread,4> writers;for(auto& writer:writers)writer=std::thread([&]{InputTrace::Writer lease(t);++ready;while(!go)std::this_thread::yield();for(unsigned n=0;n<1000;++n){auto e=t.receive({},0,t.batch());t.platformEnqueue(e,false);}});while(ready<4)std::this_thread::yield();go=true;for(auto& writer:writers)writer.join();check(t.endCapture(false)==CaptureError::Ok);check(!t.summary().complete && t.summary().ingress==4000 && t.summary().rejected==4000 && t.summary().platformDropped==0);check(t.recordCount(TraceSource::Platform)==8000);});
  test("TRACE-DESTROY-WAITS-FOR-WRITER",[]{auto t=std::make_unique<InputTrace>();check(t->beginCapture(header(),false)==CaptureError::Ok);auto* raw=t.get();std::atomic<bool> entered{false};std::thread writer([&]{InputTrace::Writer lease(*raw);entered=true;while(raw->active())std::this_thread::yield();});while(!entered)std::this_thread::yield();t.release();delete raw;writer.join();});
  test("TRACE-OBSERVES-DIAGNOSTIC-COMMANDS",[]{Fixture f;f.load();for(auto op:{TraceOperation::Measure,TraceOperation::Request120Hz,TraceOperation::UnknownCommand}){f.submit(InputPhase::Move);InputTrace::Scope scope(&f.trace,op,monotonicNow());f.platform.clear();f.session.finishTrace(scope,op==TraceOperation::UnknownCommand?Error::InvalidArgument:Error::Ok);}f.session.pause();f.end();check(f.trace.summary().complete && f.trace.summary().cleared==3);save(f.trace,"commands.jsonl");});
  test("TRACE-COUNTER-OVERFLOW",[]{InputTrace t;check(t.beginCapture(header(),false)==CaptureError::Ok);{InputTrace::Writer writer(t);InputTraceTestAccess::exhaustEvents(t);auto e=t.receive({},0);check(e.trace.event==0 && !t.active());}check(t.endCapture(false)==CaptureError::Ok && !t.summary().complete);save(t,"counter-overflow.jsonl");});
  test("TRACE-MISSING-REFERENCE-AND-BOUNDARY",[]{InputTrace t;check(t.beginCapture(header(),false)==CaptureError::Ok);{InputTrace::Scope unfinished(&t,TraceOperation::Start,0);InputEnvelope e{{},{1,0,0,1,0,0}};TraceInput i;i.event=e;i.stage=TraceStage::Terminal;i.disposition=TraceDisposition::Accepted;i.valid=MappedValue;i.sample=InputTrace::sessionHandle|60000;t.input(TraceSource::Session,i);}check(t.endCapture(false)==CaptureError::Ok && !t.summary().complete);save(t,"missing-reference.jsonl");});
  test("TRACE-RESET-EXCLUDES-OLD-EVENTS",[]{Fixture f;f.load();f.submit(InputPhase::Down);f.session.pause();f.end();check(f.trace.summary().pending==1);f.session.stop();check(f.trace.beginCapture(header(),false)==CaptureError::Ok);f.clear();f.end();check(f.trace.summary().complete && f.trace.summary().ingress==0);});
  test("TRACE-REPLAY-FULL-INTERNAL-PAUSE",[]{Fixture f;f.load();for(size_t n=0;n<maxEvents;++n){check(f.session.push({1,InputPhase::Move,{},10*second})==Error::Ok);check(f.session.update()==Error::Ok);}f.session.push({1,InputPhase::Up,{},10*second});f.session.push({1,InputPhase::Cancel,{},10*second});check(f.session.update()==Error::ReplayFull);f.end();check(f.session.state()==SessionState::Paused && f.trace.summary().accepted==maxEvents && f.trace.summary().rejected==1 && f.trace.summary().cleared==1 && !f.trace.summary().pending);});
  test("TRACE-REPLAY-INVESTIGATION-PAIRS",[]{
    for(int scenario=0;scenario<5;++scenario)for(int control=0;control<2;++control){
      Fixture f;f.load();
      if(scenario==0){InputTrace::Writer writer(f.trace);auto batch=f.trace.batch();for(uint32_t p=0;p<2;++p){auto id=control?2-p:p+1;auto t=(id==1?1020:1010)*ms;check(f.platform.submit({id,InputPhase::Down,{.5f,.8f},10*second+t},12*second,batch,p,true));} }
      if(scenario==1){if(!control){f.audio.advance(2*second);f.session.update();}f.submit(InputPhase::Down);}
      if(scenario==2){f.submit(InputPhase::Down);if(!control)f.submit(InputPhase::Down);}
      if(scenario==3){f.submit(InputPhase::Down);f.audio.sampled=true;f.audio.sampleHost=11*second;f.audio.sampleSong=control?second:500*ms;}
      if(scenario==4){f.submit(InputPhase::Move,control?500*ms:3*second);f.submit(InputPhase::Down);}
      f.poll();if(scenario!=1||control)f.audio.advance(2*second);f.session.update();if(scenario==0)f.session.time();f.session.pause();f.end();check(f.trace.summary().complete);
      const auto name="investigation-"+std::to_string(scenario)+(control?"-control.jsonl":"-original.jsonl");save(f.trace,name.c_str());
    }
  });
  std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
