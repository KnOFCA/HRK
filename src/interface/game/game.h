#pragma once
#include "interface/render/render.h"
#include <memory>
namespace hrk {
struct GameDescriptor { GameId id; uint32_t ruleVersion; };
class IRuntimeChart { public: virtual ~IRuntimeChart() = default; virtual uint64_t hash() const = 0; virtual size_t size() const = 0; virtual TimeNs endTime() const = 0; };
class IChartLoader { public: virtual ~IChartLoader() = default; virtual std::shared_ptr<const IRuntimeChart> load(const std::string& text) = 0; };
class IJudgmentSink { public: virtual ~IJudgmentSink() = default; virtual void emit(const JudgmentResult&) = 0; };
class IGameRules {
public:
  virtual ~IGameRules() = default;
  virtual void reset(const IRuntimeChart&) = 0;
  virtual void advance(TimeNs, IJudgmentSink&) = 0;
  virtual void input(const InputEvent&, IJudgmentSink&) = 0;
  virtual bool complete() const = 0;
};
class IGameInput { public: virtual ~IGameInput() = default; virtual std::vector<InputEvent> autoPlay(const IRuntimeChart&) = 0; };
class IGameRenderer { public: virtual ~IGameRenderer() = default; virtual void render(const IRuntimeChart&, TimeNs, IRenderContext&) = 0; };
class IScoreSystem { public: virtual ~IScoreSystem() = default; virtual void reset() = 0; virtual void add(const JudgmentResult&) = 0; virtual Score score() const = 0; };
class IGameModule {
public:
  virtual ~IGameModule() = default;
  virtual GameDescriptor descriptor() const = 0;
  virtual std::unique_ptr<IChartLoader> createChartLoader() = 0;
  virtual std::unique_ptr<IGameRules> createRules() = 0;
  virtual std::unique_ptr<IGameInput> createInput() = 0;
  virtual std::unique_ptr<IGameRenderer> createRenderer() = 0;
  virtual std::unique_ptr<IScoreSystem> createScoreSystem() = 0;
};
}
