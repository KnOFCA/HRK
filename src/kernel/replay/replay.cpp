#include "kernel/replay/replay.h"
#include <cstring>
#include <limits>
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559, "Replay requires IEEE754 float32");
namespace hrk {
void validateReplay(const Replay& r, uint64_t hash, GameId game, uint32_t rule) {
  require(r.header.formatVersion == 1 && r.header.kernelVersion == 1, Error::UnsupportedVersion);
  require(r.header.gameId == game && r.header.ruleVersion == rule, Error::ReplayIncompatible);
  require(r.header.chartHash == hash, Error::ReplayChartMismatch);
  require(r.events.size() <= maxEvents, Error::ReplayInvalid);
  TimeNs previous = 0;
  for (const auto& e : r.events) {
    require(validTime(e.songTime) && e.songTime >= previous && validPosition(e.position) && static_cast<uint32_t>(e.phase) <= 3, Error::ReplayInvalid);
    previous = e.songTime;
  }
}
std::vector<uint8_t> serializeReplay(const Replay& r) {
  validateReplay(r, r.header.chartHash, r.header.gameId, r.header.ruleVersion);
  std::vector<uint8_t> bytes; bytes.reserve(32 + r.events.size()*24);
  auto put = [&](uint64_t value, size_t count) { for (size_t i = 0; i < count; ++i) bytes.push_back(static_cast<uint8_t>(value >> (i*8))); };
  bytes.insert(bytes.end(), {'H','R','K','R'});
  put(r.header.formatVersion,4); put(r.header.kernelVersion,4); put(r.header.gameId,4); put(r.header.chartHash,8); put(r.header.ruleVersion,4); put(r.events.size(),4);
  for (const auto& e : r.events) {
    put(static_cast<uint64_t>(e.songTime),8); put(e.pointerId,4); put(static_cast<uint32_t>(e.phase),4);
    uint32_t x, y; std::memcpy(&x,&e.position.x,4); std::memcpy(&y,&e.position.y,4); put(x,4); put(y,4);
  }
  return bytes;
}
Replay deserializeReplay(const std::vector<uint8_t>& b) {
  require(b.size() >= 32 && std::memcmp(b.data(),"HRKR",4) == 0, Error::ReplayInvalid);
  size_t offset = 4;
  auto get = [&](size_t n) { uint64_t v = 0; for (size_t i=0;i<n;++i) v |= uint64_t(b[offset++]) << (8*i); return v; };
  Replay r; r.header.formatVersion = static_cast<uint32_t>(get(4)); r.header.kernelVersion = static_cast<uint32_t>(get(4));
  require(r.header.formatVersion == 1 && r.header.kernelVersion == 1, Error::UnsupportedVersion);
  r.header.gameId = static_cast<uint32_t>(get(4)); r.header.chartHash = get(8); r.header.ruleVersion = static_cast<uint32_t>(get(4));
  const auto count = get(4); require(count <= maxEvents && b.size() == 32+count*24, Error::ReplayInvalid);
  r.events.reserve(static_cast<size_t>(count));
  for (size_t i=0;i<count;++i) {
    InputEvent e; const auto time = get(8); require(time <= static_cast<uint64_t>(maxTime), Error::ReplayInvalid);
    e.songTime = static_cast<TimeNs>(time); e.pointerId = static_cast<uint32_t>(get(4)); e.phase = static_cast<InputPhase>(get(4));
    const auto x = static_cast<uint32_t>(get(4)), y = static_cast<uint32_t>(get(4));
    std::memcpy(&e.position.x,&x,4); std::memcpy(&e.position.y,&y,4); r.events.push_back(e);
  }
  validateReplay(r,r.header.chartHash,r.header.gameId,r.header.ruleVersion); return r;
}
}
