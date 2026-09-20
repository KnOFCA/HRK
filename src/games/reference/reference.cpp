#include "games/reference/reference.h"
#include <algorithm>
#include <sstream>
#include <locale>
#include <set>
namespace hrk::reference {
namespace {
// Narrow JSON grammar: no recursive arbitrary objects or unbounded parser stack.
class Parser {
  const std::string& s_; size_t p_ = 0;
public:
  explicit Parser(const std::string& s) : s_(s) { require(s.size() <= 16*1024*1024, Error::ChartParseError); }
  void space() { while (p_ < s_.size() && (s_[p_]==' ' || s_[p_]=='\r' || s_[p_]=='\n' || s_[p_]=='\t')) ++p_; }
  bool take(char c) { space(); if (p_ < s_.size() && s_[p_] == c) { ++p_; return true; } return false; }
  void expect(char c) { require(take(c),Error::ChartParseError); }
  std::string key() { expect('"'); size_t start = p_; while (p_ < s_.size() && s_[p_] >= 'a' && s_[p_] <= 'z') ++p_; auto k = s_.substr(start,p_-start); expect('"'); expect(':'); return k; }
  double number() {
    space(); size_t start=p_; if (p_ < s_.size() && s_[p_]=='-') ++p_;
    require(p_ < s_.size() && s_[p_]>='0' && s_[p_]<='9',Error::ChartParseError);
    if (s_[p_]=='0') ++p_; else while (p_<s_.size() && s_[p_]>='0' && s_[p_]<='9') ++p_;
    if (p_<s_.size() && s_[p_]=='.') { ++p_; size_t a=p_; while(p_<s_.size() && s_[p_]>='0' && s_[p_]<='9') ++p_; require(p_>a,Error::ChartParseError); }
    if (p_<s_.size() && (s_[p_]=='e' || s_[p_]=='E')) { ++p_; if(p_<s_.size() && (s_[p_]=='+' || s_[p_]=='-')) ++p_; size_t a=p_; while(p_<s_.size() && s_[p_]>='0' && s_[p_]<='9') ++p_; require(p_>a,Error::ChartParseError); }
    // Classic locale makes parsing independent of machine locale (also supported by Native libc++).
    std::istringstream input(s_.substr(start,p_-start)); input.imbue(std::locale::classic()); double v=0; input >> v;
    require(bool(input) && std::isfinite(v),Error::ChartParseError); return v;
  }
  bool end() { space(); return p_ == s_.size(); }
};
class Loader : public IChartLoader {
public:
  std::shared_ptr<const IRuntimeChart> load(const std::string& text) override {
    try {
      Parser p(text); p.expect('{'); double bpm=0; bool hasBpm=false, hasNotes=false, hasTempos=false;
      std::vector<TempoPoint> tempos; std::vector<std::pair<double,double>> notes;
      std::set<std::string> keys;
      if (!p.take('}')) do {
        const auto key=p.key(); require(keys.insert(key).second,Error::ChartParseError);
        if (key=="bpm") { bpm=p.number(); hasBpm=true; }
        else if (key=="notes" || key=="tempos") {
          const bool isNote = key=="notes"; if (isNote) hasNotes=true; else hasTempos=true;
          p.expect('['); if (!p.take(']')) {
            do {
              p.expect('{'); double beat=-1, value=-1; std::set<std::string> fields;
              do { auto k=p.key(); require(fields.insert(k).second,Error::ChartParseError); auto v=p.number();
                if(k=="beat") beat=v; else if(k==(isNote?"x":"bpm")) value=v; else throw Failure(Error::ChartParseError);
              } while(p.take(',')); p.expect('}');
              require(fields.size()==2 && beat>=0 && beat<=1000000,Error::ChartParseError);
              if (isNote) { require(value>=0 && value<=1 && notes.size()<100000,Error::ChartParseError); notes.push_back({beat,value}); }
              else { require(tempos.size()<4096,Error::ChartParseError); tempos.push_back({beat,value}); }
            } while (p.take(',')); p.expect(']');
          }
        } else throw Failure(Error::ChartParseError);
      } while(p.take(','));
      // An initially empty object has already consumed its closing brace.
      if (!keys.empty()) p.expect('}');
      require(p.end() && hasBpm && hasNotes && bpm>0 && bpm<=1000,Error::ChartParseError);
      if (!hasTempos) tempos.push_back({0,bpm});
      TimingMap timing(std::move(tempos)); std::vector<SimpleNote> compiled; compiled.reserve(notes.size());
      EntityIds ids;
      for (const auto& n : notes) { auto t=timing.beatToTime(n.first); require(t<=maxTime-window-1,Error::ChartParseError); compiled.push_back({ids.next(),t,static_cast<float>(n.second)}); }
      std::stable_sort(compiled.begin(),compiled.end(),[](const auto& a,const auto& b){return a.time<b.time;});
      return std::make_shared<const ReferenceRuntimeChart>(std::move(compiled),hashBytes(text));
    } catch(const Failure&) { throw Failure(Error::ChartParseError); }
  }
};
class Rules : public IGameRules {
  const ReferenceRuntimeChart* chart_ = nullptr; std::vector<uint8_t> done_; size_t cursor_=0, count_=0;
  void finish(size_t i, JudgmentId type, TimeNs input, IJudgmentSink& sink) {
    done_[i]=1; ++count_; const auto& n=chart_->notes()[i]; sink.emit({n.id,type,n.time,input,input-n.time});
  }
public:
  void reset(const IRuntimeChart& c) override { chart_=&dynamic_cast<const ReferenceRuntimeChart&>(c); done_.assign(c.size(),0); cursor_=count_=0; }
  void advance(TimeNs time,IJudgmentSink& sink) override {
    const auto& notes=chart_->notes();
    while (cursor_<notes.size() && notes[cursor_].time+window < time) { if(!done_[cursor_]) finish(cursor_,Miss,notes[cursor_].time+window+1,sink); ++cursor_; }
  }
  void input(const InputEvent& e,IJudgmentSink& sink) override {
    advance(e.songTime,sink); if (e.phase!=InputPhase::Down) return;
    const auto& notes=chart_->notes();
    for (size_t i=cursor_;i<notes.size() && notes[i].time<=e.songTime+window;++i) {
      // float32 coordinates: compare in float32 so the inclusive 0.10 boundary stays inclusive.
      if(!done_[i] && std::abs(e.position.x-notes[i].x)<=0.100000024f) { finish(i,Perfect,e.songTime,sink); while(cursor_<done_.size() && done_[cursor_]) ++cursor_; return; }
    }
  }
  bool complete() const override { return chart_ && count_==chart_->size(); }
};
class ScoreSystem : public IScoreSystem {
  Score value_;
public: void reset() override { value_={}; }
  void add(const JudgmentResult& j) override { if(j.type==Perfect) { value_.value+=100; ++value_.perfect; } else if(j.type==Miss) ++value_.miss; }
  Score score() const override { return value_; }
};
class Input : public IGameInput {
public: std::vector<InputEvent> autoPlay(const IRuntimeChart& c) override {
  const auto& chart=dynamic_cast<const ReferenceRuntimeChart&>(c); std::vector<InputEvent> events; events.reserve(chart.size()*2);
  for(const auto& n:chart.notes()) { events.push_back({0,InputPhase::Down,{n.x,0.8f},n.time}); events.push_back({0,InputPhase::Up,{n.x,0.8f},n.time}); } return events;
} };
class Renderer : public IGameRenderer {
public: void render(const IRuntimeChart& c,TimeNs time,IRenderContext& out) override {
  const auto& notes=dynamic_cast<const ReferenceRuntimeChart&>(c).notes(); out.clear({0.03f,0.05f,0.09f,1});
  out.sprite({{0.5f,0.8f},{1,0.005f},{0.3f,0.5f,0.6f,1},0});
  auto first=std::lower_bound(notes.begin(),notes.end(),time-window,[](const auto& n,TimeNs t){return n.time<t;});
  for(auto it=first;it!=notes.end() && it->time<=time+2*second;++it) out.sprite({{it->x,0.8f-static_cast<float>(seconds(it->time-time))*0.35f},{0.065f,0.035f},{0.1f,0.9f,0.7f,1},0});
} };
}
std::unique_ptr<IChartLoader> ReferenceGame::createChartLoader(){return std::make_unique<Loader>();}
std::unique_ptr<IGameRules> ReferenceGame::createRules(){return std::make_unique<Rules>();}
std::unique_ptr<IGameInput> ReferenceGame::createInput(){return std::make_unique<Input>();}
std::unique_ptr<IGameRenderer> ReferenceGame::createRenderer(){return std::make_unique<Renderer>();}
std::unique_ptr<IScoreSystem> ReferenceGame::createScoreSystem(){return std::make_unique<ScoreSystem>();}
}
