#pragma once
#include "NodeAnimation.h"
#include <map>
#include <string>

struct Animation {
    float duration = 0.0f;
    std::map<std::string, NodeAnimation> nodeAnimations;
};
