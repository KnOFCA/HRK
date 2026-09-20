#include "app/composition.h"
#include "games/reference/reference.h"
#include "kernel/audio/audio.h"
namespace hrk {
Composition::Composition(IAudioBackend& audio):game_(std::make_unique<reference::ReferenceGame>()),session_(std::make_unique<GameplaySession>(*game_,audio)){}
Error Composition::load(const std::string& chart,const std::vector<uint8_t>& bytes,RuntimeMode mode,const std::vector<uint8_t>& replay){
 try{auto c=game_->createChartLoader()->load(chart);auto pcm=decodeWav(bytes);Replay r;if(mode==RuntimeMode::Replay)r=deserializeReplay(replay);return session_->load(c,pcm,mode,mode==RuntimeMode::Replay?&r:nullptr);}catch(const Failure& e){return e.error;}
}
}
