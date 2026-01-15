#pragma once
#include "KeyFrame.h"
#include <vector>

struct NodeAnimation {
    std::vector<KeyframeVector3> position;
    std::vector<KeyframeQuaternion> rotation;
    std::vector<KeyframeVector3> scale;
};
