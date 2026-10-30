#pragma once
#include "Engine/PostEffect/PostEffectType.h"
#include <string>

inline const char* EditorLabel(const std::string& key)
{
    struct Label { const char* key; const char* label; };
    static constexpr Label kLabels[] = {
        { "Emitter", "発生" }, { "Particle", "粒子" }, { "Simulation", "動き" }, { "Render", "描画" },
        { "Trail", "軌跡" }, { "Light", "ライト" }, { "Fields", "フィールド" }, { "Shaders", "シェーダー" },
        { "DisplayName", "表示名" }, { "Count", "発生数" }, { "EmitCount", "発生数" }, { "Radius", "範囲" },
        { "Frequency", "発生間隔（秒）" }, { "Shape", "発生形状" }, { "LifeTime", "寿命（秒）" },
        { "MaxParticles", "粒子上限" }, { "StartScale", "開始サイズ" }, { "EndScale", "終了サイズ" },
        { "StartColor", "開始色" }, { "EndColor", "終了色" }, { "StartRotation", "開始角度" },
        { "RotationSpeed", "回転速度" }, { "Velocity", "速度（X・Y・Z）" }, { "EnableGravity", "重力を使用" },
        { "Gravity", "重力" }, { "EnableDrag", "抵抗を使用" }, { "Drag", "抵抗" }, { "EnableNoise", "ノイズを使用" },
        { "NoiseStrength", "ノイズの強さ" }, { "EnableAttraction", "吸引を使用" }, { "AttractionStrength", "吸引の強さ" },
        { "Texture", "テクスチャ" }, { "MeshType", "メッシュ形状" }, { "BlendMode", "合成方法" },
        { "DepthTest", "深度判定" }, { "DepthWrite", "深度を書き込む" }, { "CullMode", "裏面の扱い" },
        { "Loop", "ループ" }, { "Duration", "再生時間（秒・-1で無制限）" }, { "Type", "種類" },
        { "MaxPoints", "軌跡の点数上限" }, { "MinVertexDistance", "点の最小間隔" }, { "BreakDistance", "軌跡を切る距離" },
        { "StartWidth", "開始幅" }, { "EndWidth", "終了幅" }, { "TextureTiling", "テクスチャの繰り返し" },
        { "FaceCamera", "カメラに向ける" }, { "RootExtension", "根元の延長" }, { "Enabled", "有効" },
        { "Color", "色" }, { "Offset", "位置の補正" }, { "Intensity", "光の強さ" }, { "Decay", "減衰" },
        { "FadeDuration", "消える時間（秒）" }, { "FollowEmitter", "発生源に追従" }, { "FadeOut", "徐々に消す" },
        { "Position", "位置（X・Y・Z）" }, { "Direction", "方向（X・Y・Z）" }, { "Strength", "強さ" },
        { "Space", "座標の基準" }, { "Falloff", "距離による減衰" }, { "Emit", "発生処理" }, { "Update", "更新処理" },
        { "Vertex", "頂点処理" }, { "Pixel", "色の処理" }, { "RenderParameter", "描画パラメーター" },
        { "DissolveThreshold", "消滅のしきい値" }, { "EmissionStrength", "発光の強さ" }, { "UvScrollSpeed", "画像の移動速度" },
        { "UVScrollSpeed", "画像の移動速度" }, { "grayScaleStrength", "白黒の強さ" }, { "vignetteStrength", "周辺を暗くする強さ" },
        { "outlineScale", "輪郭の太さ" }, { "outlineThreshold", "深度のしきい値" }, { "outlineSoftness", "輪郭の滑らかさ" },
        { "outlineNormalStrength", "法線の輪郭強度" }, { "radialBlurWidth", "放射状ブラーの幅" },
        { "radialBlurSampleCount", "放射状ブラーのサンプル数" }, { "dissolveThreshold", "消滅のしきい値" },
        { "dissolveEdgeWidth", "消滅境界の幅" }, { "dissolveEdgeStrength", "消滅境界の明るさ" },
        { "pixelSize", "ピクセルの大きさ" }, { "colorBrightness", "明るさ" }, { "colorContrast", "コントラスト" },
        { "colorSaturation", "彩度" }, { "focusDepth", "ピントの位置" }, { "focusRange", "ピントの範囲" },
        { "depthOfFieldRadius", "被写界深度のぼかし" }, { "motionBlurStrength", "移動ブラーの強さ" },
        { "chromaticAberrationStrength", "色ずれの強さ" }, { "lensDistortionStrength", "レンズのゆがみ" },
        { "filmGrainStrength", "粒状ノイズの強さ" }, { "lensDirtStrength", "レンズ汚れの強さ" },
        { "cameraShakeStrength", "画面の揺れ" }, { "bokehRadius", "玉ぼけの大きさ" }, { "fisheyeStrength", "魚眼の強さ" },
        { "lightThreshold", "光のしきい値" }, { "lightStrength", "光の強さ" }, { "lightRadius", "光の広がり" },
        { "lightAngle", "光の角度" }, { "threshold", "しきい値" }, { "intensity", "強度" },
        { "blurRadius", "ぼかし半径" }, { "blurSigma", "ぼかしの滑らかさ" }, { "softKnee", "抽出の滑らかさ" },
        { "maxRadiance", "輝度の上限" }, { "exposure", "露出" }, { "fogColor", "霧の色" },
        { "distanceFog", "距離の霧（開始・終了・濃度・曲線）" }, { "fxaa", "画面の縁を滑らかにする" },
        { "toneMapping", "色の変換方式" }, { "compensationEv", "露出補正" }
    };
    for (const auto& label : kLabels) { if (key == label.key) { return label.label; } }
    return nullptr;
}

