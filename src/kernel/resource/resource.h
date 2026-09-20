#pragma once
#include "interface/platform/platform.h"
namespace hrk { class NativeFileSystem : public IFileSystem {
public: std::vector<uint8_t> read(const std::string& path) override;
  void write(const std::string& path, const std::vector<uint8_t>& bytes) override;
}; }
