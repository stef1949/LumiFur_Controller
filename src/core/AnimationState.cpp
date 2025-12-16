#include "core/AnimationState.h"

namespace animation
{

namespace
{
AnimationState gState{};
}

AnimationState &state()
{
  return gState;
}

void reset(AnimationState &state)
{
  state = AnimationState{};
}

} // namespace animation

