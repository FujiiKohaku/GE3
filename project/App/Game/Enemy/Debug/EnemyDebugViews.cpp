#include "BaseEnemyDebugView.h"
#include "App/Game/Enemy/MoveEnemy/MoveEnemy.h"
#include "App/Game/Boss/IceJellyfish/IceJellyfish.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

#if defined(_DEBUG) || defined(ENABLE_DEVELOPMENT_TOOLS)
class MoveEnemyDebugView final : public BaseEnemyDebugView {
public:
    explicit MoveEnemyDebugView(MoveEnemy& enemy) : enemy_(enemy) {}
    void DrawInspector() override {
        const char* patterns[] = { "LeftRight", "UpDown", "ZigZag", "SineWave" };
        int pattern = static_cast<int>(enemy_.GetMovePattern());
        if (ImGui::Combo("Move Pattern", &pattern, patterns, 4)) enemy_.SetMovePattern(static_cast<MovePattern>(pattern));
        float speed = enemy_.GetMoveSpeed();
        float amplitude = enemy_.GetAmplitude();
        float frequency = enemy_.GetFrequency();
        if (ImGui::SliderFloat("Move Speed", &speed, 0.0f, 10.0f)) enemy_.SetMoveSpeed(speed);
        if (ImGui::SliderFloat("Amplitude", &amplitude, 0.0f, 20.0f)) enemy_.SetAmplitude(amplitude);
        if (ImGui::SliderFloat("Frequency", &frequency, 0.0f, 10.0f)) enemy_.SetFrequency(frequency);
        if (ImGui::Button("Reset Position")) enemy_.ResetPosition();
    }
    bool HasMovementControls() const override { return true; }
    void AppendControls(nlohmann::json& controls, int index) const override {
        const std::string prefix = "enemy." + std::to_string(index) + ".";
        const std::string group = "移動する敵 " + std::to_string(index + 1);
        controls.push_back({{"key",prefix+"pattern"},{"label","移動パターン"},{"group",group},{"type","number"},{"minimum",0},{"maximum",3},{"step",1}});
        controls.push_back({{"key",prefix+"speed"},{"label","速度"},{"group",group},{"type","number"},{"minimum",0},{"maximum",10},{"step",0.1}});
        controls.push_back({{"key",prefix+"amplitude"},{"label","振幅"},{"group",group},{"type","number"},{"minimum",0},{"maximum",20},{"step",0.1}});
        controls.push_back({{"key",prefix+"frequency"},{"label","周波数"},{"group",group},{"type","number"},{"minimum",0},{"maximum",10},{"step",0.1}});
        controls.push_back({{"key",prefix+"reset"},{"label","位置を戻す"},{"group",group},{"type","action"}});
    }
    nlohmann::json GetState(int index) const override {
        return {{"index",index},{"pattern",static_cast<int>(enemy_.GetMovePattern())},
            {"speed",enemy_.GetMoveSpeed()},{"amplitude",enemy_.GetAmplitude()},{"frequency",enemy_.GetFrequency()}};
    }
    bool Apply(const std::string& field, const std::string& value) override {
        if (field == "reset") { enemy_.ResetPosition(); return true; }
        char* end = nullptr;
        const float number = std::strtof(value.c_str(), &end);
        if (end == value.c_str() || *end != '\0' || !std::isfinite(number)) return false;
        if (field == "pattern") enemy_.SetMovePattern(static_cast<MovePattern>(static_cast<int>(std::clamp(number, 0.0f, 3.0f))));
        else if (field == "speed") enemy_.SetMoveSpeed(std::clamp(number, 0.0f, 10.0f));
        else if (field == "amplitude") enemy_.SetAmplitude(std::clamp(number, 0.0f, 20.0f));
        else if (field == "frequency") enemy_.SetFrequency(std::clamp(number, 0.0f, 10.0f));
        else return false;
        return true;
    }
private:
    MoveEnemy& enemy_;
};

class JellyfishDebugView final : public BaseEnemyDebugView {
public:
    explicit JellyfishDebugView(IceJellyfish& enemy) : enemy_(enemy) {}
    void DrawInspector() override {
        ImGui::Text("Ice Jellyfish core HP: %.0f / %.0f", enemy_.GetHp(), IceJellyfish::kMaxHp);
    }
    bool HasBossInspector() const override { return true; }
    float GetBossHp() const override { return enemy_.GetHp(); }
    void DrawBossInspector(bool& showCollision, float& railDistance, const Vector3& position) override {
        DrawInspector();
        ImGui::Checkbox("Show Ice Jellyfish collision", &showCollision);
        if (ImGui::Button("Teleport to Ice Jellyfish")) railDistance = (std::max)(0.0f, position.z - 200.0f);
    }
    void DrawCollisionDebug() const override { enemy_.DrawCollisionDebug(); }
private:
    IceJellyfish& enemy_;
};

std::unique_ptr<BaseEnemyDebugView> MoveEnemy::CreateDebugView() { return std::make_unique<MoveEnemyDebugView>(*this); }
std::unique_ptr<BaseEnemyDebugView> IceJellyfish::CreateDebugView() { return std::make_unique<JellyfishDebugView>(*this); }
#endif
