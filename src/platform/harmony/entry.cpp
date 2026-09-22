#include "platform/harmony/backend.h"
#include "app/composition.h"
#include "kernel/diagnostics/frames.h"
#include <napi/native_api.h>
#include <mutex>
#include <cstring>
#include <sstream>
namespace {
using namespace hrk;using namespace hrk::harmony;
struct ControlObservation {
 GameplaySession& session;InputTrace::Scope scope;Error result=Error::Ok;
 ControlObservation(GameplaySession& s,TraceOperation op,TimeNs target=0):session(s),scope(s.trace(),op,monotonicNow(),target){}
 ~ControlObservation(){session.finishTrace(scope,result);}
};
struct State {
 std::mutex control;OHAudioBackend audio;HarmonyInputBackend input;GlesRenderBackend render;
 FrameCapture capture;HarmonyFrameScheduler scheduler;HarmonyLifecycle lifecycle;Composition app{audio};
 uint64_t surfaceCreates=0,surfaceDestroys=0,surfaceResizes=0;OH_NativeXComponent* component=nullptr;Error lastError=Error::Ok;std::atomic<uint64_t> skippedFrames{0};
 State(){input.setTrace(&app.inputTrace());lifecycle.setBackgroundCallback([this]{auto& s=app.session();ControlObservation observe(s,TraceOperation::Background);if(s.state()==SessionState::Playing)s.pause();input.clear();});}
};
State& state(){static State value;return value;}
void frame(OH_NativeXComponent*,uint64_t timestamp,uint64_t){
 auto& v=state();std::unique_lock<std::mutex> guard(v.control,std::try_to_lock);if(!guard.owns_lock()){++v.skippedFrames;return;}
 auto& s=v.app.session();v.capture.observe(static_cast<TimeNs>(timestamp));
 if(v.audio.needsPause())v.lifecycle.background();
 for(;;){InputEnvelope event;if(!v.input.pollTraced(event))break;InputTrace::Transfer action(&v.app.inputTrace());v.app.inputTrace().platformPoll(event);if(s.state()==SessionState::Playing){auto error=s.pushTraced(event);if(error!=Error::Ok && error!=Error::InvalidState)v.lastError=error;}else v.app.inputTrace().cleared(event,TraceReason::InactiveDrain);}
 if(s.state()==SessionState::Playing){auto error=s.update();if(error!=Error::Ok)v.lastError=error;}
 s.render(v.render,static_cast<TimeNs>(timestamp));
}
void created(OH_NativeXComponent* component,void* window){
 auto& v=state();std::lock_guard<std::mutex> guard(v.control);uint64_t w=0,h=0;
 ControlObservation observe(v.app.session(),TraceOperation::SurfaceCreate);
 v.component=component;
 if(OH_NativeXComponent_GetXComponentSize(component,window,&w,&h)!=OH_NATIVEXCOMPONENT_RESULT_SUCCESS || !v.render.create(window,static_cast<uint32_t>(w),static_cast<uint32_t>(h))){v.lastError=Error::BackendFailure;observe.result=v.lastError;return;}
 ++v.surfaceCreates;OH_NativeXComponent_ExpectedRateRange range{60,60,60};
 if(OH_NativeXComponent_SetExpectedFrameRateRange(component,&range)!=OH_NATIVEXCOMPONENT_RESULT_SUCCESS || OH_NativeXComponent_RegisterOnFrameCallback(component,frame)!=OH_NATIVEXCOMPONENT_RESULT_SUCCESS){v.lastError=Error::BackendFailure;observe.result=v.lastError;}
}
void changed(OH_NativeXComponent* component,void* window){auto& v=state();std::lock_guard<std::mutex> guard(v.control);ControlObservation observe(v.app.session(),TraceOperation::SurfaceResize);++v.surfaceResizes;uint64_t w=0,h=0;if(OH_NativeXComponent_GetXComponentSize(component,window,&w,&h)==OH_NATIVEXCOMPONENT_RESULT_SUCCESS)v.render.resize(static_cast<uint32_t>(w),static_cast<uint32_t>(h));}
void destroyed(OH_NativeXComponent* component,void*){OH_NativeXComponent_UnregisterOnFrameCallback(component);auto& v=state();std::lock_guard<std::mutex> guard(v.control);ControlObservation observe(v.app.session(),TraceOperation::SurfaceDestroy);v.lifecycle.background();v.render.destroy();v.component=nullptr;++v.surfaceDestroys;}
void touched(OH_NativeXComponent* c,void* w){state().input.touch(c,w);}
napi_value string(napi_env env,const std::string& text){napi_value v=nullptr;napi_create_string_utf8(env,text.c_str(),text.size(),&v);return v;}
std::string readString(napi_env env,napi_value v,size_t limit=16*1024*1024){size_t n=0;require(napi_get_value_string_utf8(env,v,nullptr,0,&n)==napi_ok && n<=limit,Error::InvalidArgument);std::string s(n+1,'\0');require(napi_get_value_string_utf8(env,v,s.data(),s.size(),&n)==napi_ok,Error::InvalidArgument);s.resize(n);return s;}
std::vector<uint8_t> readBuffer(napi_env env,napi_value v){void* p=nullptr;size_t n=0;require(napi_get_arraybuffer_info(env,v,&p,&n)==napi_ok && n<=128*1024*1024,Error::InvalidArgument);if(!n)return {};return {static_cast<uint8_t*>(p),static_cast<uint8_t*>(p)+n};}
napi_value initialize(napi_env env,napi_callback_info info){
 try{size_t count=4;napi_value args[4];require(napi_get_cb_info(env,info,&count,args,nullptr,nullptr)==napi_ok && count==4,Error::InvalidArgument);
  auto chart=readString(env,args[0]);auto bytes=readBuffer(env,args[1]);auto replay=readBuffer(env,args[3]);int32_t mode=0;require(napi_get_value_int32(env,args[2],&mode)==napi_ok && mode>=0 && mode<=2,Error::InvalidArgument);
  auto& v=state();std::lock_guard<std::mutex> guard(v.control);ControlObservation observe(v.app.session(),TraceOperation::Load);v.app.session().stop();v.input.clear();v.lastError=v.app.load(chart,bytes,static_cast<RuntimeMode>(mode),replay);observe.result=v.lastError;return string(env,errorName(v.lastError));
 }catch(const std::exception& e){return string(env,e.what());}
}
napi_value command(napi_env env,napi_callback_info info){
 try{size_t count=2;napi_value args[2];require(napi_get_cb_info(env,info,&count,args,nullptr,nullptr)==napi_ok && count>=1,Error::InvalidArgument);auto name=readString(env,args[0]);
  auto& v=state();std::lock_guard<std::mutex> guard(v.control);auto& s=v.app.session();Error result=Error::InvalidArgument;
  const auto operation=name=="start"?TraceOperation::Start:name=="pause"?TraceOperation::Pause:name=="resume"?TraceOperation::Resume:name=="seek"?TraceOperation::Seek:name=="stop"?TraceOperation::Stop:name=="background"?TraceOperation::Background:name=="foreground"?TraceOperation::Foreground:name=="measure"?TraceOperation::Measure:name=="120hz"?TraceOperation::Request120Hz:TraceOperation::UnknownCommand;
  TimeNs target=0;if(name=="seek" && count==2){double value=0;require(napi_get_value_double(env,args[1],&value)==napi_ok && std::isfinite(value) && value>=0 && value<=86400,Error::InvalidArgument);target=static_cast<TimeNs>(value*second);}
  ControlObservation observe(s,operation,target);
  if(name=="start")result=s.start(monotonicNow());else if(name=="pause")result=s.pause();else if(name=="resume")result=s.resume(monotonicNow());
  else if(name=="seek" && count==2)result=s.seek(target,monotonicNow());
  else if(name=="measure"){v.capture.start();result=Error::Ok;}
  else if(name=="stop"){s.stop();result=Error::Ok;}else if(name=="background"){v.lifecycle.background();result=Error::Ok;}
  else if(name=="120hz" && v.component){OH_NativeXComponent_ExpectedRateRange range{120,120,120};result=OH_NativeXComponent_SetExpectedFrameRateRange(v.component,&range)==OH_NATIVEXCOMPONENT_RESULT_SUCCESS?Error::Ok:Error::BackendFailure;}
  v.input.clear();v.lastError=result;observe.result=result;return string(env,errorName(result));
 }catch(const std::exception& e){return string(env,e.what());}
}
napi_value status(napi_env env,napi_callback_info){
 auto& v=state();std::lock_guard<std::mutex> guard(v.control);auto& s=v.app.session();auto score=s.score();auto d=s.diagnostics();
 std::ostringstream out;out.imbue(std::locale::classic());out<<"{\"time\":"<<seconds(s.time())<<",\"state\":"<<static_cast<int>(s.state())<<",\"score\":"<<score.value<<",\"perfect\":"<<score.perfect<<",\"miss\":"<<score.miss<<",\"fps\":"<<d.fps<<",\"frames\":"<<d.frames<<",\"inputs\":"<<d.inputs<<",\"rejected\":"<<d.rejectedInputs<<",\"maxInputDelayMs\":"<<milliseconds(d.maxInputDelay)<<",\"overflow\":"<<v.input.dropped()+s.overflowCount()<<",\"error\":\""<<errorName(v.lastError)<<"\"}";return string(env,out.str());
}
napi_value exportReplay(napi_env env,napi_callback_info){
 try{auto& v=state();std::lock_guard<std::mutex> guard(v.control);const auto bytes=serializeReplay(v.app.session().replay());void* buffer=nullptr;napi_value result=nullptr;require(napi_create_arraybuffer(env,bytes.size(),&buffer,&result)==napi_ok,Error::InvalidArgument);std::memcpy(buffer,bytes.data(),bytes.size());return result;
 }catch(const std::exception& e){napi_throw_error(env,nullptr,e.what());return nullptr;}
}
napi_value exportResult(napi_env env,napi_callback_info){
 auto& v=state();std::lock_guard<std::mutex> guard(v.control);auto& s=v.app.session();auto score=s.score();
 std::ostringstream out;out<<"{\"score\":"<<score.value<<",\"perfect\":"<<score.perfect<<",\"miss\":"<<score.miss<<",\"judgments\":[";
 bool first=true;for(const auto& j:s.judgments()){if(!first)out<<',';first=false;out<<"{\"id\":"<<j.entity<<",\"type\":"<<j.type<<",\"targetTime\":"<<j.targetTime<<",\"inputTime\":"<<j.inputTime<<",\"error\":"<<j.error<<'}';}out<<"]}";return string(env,out.str());
}
napi_value exportMetrics(napi_env env,napi_callback_info){
 auto& v=state();std::lock_guard<std::mutex> guard(v.control);std::ostringstream out;out.imbue(std::locale::classic());
 out<<"{\"complete\":"<<(v.capture.complete()?"true":"false")<<",\"durationSeconds\":"<<v.capture.elapsed()<<",\"intervals\":"<<v.capture.count()<<",\"p95Ms\":"<<v.capture.p95ms()<<",\"lowestFps\":"<<v.capture.lowestFps()<<",\"skippedFrames\":"<<v.skippedFrames.load()<<",\"surfaceCreates\":"<<v.surfaceCreates<<",\"surfaceDestroys\":"<<v.surfaceDestroys<<",\"surfaceResizes\":"<<v.surfaceResizes<<",\"width\":"<<v.render.width()<<",\"height\":"<<v.render.height()<<",\"maxGapMs\":"<<v.capture.maxGapMs()<<",\"fpsBuckets\":[";bool first=true;for(auto fps:v.capture.buckets()){if(!first)out<<',';first=false;out<<fps;}out<<"]}";return string(env,out.str());
}
// Metadata is supplied by the diagnostic caller before initialize/load. No device
// or revision identity is guessed here, and the requested refresh rate is not used.
napi_value beginCapture(napi_env env,napi_callback_info info){
 try{
  size_t count=1;napi_value args[1];require(napi_get_cb_info(env,info,&count,args,nullptr,nullptr)==napi_ok && count==1,Error::InvalidArgument);
  auto field=[&](const char* name){napi_value value=nullptr;require(napi_get_named_property(env,args[0],name,&value)==napi_ok,Error::InvalidArgument);return readString(env,value,256);};
  TraceHeader h;h.runId=field("runId");h.sourceRevision=field("sourceRevision");h.specRevision=field("specRevision");h.policyVersion=field("policyVersion");h.buildMode=field("buildMode");h.deviceModel=field("deviceModel");h.osVersion=field("osVersion");h.chartSha256=field("chartSha256");h.audioSha256=field("audioSha256");h.origin=field("origin");
  napi_value refresh=nullptr;napi_valuetype type=napi_undefined;require(napi_get_named_property(env,args[0],"actualRefreshHz",&refresh)==napi_ok && napi_typeof(env,refresh,&type)==napi_ok,Error::InvalidArgument);
  if(type!=napi_null)require(napi_get_value_double(env,refresh,&h.actualRefreshHz)==napi_ok && h.actualRefreshHz>0,Error::InvalidArgument);
  auto& v=state();std::lock_guard<std::mutex> guard(v.control);h.inputDeliveryGrace=v.audio.inputDeliveryGrace();return string(env,captureErrorName(v.app.beginCapture(h)));
 }catch(const std::exception&){return string(env,captureErrorName(CaptureError::CaptureInvalidState));}
}
napi_value endCapture(napi_env env,napi_callback_info){auto& v=state();std::lock_guard<std::mutex> guard(v.control);return string(env,captureErrorName(v.app.endCapture()));}
napi_value exportCapture(napi_env env,napi_callback_info info){
 try{size_t count=1;napi_value args[1];require(napi_get_cb_info(env,info,&count,args,nullptr,nullptr)==napi_ok && count==1,Error::InvalidArgument);const auto path=readString(env,args[0],4096);auto& v=state();std::lock_guard<std::mutex> guard(v.control);return string(env,captureErrorName(v.app.exportCapture(path)));}
 catch(const std::exception&){return string(env,captureErrorName(CaptureError::CaptureExportFailed));}
}
napi_value init(napi_env env,napi_value exports){
 napi_property_descriptor properties[]={{"initialize",nullptr,initialize,nullptr,nullptr,nullptr,napi_default,nullptr},{"command",nullptr,command,nullptr,nullptr,nullptr,napi_default,nullptr},{"status",nullptr,status,nullptr,nullptr,nullptr,napi_default,nullptr},{"exportReplay",nullptr,exportReplay,nullptr,nullptr,nullptr,napi_default,nullptr},{"exportResult",nullptr,exportResult,nullptr,nullptr,nullptr,napi_default,nullptr},{"exportMetrics",nullptr,exportMetrics,nullptr,nullptr,nullptr,napi_default,nullptr},{"beginCapture",nullptr,beginCapture,nullptr,nullptr,nullptr,napi_default,nullptr},{"endCapture",nullptr,endCapture,nullptr,nullptr,nullptr,napi_default,nullptr},{"exportCapture",nullptr,exportCapture,nullptr,nullptr,nullptr,napi_default,nullptr}};
 napi_define_properties(env,exports,9,properties);
 napi_value object=nullptr;OH_NativeXComponent* component=nullptr;
 if(napi_get_named_property(env,exports,OH_NATIVE_XCOMPONENT_OBJ,&object)==napi_ok && napi_unwrap(env,object,reinterpret_cast<void**>(&component))==napi_ok && component){
  static OH_NativeXComponent_Callback callbacks{created,changed,destroyed,touched};OH_NativeXComponent_RegisterCallback(component,&callbacks);
 }
 return exports;
}
}
static napi_module module={1,0,nullptr,init,"hrk_entry",nullptr,{nullptr,nullptr,nullptr,nullptr}};
extern "C" __attribute__((constructor)) void RegisterHRK(){napi_module_register(&module);}
