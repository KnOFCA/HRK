#include "kernel/resource/resource.h"
#include <fstream>
namespace hrk {
std::vector<uint8_t> NativeFileSystem::read(const std::string& path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate); require(bool(f), Error::ResourceNotFound);
  const auto size = f.tellg(); require(size >= 0 && size <= 128*1024*1024, Error::InvalidArgument);
  std::vector<uint8_t> bytes(static_cast<size_t>(size)); f.seekg(0);
  f.read(reinterpret_cast<char*>(bytes.data()), size); require(bool(f), Error::ResourceNotFound); return bytes;
}
void NativeFileSystem::write(const std::string& path, const std::vector<uint8_t>& b) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc); require(bool(f), Error::ResourceNotFound);
  f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size())); f.flush(); require(bool(f), Error::ResourceNotFound);
}
}
