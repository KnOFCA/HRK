#include "games/reference/reference.h"
#include "platform/headless/headless.h"
#include "kernel/gameplay/session.h"
#include "kernel/resource/resource.h"
#include "kernel/audio/audio.h"
#include "kernel/diagnostics/frames.h"
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
std::atomic<uint64_t> allocationCount{0};
void* operator new(std::size_t n){if(void* p=std::malloc(n?n:1)){++allocationCount;return p;}throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
using namespace hrk;
using namespace hrk::reference;
namespace {
int passed=0,failed=0;
void check(bool v){if(!v)throw std::runtime_error("assertion failed");}
void test(const char* id,const std::function<void()>& f){try{f();++passed;std::cout<<id<<" PASS\n";}catch(const std::exception& e){++failed;std::cout<<id<<" FAIL "<<e.what()<<'\n';}}
void failure(Error e,const std::function<void()>& f){try{f();}catch(const Failure& actual){check(actual.error==e);return;}throw std::runtime_error("expected error");}
std::vector<uint8_t> data(const std::string& p){return NativeFileSystem().read(std::string(HRK_DATA_DIR)+"/"+p);}
std::string text(const std::string& p){auto b=data(p);return {b.begin(),b.end()};}
Pcm pcm(){Pcm p;p.samples.resize(48000*2*8);return p;}
struct Fixture{
 ReferenceGame game;FakeAudioBackend audio;GameplaySession session{game,audio};
 std::shared_ptr<const IRuntimeChart> chart;
 Fixture(const char* file="single_note"){chart=game.createChartLoader()->load(text(std::string("charts/")+file+".json"));}
 void load(RuntimeMode mode=RuntimeMode::Play,const Replay* r=nullptr){check(session.load(chart,pcm(),mode,r)==Error::Ok);check(session.start(10*second)==Error::Ok);}
 void touch(TimeNs t,float x=.5f,InputPhase phase=InputPhase::Down){check(session.push({1,phase,{x,.8f},10*second+t})==Error::Ok);}
 void advance(TimeNs t){audio.advance(t);check(session.update()==Error::Ok);}
};
std::vector<JudgmentResult> run(const std::vector<TimeNs>& steps,bool render=true){
 Fixture f("simple_sequence");auto replay=deserializeReplay(data("replay/mixed.replay"));f.load(RuntimeMode::Replay,&replay);NullRenderBackend out;size_t frame=0;
 while(f.session.state()==SessionState::Playing){f.advance(steps[frame++%steps.size()]);if(render)f.session.render(out,static_cast<TimeNs>(frame)*16*ms);}
 check(f.session.score().value==300 && f.session.score().perfect==3 && f.session.score().miss==1);
 return f.session.judgments();
}
void judge(TimeNs t,float x,bool perfect){Fixture f;f.load();f.touch(t,x);f.advance(2*second);check(f.session.judgments().size()==1);const auto& j=f.session.judgments()[0];check(j.type==(perfect?Perfect:Miss));check(j.error==(perfect?t-second:window+1));}
}
int main(){
 test("TC-BASE-001",[]{check(second+500*ms==1500000000 && second-500*ms==500000000);});
 test("TC-BASE-002",[]{check(seconds(1500*ms)==1.5 && milliseconds(1500*ms)==1500);});
 test("TC-BASE-003",[]{EntityIds ids;std::set<EntityId> unique;for(int i=0;i<10000;++i)unique.insert(ids.next());check(unique.size()==10000);});
 test("TC-TIME-001",[]{FakeAudioBackend a;SongClock c(a);check(c.state()==ClockState::Stopped && c.now()==0);});
 test("TC-TIME-002",[]{Fixture f;f.load();f.advance(100*ms);check(f.session.time()==100*ms);});
 test("TC-TIME-003",[]{Fixture f;f.load();f.advance(second);check(f.session.pause()==Error::Ok);f.audio.advance(500*ms);check(f.session.time()==second);});
 test("TC-TIME-004",[]{Fixture f;f.load();f.advance(second);f.session.pause();f.audio.advance(500*ms);check(f.session.resume(11500*ms)==Error::Ok);f.advance(200*ms);check(f.session.time()==1200*ms);});
 test("TC-TIME-005",[]{Fixture f;f.load();check(f.session.seek(5*second,20*second)==Error::Ok);f.advance(100*ms);check(f.session.time()==5100*ms);});
 test("TC-TIME-006",[]{FakeAudioBackend a;a.load(pcm());SongClock c(a);a.start();c.start();a.advance(second);check(c.now()==second);a.seek(500*ms);check(c.now()==second);c.seek(500*ms);check(c.now()==500*ms);});
 test("TC-TIME-007",[]{ClockMapper m;m.anchor(10*second,4*second);TimeNs t;check(m.map(10025*ms,t)==Error::Ok && t==4025*ms);check(m.map(9*second,t)==Error::StaleInput);});
 test("TC-TIME-008",[]{Fixture f;f.load();f.advance(second);f.touch(1005*ms);f.advance(16*ms);check(f.session.judgments()[0].inputTime==1005*ms);});
 test("TC-TL-001",[]{TimingMap m({{0,120}});for(int i=0;i<5;++i)check(m.beatToTime(i)==i*500*ms);});
 test("TC-TL-002",[]{TimingMap m({{0,120}});check(m.timeToBeat(250*ms)==.5 && m.timeToBeat(1250*ms)==2.5);});
 test("TC-TL-003",[]{TimingMap m({{0,120},{4,240}});check(m.beatToTime(4)==2000*ms && m.beatToTime(5)==2250*ms && m.beatToTime(6)==2500*ms);});
 test("TC-TL-004",[]{TimingMap m({{0,120},{4,240}});for(double b:{0.,.25,1.,3.5,4.,5.25,16.})check(std::abs(m.timeToBeat(m.beatToTime(b))-b)<1e-7);});
 test("TC-TL-005",[]{failure(Error::InvalidArgument,[]{TimingMap m({});});});
 test("TC-CHART-001",[]{ReferenceGame g;auto c=g.createChartLoader()->load(R"({"bpm":120,"notes":[{"beat":1,"x":0.25},{"beat":2,"x":0.5}]})");auto& r=dynamic_cast<const ReferenceRuntimeChart&>(*c);check(r.size()==2 && r.notes()[0].time==500*ms && r.notes()[1].time==second);});
 test("TC-CHART-002",[]{ReferenceGame g;failure(Error::ChartParseError,[&]{g.createChartLoader()->load(R"({"bpm":0,"notes":[]})");});});
 test("TC-CHART-003",[]{ReferenceGame g;failure(Error::ChartParseError,[&]{g.createChartLoader()->load(R"({"bpm":120,"notes":[{"beat":1,"x":1.5}]})");});});
 test("TC-CHART-004",[]{Fixture f;auto hash=f.chart->hash();f.load();f.advance(2*second);check(f.chart->hash()==hash && f.chart->size()==1);static_assert(std::is_const_v<std::remove_reference_t<decltype(dynamic_cast<const ReferenceRuntimeChart&>(*f.chart).notes())>>);});
 test("TC-IN-001",[]{Fixture f;f.load();f.touch(second);f.advance(second);auto e=f.session.replay().events[0];check(e.pointerId==1 && e.phase==InputPhase::Down && e.position.x==.5f && e.songTime==second);});
 test("TC-IN-002",[]{Fixture f;f.load();for(int i=0;i<4;++i)f.touch((1000+i*10)*ms,.5f,i==0?InputPhase::Down:i==3?InputPhase::Up:InputPhase::Move);f.advance(1100*ms);auto& e=f.session.replay().events;check(e.size()==4);for(int i=0;i<4;++i)check(e[i].songTime==(1000+i*10)*ms);});
 test("TC-IN-003",[]{InputQueue q;q.push({3,InputPhase::Down,{.5f,.5f},0});q.push({7,InputPhase::Down,{.5f,.5f},1});RawInputEvent e;check(q.pop(e)&&e.pointerId==3);check(q.pop(e)&&e.pointerId==7);});
 test("TC-IN-004",[]{PointerState p;p.apply({1,InputPhase::Down,{},0});p.apply({2,InputPhase::Down,{},0});p.apply({1,InputPhase::Move,{},0});p.apply({2,InputPhase::Up,{},0});check(p.active(1)&&!p.active(2));p.apply({1,InputPhase::Up,{},0});check(!p.active(1));});
 test("TC-IN-005",[]{PointerState p;p.apply({1,InputPhase::Down,{},0});p.apply({1,InputPhase::Move,{},0});p.apply({1,InputPhase::Cancel,{},0});check(!p.active(1));});
 test("TC-IN-006",[]{FixedQueue<int,4> q;for(int i=0;i<4;++i)check(q.push(i));check(!q.push(4));for(int i=0;i<4;++i){int v;check(q.pop(v)&&v==i);}int v;check(!q.pop(v));});
 test("TC-IN-007",[]{InputQueue q;for(int i=0;i<2048;++i)check(q.push({static_cast<uint32_t>(i),InputPhase::Move,{},i})==Error::Ok);check(q.push({})==Error::QueueFull && q.overflow()==1);for(int i=0;i<2048;++i){RawInputEvent e;check(q.pop(e)&&e.pointerId==static_cast<uint32_t>(i));}});
 test("TC-JUDGE-001",[]{judge(1000*ms,.5f,true);});test("TC-JUDGE-002",[]{judge(950*ms,.5f,true);});test("TC-JUDGE-003",[]{judge(1050*ms,.5f,true);});
 test("TC-JUDGE-004",[]{judge(949*ms,.5f,false);});test("TC-JUDGE-005",[]{judge(1051*ms,.5f,false);});test("TC-JUDGE-006",[]{judge(1000*ms,.8f,false);});
 test("TC-JUDGE-007",[]{Fixture f;f.load();f.touch(second);f.touch(1010*ms);f.advance(2*second);check(f.session.judgments().size()==1);});
 test("TC-JUDGE-008",[]{Fixture f;f.load();f.advance(1100*ms);check(f.session.judgments().size()==1 && f.session.judgments()[0].type==Miss);});
 test("TC-JUDGE-009",[]{Fixture f;f.load();f.advance(2*second);f.advance(second);check(f.session.judgments().size()==1);});
 test("TC-SCORE-001",[]{Fixture f;f.load();f.touch(second);f.advance(second);auto s=f.session.score();check(s.value==100 && s.perfect==1 && s.miss==0);});
 test("TC-SCORE-002",[]{check(run({16*ms}).size()==4);});
 test("TC-SCORE-003",[]{ReferenceGame g;auto s=g.createScoreSystem();for(int i=0;i<5;++i)s->add({1,Perfect,0,0,0});check(s->score().value==500);s->reset();check(s->score().value==0 && s->score().perfect==0 && s->score().miss==0);});
 test("TC-GAME-001",[]{Fixture f;check(f.session.state()==SessionState::Created);check(f.session.load(f.chart,pcm(),RuntimeMode::AutoPlay)==Error::Ok);check(f.session.state()==SessionState::Ready);check(f.session.start(0)==Error::Ok);f.advance(8*second);check(f.session.state()==SessionState::Finished);});
 test("TC-GAME-002",[]{Fixture f;f.load();f.advance(900*ms);f.session.pause();f.audio.advance(second);check(f.session.update()==Error::InvalidState && f.session.judgments().empty());f.session.resume(11900*ms);f.advance(200*ms);check(f.session.score().miss==1);});
 test("TC-GAME-003",[]{Fixture f;f.load();f.session.stop();check(f.session.state()==SessionState::Finished && f.session.push({})==Error::InvalidState);f.audio.advance(second);check(f.audio.position()==0);});
 test("TC-GAME-004",[]{Fixture f;check(f.session.resume(0)==Error::InvalidState && f.session.start(0)==Error::InvalidState && f.session.seek(0,0)==Error::InvalidState);});
 test("TC-AUDIO-001",[]{auto p=decodeWav(data("audio/short_test.wav"));check(p.sampleRate==48000 && p.channels==2 && p.frames()==384000);});
 test("TC-AUDIO-002",[]{auto b=data("audio/short_test.wav");auto p=decodeWav(b);check(p.samples.size()==768000);b.pop_back();failure(Error::AudioInvalid,[&]{decodeWav(b);});});
 test("TC-AUDIO-003",[]{auto p=decodeWav(data("audio/short_test.wav"));FakeAudioBackend a;a.load(p);check(a.seek(second)==Error::Ok && a.position()==second);check(a.seek(9*second)==Error::InvalidArgument);});
 test("TC-AUDIO-004",[]{FakeAudioBackend a;a.load(pcm());a.start();a.advance(500*ms);check(a.position()==500*ms);});
 test("TC-AUDIO-005",[]{FakeAudioBackend a;a.load(pcm());a.start();a.advance(500*ms);a.pause();a.advance(500*ms);check(a.position()==500*ms);});
 test("TC-AUDIO-006",[]{FakeAudioBackend a;a.load(pcm());a.start();a.advance(500*ms);a.pause();a.advance(500*ms);a.resume();a.advance(250*ms);check(a.position()==750*ms);});
 test("TC-AUDIO-007",[]{Fixture f;f.load();f.audio.advance(1234*ms);check(f.session.time()==1234*ms);});
 test("TC-REP-001",[]{Fixture f;f.load();f.touch(950*ms);f.touch(970*ms,.5f,InputPhase::Up);f.touch(1500*ms);f.touch(1520*ms,.5f,InputPhase::Up);f.advance(2*second);auto r=deserializeReplay(serializeReplay(f.session.replay()));check(r.events==f.session.replay().events && r.events.size()==4);});
 test("TC-REP-002",[]{auto r=deserializeReplay(data("replay/mixed.replay"));check(r.header.formatVersion==1 && r.header.gameId==1 && r.header.ruleVersion==1 && r.header.chartHash==hashBytes(text("charts/simple_sequence.json")));});
 test("TC-REP-003",[]{auto b=data("replay/mixed.replay");check(serializeReplay(deserializeReplay(b))==b);});
 test("TC-REP-004",[]{auto b=data("replay/mixed.replay");b[4]=2;failure(Error::UnsupportedVersion,[&]{deserializeReplay(b);});});
 test("TC-REP-005",[]{auto r=deserializeReplay(data("replay/mixed.replay"));failure(Error::ReplayChartMismatch,[&]{validateReplay(r,0,1,1);});});
 test("TC-DET-001",[]{auto r=run({16*ms});check(r.size()==4 && r[0].error==0 && r[1].error==20*ms && r[2].type==Miss && r[3].error==-40*ms);});
 test("TC-DET-002",[]{auto expected=run({16*ms});for(int i=0;i<100;++i)check(run({16*ms})==expected);});
 test("TC-DET-003",[]{auto expected=run({16*ms});for(int fps:{30,60,90,120,144})check(run({second/fps})==expected);});
 test("TC-DET-004",[]{check(run({16*ms,16*ms,33*ms,8*ms,20*ms,12*ms})==run({second/60}));});
 test("TC-DET-005",[]{check(run({16*ms},false)==run({16*ms},true));});
 test("TC-AUTO-001",[]{Fixture f("simple_sequence");f.load(RuntimeMode::AutoPlay);f.advance(3*second);check(f.session.score().perfect==4 && f.session.score().miss==0);});
 test("TC-REN-001",[]{NullRenderBackend out;out.sprite({{}, {}, {},0});check(out.sprites==1);});
 test("TC-REN-003",[]{check(run({33*ms},false)==run({8*ms},true));});
 test("TC-HEAD-001",[]{check(run({16*ms}).size()==4);});
 test("TC-HEAD-002",[]{check(run({16*ms},false).size()==4);});
 test("TC-HEAD-003",[]{check(run({16*ms},false).size()==4);});
 test("TC-RES-001",[]{check(!data("charts/single_note.json").empty());});
 test("TC-RES-002",[]{failure(Error::ResourceNotFound,[]{data("not-present");});});
 test("TC-DIAG-001",[]{Fixture f;f.load();NullRenderBackend out;f.session.render(out,second);f.session.render(out,second+second/60);check(f.session.diagnostics().frames==2 && std::abs(f.session.diagnostics().fps-60)<.01);});
 test("TC-DIAG-002",[]{Fixture f;f.load();f.touch(second);f.advance(second);check(f.session.diagnostics().inputs==1);});
 test("TC-DIAG-003",[]{Fixture f;f.load();f.advance(2*second);check(f.session.diagnostics().judgments==f.session.score().perfect+f.session.score().miss);});
 test("TC-STAB-001",[]{Fixture f;for(int i=0;i<100;++i){f.load();f.session.stop();}});
 test("TC-STAB-002",[]{Fixture f;f.load();for(int i=0;i<100;++i){check(f.session.pause()==Error::Ok);check(f.session.resume(10*second)==Error::Ok);}});
 test("TC-STAB-003",[]{Fixture f;f.load();for(int i=0;i<100;++i){auto t=(i*7919%7000)*ms;check(f.session.seek(t,10*second)==Error::Ok && f.session.time()==t);}});
 test("TC-PERF-003",[]{std::ostringstream s;s<<"{\"bpm\":120,\"notes\":[";for(int i=0;i<1000;++i){if(i)s<<',';s<<"{\"beat\":"<<i*.01<<",\"x\":0.5}";}s<<"]}";Fixture f;f.chart=f.game.createChartLoader()->load(s.str());auto begin=monotonicNow();f.load(RuntimeMode::AutoPlay);f.advance(8*second);check(f.session.score().perfect==1000 && monotonicNow()-begin<second);});
 test("TC-ERR-001",[]{ReferenceGame g;for(auto s:{"{",R"({"bpm":120,"notes":[],"bpm":120})",R"({"bpm":120,"notes":[],"unknown":0})",R"({"bpm":01,"notes":[]})",R"({"bpm":120,"notes":[],})"})failure(Error::ChartParseError,[&]{g.createChartLoader()->load(s);});});
 test("TC-ERR-002",[]{Fixture f;check(f.session.load(f.chart,Pcm{},RuntimeMode::Play)!=Error::Ok && f.session.start(0)==Error::InvalidState);});
 test("TC-ERR-003",[]{Fixture f;f.audio.fail=true;check(f.session.load(f.chart,pcm(),RuntimeMode::Play)==Error::BackendFailure && f.session.state()==SessionState::Created);});
 test("REG-REPLAY-CORRUPTION",[]{auto b=data("replay/mixed.replay");for(size_t n=0;n<b.size();++n){auto bad=b;bad.resize(n);failure(Error::ReplayInvalid,[&]{deserializeReplay(bad);});}auto bad=b;bad.push_back(0);failure(Error::ReplayInvalid,[&]{deserializeReplay(bad);});bad=b;bad[44]=255;failure(Error::ReplayInvalid,[&]{deserializeReplay(bad);});});
 test("REG-REPLAY-COMPATIBILITY",[]{auto r=deserializeReplay(data("replay/mixed.replay"));failure(Error::ReplayIncompatible,[&]{validateReplay(r,r.header.chartHash,2,1);});failure(Error::ReplayIncompatible,[&]{validateReplay(r,r.header.chartHash,1,2);});});
 test("REG-LATE-INPUT",[]{Fixture f;f.load();f.advance(1100*ms);f.touch(second);f.audio.advance(ms);check(f.session.update()==Error::LateInput && f.session.score().miss==1 && f.session.replay().events.empty());});
 test("REG-SEEK-REBUILD",[]{Fixture f("simple_sequence");auto r=deserializeReplay(data("replay/mixed.replay"));f.load(RuntimeMode::Replay,&r);f.advance(3*second);check(f.session.seek(1200*ms,20*second)==Error::Ok && f.session.score().value==200);f.advance(2*second);check(f.session.judgments()==run({16*ms}));});
 test("REG-PLAY-RECORD-REPLAY",[]{Fixture f;f.load();f.touch(second);f.advance(2*second);auto r=deserializeReplay(serializeReplay(f.session.replay()));Fixture playback;playback.load(RuntimeMode::Replay,&r);playback.advance(2*second);check(playback.session.judgments()==f.session.judgments());});
 test("REG-PLAY-SEEK-BRANCH",[]{Fixture f;f.load();f.touch(second);f.advance(2*second);check(f.session.seek(500*ms,20*second)==Error::Ok);f.audio.advance(2*second);check(f.session.update()==Error::Ok && f.session.score().miss==1);auto r=f.session.replay();Fixture playback;playback.load(RuntimeMode::Replay,&r);playback.advance(2500*ms);check(playback.session.judgments()==f.session.judgments());});
 test("REG-EMPTY-CHART",[]{Fixture f("empty_chart");f.load(RuntimeMode::AutoPlay);f.advance(8*second);check(f.session.judgments().empty() && f.session.state()==SessionState::Finished);});

 test("REG-CLOCK-DEVICE-PAIR",[]{ClockMapper m;m.anchor(10*second,0);m.synchronize(11*second,900*ms);TimeNs song=0;check(m.map(10500*ms,song)==Error::Ok && song==400*ms);check(m.map(9999*ms,song)==Error::StaleInput);});
 test("REG-QUEUE-CONCURRENCY",[]{FixedQueue<uint32_t,2048> q;std::thread producer([&]{for(uint32_t i=0;i<100000;++i)while(!q.push(i))std::this_thread::yield();});bool ordered=true;for(uint32_t i=0;i<100000;++i){uint32_t v;while(!q.pop(v))std::this_thread::yield();if(v!=i)ordered=false;}producer.join();check(ordered);});
 test("TC-RT-002",[]{Fixture f;f.load();const auto count=allocationCount.load();for(int batch=0;batch<10;++batch){for(int i=0;i<1000;++i)check(f.session.push({1,InputPhase::Move,{.5f,.5f},10*second+batch*ms})==Error::Ok);f.audio.advance(ms);check(f.session.update()==Error::Ok);}check(allocationCount.load()==count);});
#ifdef _WIN32
 auto resident=[](){PROCESS_MEMORY_COUNTERS_EX memory{};check(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))!=0);return memory.PrivateUsage;};
 test("TC-MEM-001",[&]{for(int i=0;i<5;++i){Fixture f;f.load();f.advance(2*second);}auto baseline=resident();for(int i=0;i<100;++i){Fixture f;f.load();f.advance(2*second);}auto end=resident();std::cout<<"Memory session baseline="<<baseline<<" end="<<end<<'\n';check(end<=baseline+8*1024*1024);});
 test("TC-MEM-002",[&]{auto bytes=data("replay/mixed.replay");for(int i=0;i<5;++i){auto r=deserializeReplay(bytes);check(r.events.size()==3);}auto baseline=resident();for(int i=0;i<100;++i){auto r=deserializeReplay(bytes);check(r.events.size()==3);}auto end=resident();std::cout<<"Memory replay baseline="<<baseline<<" end="<<end<<'\n';check(end<=baseline+8*1024*1024);});
