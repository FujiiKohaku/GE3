#pragma once
#include "KeyFrame.h"
#include <vector>

struct NodeAnimation {
    std::vector<KeyframeVector3> translate;
    std::vector<KeyframeQuaternion> rotation;
    std::vector<KeyframeVector3> scale;
};
