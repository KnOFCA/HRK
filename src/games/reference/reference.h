#pragma once
#include "interface/game/game.h"
#include "kernel/timeline/timeline.h"
namespace hrk::reference {
constexpr JudgmentId Perfect = 1, Miss = 2;
constexpr TimeNs window = 50 * ms;
struct SimpleNote { EntityId id; TimeNs time; float x; };
class ReferenceRuntimeChart : public IRuntimeChart {
  const std::vector<SimpleNote> notes_; const uint64_t hash_;
public:
  ReferenceRuntimeChart(std::vector<SimpleNote> notes, uint64_t hash) : notes_(std::move(notes)), hash_(hash) {}
  const std::vector<SimpleNote>& notes() const { return notes_; }
  uint64_t hash() const override { return hash_; } size_t size() const override { return notes_.size(); }
  TimeNs endTime() const override { return notes_.empty() ? 0 : notes_.back().time + window + 1; }
};
class ReferenceGame : public IGameModule {
public:
  GameDescriptor descriptor() const override { return {1,1}; }
  std::unique_ptr<IChartLoader> createChartLoader() override;
  std::unique_ptr<IGameRules> createRules() override;
  std::unique_ptr<IGameInput> createInput() override;
  std::unique_ptr<IGameRenderer> createRenderer() override;
  std::unique_ptr<IScoreSystem> createScoreSystem() override;
};
}
