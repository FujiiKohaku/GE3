#pragma once

#include <string>

struct AnimationEvent {
    float time = 0.0f;
    std::string name;
    std::string value;
    std::string bone;
};
