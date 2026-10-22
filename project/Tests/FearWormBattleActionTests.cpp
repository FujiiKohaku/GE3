#include "FearWormBattleActionTests.h"
#include "FearWormTimingTests.h"
#include "AppTimingTests.h"
#include "FearWormStateTests.h"
#include "FearWormMovementTests.h"
#include "FearWormEntryTests.h"
#include "BeamStateTests.h"
#include "Engine/Time/TimeManager.h"
#include <cstdio>

int main()
{
    TimeManager::GetInstance()->Initialize();
    if (!RunAppTimingTests() || !RunFearWormTimingTests() || !RunFearWormBattleActionTests() || !RunFearWormStateTests()
        || !RunFearWormMovementTests() || !RunFearWormEntryTests() || !RunBeamStateTests()) {
        std::puts("FearWorm regression tests failed.");
        return 1;
    }
    std::puts("All FearWorm regression tests passed.");
    return 0;
}
