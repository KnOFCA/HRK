#include "app/composition.h"
#include "games/reference/reference.h"
#include "kernel/audio/audio.h"
namespace hrk {
Composition::Composition(IAudioBackend& audio):audio_(audio),game_(std::make_unique<reference::ReferenceGame>()),session_(std::make_unique<GameplaySession>(*game_,audio)){session_->setTrace(&trace_);}
CaptureError Composition::beginCapture(TraceHeader header){header.inputDeliveryGrace=audio_.inputDeliveryGrace();return trace_.beginCapture(header,session_->state()==SessionState::Playing || session_->state()==SessionState::Paused);}
Error Composition::load(const std::string& chart,const std::vector<uint8_t>& bytes,RuntimeMode mode,const std::vector<uint8_t>& replay){
 InputTrace::Scope scope(&trace_,TraceOperation::Load,trace_.active()?monotonicNow():0);
 try{auto c=game_->createChartLoader()->load(chart);auto pcm=decodeWav(bytes);Replay r;if(mode==RuntimeMode::Replay)r=deserializeReplay(replay);auto result=session_->load(c,pcm,mode,mode==RuntimeMode::Replay?&r:nullptr);session_->finishTrace(scope,result);return result;}catch(const Failure& e){session_->finishTrace(scope,e.error);return e.error;}
}
}
