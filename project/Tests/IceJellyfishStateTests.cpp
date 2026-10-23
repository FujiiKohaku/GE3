#include "IceJellyfishStateTests.h"
#include "JellyfishAttackTests.h"
#include "Engine/Time/TimeManager.h"
#include <cstdio>

int main()
{
    TimeManager::GetInstance()->Initialize();
    if (!RunIceJellyfishStateTests()) {
        std::puts("IceJellyfish state regression tests failed.");
        return 1;
    }
    if (!RunJellyfishAttackTests()) {
        std::puts("IceJellyfish active attack regression tests failed.");
        return 1;
    }
    std::puts("All IceJellyfish state and attack regression tests passed.");
    return 0;
}
