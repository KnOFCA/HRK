#pragma once
#include "kernel/base/base.h"
namespace hrk {
struct ReplayHeader { uint32_t formatVersion = 1, kernelVersion = 1; GameId gameId = 1; uint64_t chartHash = 0; uint32_t ruleVersion = 1; };
struct Replay { ReplayHeader header; std::vector<InputEvent> events; };
void validateReplay(const Replay& replay, uint64_t chartHash, GameId gameId, uint32_t ruleVersion);
std::vector<uint8_t> serializeReplay(const Replay& replay);
Replay deserializeReplay(const std::vector<uint8_t>& bytes);
}