inline const char* PostLabel(PostEffectType type)
{
    switch (type) {
    case PostEffectType::Fog: return "フォグ";
    case PostEffectType::Bloom: return "ブルーム";
    case PostEffectType::ColorAdjust: return "色調整";
    case PostEffectType::GrayScale: return "白黒";
    case PostEffectType::Vignette: return "周辺減光";
    case PostEffectType::DepthOutline: return "深度の輪郭線";
    case PostEffectType::LuminanceBasedOutline: return "明るさの輪郭線";
    case PostEffectType::Outline: return "輪郭線";
    case PostEffectType::RadialBlur: return "放射状ブラー";
    case PostEffectType::Dissolve: return "消滅";
    case PostEffectType::DepthOfField: return "被写界深度";
    case PostEffectType::MotionBlur: return "移動ブラー";
    case PostEffectType::ChromaticAberration: return "色ずれ";
    case PostEffectType::LensDistortion: return "レンズのゆがみ";
    case PostEffectType::FilmGrain: return "粒状ノイズ";
    case PostEffectType::LensDirt: return "レンズ汚れ";
    case PostEffectType::CameraShake: return "画面の揺れ";
    case PostEffectType::BokehShape: return "玉ぼけ";
    case PostEffectType::Fisheye: return "魚眼";
    case PostEffectType::Pixelate: return "ピクセル化";
    case PostEffectType::smoothing: return "平滑化";
    case PostEffectType::GaussianFilter: return "ガウスぼかし";
    case PostEffectType::Random: return "ランダムノイズ";
    case PostEffectType::LensFlare: return "レンズフレア";
    case PostEffectType::Glare: return "まぶしさ";
    case PostEffectType::LightShafts: return "光の筋";
    case PostEffectType::VolumetricLight: return "空気中の光";
    case PostEffectType::AnamorphicFlare: return "横長フレア";
    case PostEffectType::Halo: return "光の輪";
    case PostEffectType::LightStreak: return "光の残像";
    case PostEffectType::NeonGlow: return "ネオン発光";
    case PostEffectType::GhostImage: return "光のゴースト";
    case PostEffectType::FocusLine: return "集中線";
    case PostEffectType::Paint: return "ペイント";
    case PostEffectType::GlassCrack: return "ガラスのひび";
    case PostEffectType::Shockwave: return "衝撃波";
    case PostEffectType::HeatHaze: return "陽炎";
    case PostEffectType::SonicBoom: return "ソニックブーム";
    case PostEffectType::RainDrops: return "雨粒";
    case PostEffectType::CyberScanline: return "走査線";
    case PostEffectType::HexShield: return "六角形シールド";
    case PostEffectType::BlackHoleDistortion: return "ブラックホール";
    case PostEffectType::ArchiveAtmosphere: return "紙の空気感";
    case PostEffectType::ScreenLighting: return "画面の陰影";
    default: return "標準";
    }
}
