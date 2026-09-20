#pragma once
#include "kernel/base/base.h"
namespace hrk {
using TextureHandle = uint32_t;
struct Color { float r, g, b, a; };
struct SpriteDrawCommand { Vec2 position; Vec2 size; Color color; TextureHandle texture = 0; };
class IRenderContext {
public:
  virtual ~IRenderContext() = default;
  virtual void clear(Color color) = 0;
  virtual void sprite(const SpriteDrawCommand& command) = 0;
};
}
