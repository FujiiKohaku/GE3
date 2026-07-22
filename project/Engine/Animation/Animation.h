#pragma once
#include "Event/AnimationEvent.h"
#include "NodeAnimation.h"
#include <map>
#include <string>
#include <vector>

struct Animation {
    std::string name;
    float duration = 0.0f;
    std::map<std::string, NodeAnimation> nodeAnimations;
    std::vector<AnimationEvent> events;
};
