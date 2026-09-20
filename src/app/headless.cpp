#include "games/reference/reference.h"
#include "platform/headless/headless.h"
#include "kernel/gameplay/session.h"
#include "kernel/resource/resource.h"
#include "kernel/audio/audio.h"
#include <iostream>
int main(int argc,char** argv){
  using namespace hrk;
  try{
    if(argc!=4){std::cerr<<"Usage: hrk_headless chart.json audio.wav input.replay\n";return 2;}
    NativeFileSystem fs; auto bytes=fs.read(argv[1]); reference::ReferenceGame game;
    auto chart=game.createChartLoader()->load(std::string(bytes.begin(),bytes.end()));auto pcm=decodeWav(fs.read(argv[2]));auto replay=deserializeReplay(fs.read(argv[3]));
    FakeAudioBackend audio; GameplaySession session(game,audio);auto e=session.load(chart,pcm,RuntimeMode::Replay,&replay);require(e==Error::Ok,e);require(session.start(0)==Error::Ok,Error::BackendFailure);
    while(session.state()==SessionState::Playing){audio.advance(16*ms);require(session.update()==Error::Ok,Error::InvalidState);}
    std::cout<<"{\"score\":"<<session.score().value<<",\"perfect\":"<<session.score().perfect<<",\"miss\":"<<session.score().miss<<",\"judgments\":[";
    bool first=true;for(const auto& j:session.judgments()){if(!first)std::cout<<',';first=false;std::cout<<"{\"id\":"<<j.entity<<",\"type\":"<<j.type<<",\"targetTime\":"<<j.targetTime<<",\"inputTime\":"<<j.inputTime<<",\"error\":"<<j.error<<'}';}std::cout<<"]}\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;
}
