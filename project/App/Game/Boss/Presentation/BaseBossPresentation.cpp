#include "BaseBossPresentation.h"
#include "App/Game/Boss/Common/BaseBoss.h"

class JellyfishPresentation final : public BaseBossPresentation {
public:
    void Update(const BaseBoss& boss, const BossPresentationContext& context) override {
        const Vector3 position = boss.GetPosition();
        if (context.setSpotPosition) context.setSpotPosition(position + Vector3 { -55.0f, 100.0f, -45.0f });
        if (context.setPointPosition) context.setPointPosition(position + Vector3 { 45.0f, 20.0f, -35.0f });
        if (context.followFog && context.setFogCenter) {
            context.setFogCenter(0, { position.x, 4.0f, position.z });
            context.setFogCenter(1, position + Vector3 { 0.0f, 25.0f, 0.0f });
        }
    }
};

std::unique_ptr<BaseBossPresentation> CreateJellyfishPresentation()
{
    return std::make_unique<JellyfishPresentation>();
}
