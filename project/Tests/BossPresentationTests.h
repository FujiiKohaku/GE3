#pragma once
#include "App/Game/Boss/IceJellyfish/IceJellyfish.h"

inline bool RunBossPresentationTests()
{
    IceJellyfish boss;
    boss.SetPosition({ 10.0f, 20.0f, 30.0f });
    auto positions = boss.GetPresentationPositions();
    if (positions.spotLightPosition.x != -45.0f || positions.spotLightPosition.y != 120.0f
        || positions.spotLightPosition.z != -15.0f || positions.pointLightPosition.x != 55.0f
        || positions.pointLightPosition.y != 40.0f || positions.pointLightPosition.z != -5.0f) { return false; }
    if (positions.fogCenters[0].x != 10.0f || positions.fogCenters[0].y != 4.0f
        || positions.fogCenters[0].z != 30.0f || positions.fogCenters[1].x != 10.0f
        || positions.fogCenters[1].y != 45.0f || positions.fogCenters[1].z != 30.0f) { return false; }
    boss.SetPosition({ 40.0f, 60.0f, 80.0f });
    positions = boss.GetPresentationPositions();
    return positions.spotLightPosition.x == -15.0f && positions.spotLightPosition.y == 160.0f
        && positions.spotLightPosition.z == 35.0f && positions.pointLightPosition.x == 85.0f
        && positions.pointLightPosition.y == 80.0f && positions.pointLightPosition.z == 45.0f
        && positions.fogCenters[0].x == 40.0f && positions.fogCenters[0].y == 4.0f
        && positions.fogCenters[0].z == 80.0f && positions.fogCenters[1].x == 40.0f
        && positions.fogCenters[1].y == 85.0f && positions.fogCenters[1].z == 80.0f;
}