#endif
 test("REG-FRAME-CAPTURE",[]{FrameCapture capture;capture.start();for(int i=0;i<=3601;++i)capture.observe(second+static_cast<TimeNs>(i)*second/60);check(capture.complete() && capture.p95ms()<17 && capture.lowestFps()>=59);});

 test("REG-DEVICE-DELIVERY-GRACE",[]{
  class DelayedAudio : public FakeAudioBackend {public: TimeNs inputDeliveryGrace() const override{return 40*ms;}} audio;
  ReferenceGame game;auto chart=game.createChartLoader()->load(text("charts/single_note.json"));GameplaySession session(game,audio);
  check(session.load(chart,pcm(),RuntimeMode::Play)==Error::Ok);check(session.start(10*second)==Error::Ok);
  audio.advance(1016*ms);check(session.update()==Error::Ok);
  check(session.push({7,InputPhase::Down,{.5f,.8f},11*second})==Error::Ok);
  audio.advance(16*ms);check(session.update()==Error::Ok);check(session.score().perfect==1 && session.judgments()[0].error==0);
  auto r=session.replay();Fixture f;f.load(RuntimeMode::Replay,&r);f.advance(2*second);check(f.session.judgments()==session.judgments());
 });
 std::cout<<"TOTAL "<<passed<<" PASS "<<failed<<" FAIL\n";return failed?1:0;
}
