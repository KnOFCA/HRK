#include "kernel/audio/audio.h"
#include <cstring>
namespace hrk {
Pcm decodeWav(const std::vector<uint8_t>& b) {
  require(b.size() >= 44 && b.size() <= 128 * 1024 * 1024, Error::AudioInvalid);
  auto u16 = [&](size_t i) { return uint32_t(b[i]) | (uint32_t(b[i+1]) << 8); };
  auto u32 = [&](size_t i) { return u16(i) | (u16(i+2) << 16); };
  require(std::memcmp(b.data(), "RIFF", 4) == 0 && std::memcmp(b.data()+8, "WAVE", 4) == 0 && u32(4) == b.size()-8, Error::AudioInvalid);
  Pcm pcm; bool fmt = false, data = false;
  for (size_t p = 12; p + 8 <= b.size();) {
    const size_t n = u32(p+4), start = p+8;
    require(n <= b.size()-start, Error::AudioInvalid);
    if (std::memcmp(b.data()+p, "fmt ", 4) == 0) {
      require(!fmt && n >= 16 && u16(start) == 1, Error::AudioInvalid);
      pcm.channels = u16(start+2); pcm.sampleRate = u32(start+4);
      require((pcm.channels == 1 || pcm.channels == 2) && pcm.sampleRate == 48000 && u16(start+14) == 16 && u16(start+12) == pcm.channels*2 && u32(start+8) == pcm.sampleRate*pcm.channels*2, Error::AudioInvalid); fmt = true;
    } else if (std::memcmp(b.data()+p, "data", 4) == 0) {
      require(fmt && !data && n > 0 && n % (pcm.channels*2) == 0, Error::AudioInvalid);
      pcm.samples.resize(n/2);
      for (size_t i = 0; i < n/2; ++i) pcm.samples[i] = static_cast<int16_t>(u16(start+i*2));
      data = true;
    }
    p = start+n+(n%2);
    require(p <= b.size(), Error::AudioInvalid);
  }
  require(fmt && data, Error::AudioInvalid); return pcm;
}
}
