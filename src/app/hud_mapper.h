#pragma once

#include "app/view_state.h"

namespace game2048 {

class AIAdvisor;
class GameController;
class InteractionSession;

HUDState BuildHUDState(const GameController& gameController,
                       const InteractionSession& session,
                       const AIAdvisor& advisor,
                       AnimationSpeed animationSpeed);

}  // namespace game2048