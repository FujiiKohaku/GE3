#pragma once
#include "Animation.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cassert>
#include <string>
class AnimationLoder {

    public:
    static Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);


};
