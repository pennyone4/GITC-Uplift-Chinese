#include "ui/settings_schema.hpp"
#include "nr/types.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <type_traits>
#include <utility>
#include <vector>

namespace uplift::ui {
namespace {

constexpr std::array<std::string_view, 3> MODELS = {"风格 A", "风格 B", "风格 C"};
constexpr std::array<std::string_view, 3> NR_NETWORKS = {"运行时默认", "A：旧版（Legacy）", "B：小型 FP8（Small FP8）"};
constexpr std::array<std::string_view, 3> PASS_NETWORKS = {"与第 1 层相同", "A：旧版（Legacy）", "B：小型 FP8（Small FP8）"};
constexpr std::array<std::string_view, 3> SOURCES = {"自动", "DLSS 后", "后处理"};
constexpr std::array<std::string_view, 5> MOTION_VECTORS = {"自动", "DLSS", "LaunchPad", "无", "Lumenite"};  // stored-index order (Setup shows Lumenite after Launchpad)
constexpr std::array<std::string_view, 7> QUALITY_MODES = {"游戏默认", "性能", "均衡", "质量",
                                                           "极致性能", "极致质量", "DLAA"};
constexpr std::array<std::string_view, 5> PRESETS = {"游戏默认", "J", "K", "L", "M"};
constexpr std::array<std::string_view, 3> AUTO_EXPOSURE = {"游戏默认", "关闭", "开启"};
constexpr std::array<std::string_view, 6> ENCODINGS = {"自动", "线性 BT.709", "sRGB", "BT.2100 PQ", "scRGB", "scRGB-nl"};
constexpr std::array<std::string_view, 3> DEPTH_DIRECTIONS = {"游戏默认", "正向", "反向"};
constexpr std::array<std::string_view, 2> STATE_RESTORE_MODES = {"完整", "最小"};
constexpr std::array<std::string_view, 4> LOG_LEVELS = {"错误", "警告", "信息", "调试"};
constexpr std::array<std::string_view, 2> NGX_HOOK_MODES = {"自动", "关闭（安全模式）"};
constexpr std::array<std::string_view, 2> FOREIGN_NR_MODES = {"让出", "观察"};
constexpr std::array<std::string_view, 3> VRAM_CHECKS = {"保守", "宽松", "关闭"};  // 2026-10-09
constexpr std::array<std::string_view, 3> VULKAN_NR_MODES = {"原生", "Direct3D 12", "辅助进程"};  // Plan 19; T5: Helper
constexpr std::array<std::string_view, 6> RESOLUTION_MODES = {"全分辨率", "质量（67 %）", "均衡（58 %）",
                                                              "性能（50 %）", "匹配游戏", "自定义"};
constexpr std::array<std::string_view, 2> UPSAMPLING_MODES = {"经典", "边缘感知"};
// Split chain (2026-10-10): a pass's Runs at after its first choice, "Same as pass n-1" (PassStageChoices), in stored-index order.
constexpr std::array<std::string_view, 3> PASS_STAGES = {"DLSS 前", "DLSS 后", "后处理"};
constexpr std::array<std::string_view, 2> MASK_MODES = {"自动（UPLIFT_MASK）", "关闭"};
constexpr std::array<std::string_view, 3> UI_CORRECTION_MODES = {"自动", "关闭", "开启"};
constexpr std::array<std::string_view, 3> STABILIZE_MODES = {"关闭", "静态", "运动"};
constexpr std::array<std::string_view, 4> INPUT_EXPOSURES = {"自动", "游戏曝光", "测光", "手动"};
constexpr std::array<std::string_view, 2> AUTO_EXPOSURE_MODES = {"混合", "切换"};  // 2026-10-09
constexpr std::array<std::string_view, 2> ADAPT_MODES = {"关闭", "平滑"};
constexpr std::array<std::string_view, 4> PRIMARIES = {"自动", "BT.709", "BT.2020", "AP1 (ACEScg)"};
constexpr std::array<std::string_view, 2> NEURAL_TRANSFERS = {"有界比率", "一致"};
constexpr std::array<std::string_view, 3> PEDESTAL_MODES = {"关闭", "自动", "始终"};

using setting_flags::APPLY_ON_RELEASE;
using setting_flags::BOOKKEEPING;
using setting_flags::GREYED;
using setting_flags::NEXT_DLSS_CREATE;
using setting_flags::NEXT_START;
using setting_flags::NETWORK_PRESET;
using setting_flags::PASS_STAGE;
using setting_flags::PASS_SWITCH;
using setting_flags::RESTARTS_HISTORY;
using setting_flags::SLIDER_RANGE_ONLY;

template <class Enum>
double Index(Enum value) {
  return static_cast<double>(static_cast<uint32_t>(value));
}

template <class Enum>
Enum FromIndex(double value) {
  return static_cast<Enum>(static_cast<uint32_t>(value));
}

double Flag(bool value) {
  return (value ? 1.0 : 0.0);
}

constexpr std::string_view PRESET_TOOLTIP =
    "此质量模式（J-M）对应的 DLSS 渲染预设。游戏下次创建 DLSS 功能时生效。";

const auto BASE_ROWS = std::to_array<SettingDescriptor>({
    {
        .key = ENABLED_KEY,
        .kind = SettingKind::BOOL,
        .section = SettingSection::TOP,
        .label = "启用",
        .tooltip = "运行 DLSS-NR。关闭后，宽限期结束时会释放 NR 占用的全部显存。",
        .get = [](const Settings& s) { return Flag(s.enabled); },
        .set = [](Settings& s, double v) { s.enabled = (v != 0.0); },
    },
    {
        .key = "EnableKey",
        .kind = SettingKind::KEY,
        .section = SettingSection::HOTKEYS,
        .label = "启用快捷键",
        .tooltip = "切换“启用”状态。点击此处后按下一个按键；右键点击可清除快捷键。",
        .min = 0.0,
        .max = 255.0,
        .get = [](const Settings& s) { return static_cast<double>(s.enable_key); },
        .set = [](Settings& s, double v) { s.enable_key = static_cast<uint32_t>(v); },
    },
    {
        .key = "ResolutionMode",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::PLACEMENT,
        .label = "分辨率",
        .tooltip = "NR 的处理分辨率。分辨率越低，GPU 开销和显存占用越小，NR 的处理结果会升采样回原尺寸。“匹配游戏”会跟随游戏的 DLSS 渲染尺寸。"
                   "NR 的处理尺寸不会低于 " UPLIFT_NR_FLOOR_TEXT "。更改后会延迟半秒生效，并重置 NR 的时序历史。",
        .choices = RESOLUTION_MODES,
        .flags = RESTARTS_HISTORY,
        .get = [](const Settings& s) { return Index(s.resolution_mode); },
        .set = [](Settings& s, double v) { s.resolution_mode = FromIndex<ResolutionMode>(v); },
    },
    {
        .key = "Intensity",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LOOK,
        .label = "强度",
        .tooltip = "控制 NR 对画面的处理强度，范围为 0（关闭）至 1（最大）。如需更强效果，可提高“处理结果调整”中的“调整强度”。",
        .min = 0.0,
        .max = 10.0,
        .ui_max = 1.0,
        .get = [](const Settings& s) { return static_cast<double>(s.intensity); },
        .set = [](Settings& s, double v) { s.intensity = static_cast<float>(v); },
    },
    {
        .key = "Style",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::LOOK,
        .label = "风格",
        .tooltip = "选择处理风格 A/B/C（0/1/2），与“设置”中的 NR 渲染模型预设相互独立。更改后会重置 NR 的时序历史。",
        .choices = MODELS,
        .flags = RESTARTS_HISTORY,
        .get = [](const Settings& s) { return static_cast<double>(s.style); },
        .set = [](Settings& s, double v) { s.style = static_cast<uint32_t>(v); },
    },
    {
        .key = "LocalStructure",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LOOK,
        .label = "结构",
        .tooltip = "控制 NR 添加的精细结构。更改后会重置 NR 的时序历史。",
        .min = 0.0,
        .max = 1.0,
        .flags = RESTARTS_HISTORY,
        .get = [](const Settings& s) { return static_cast<double>(s.local_structure); },
        .set = [](Settings& s, double v) { s.local_structure = static_cast<float>(v); },
    },
    {
        .key = "LocalTone",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LOOK,
        .label = "局部色调",
        .tooltip = "控制 NR 添加的局部对比度。更改后会重置 NR 的时序历史。",
        .min = 0.0,
        .max = 1.0,
        .flags = RESTARTS_HISTORY,
        .get = [](const Settings& s) { return static_cast<double>(s.local_tone); },
        .set = [](Settings& s, double v) { s.local_tone = static_cast<float>(v); },
    },
    {
        .key = "AutoMask",
        .kind = SettingKind::BOOL,
        .section = SettingSection::LOOK,
        .label = "角色遮罩",
        .tooltip = "使用 NR 自带的蒙版保护角色。更改后会重置 NR 的时序历史。",
        .flags = RESTARTS_HISTORY,
        .get = [](const Settings& s) { return Flag(s.auto_mask); },
        .set = [](Settings& s, double v) { s.auto_mask = (v != 0.0); },
    },
    {
        .key = "SkinStructure",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LOOK,
        .label = "皮肤结构",
        .tooltip = "控制皮肤区域的结构强度；-1 表示跟随“结构”设置。更改后会重置 NR 的时序历史。",
        .min = 0.0,
        .max = 1.0,
        .special = static_cast<double>(SKIN_SAME_AS_STRUCTURE),
        .flags = RESTARTS_HISTORY,
        .get = [](const Settings& s) { return static_cast<double>(s.skin_structure); },
        .set = [](Settings& s, double v) { s.skin_structure = static_cast<float>(v); },
    },
    {
        .key = "KeepFaces",
        .kind = SettingKind::BOOL,
        .section = SettingSection::LOOK,
        .label = "保护面部",
        .tooltip = "保留 NR 对角色光照和阴影的处理，同时避免 NR 改变角色面部形状。为此每帧会额外运行一次 NR（增加少量 GPU 开销和显存占用），与层数无关。"
                   "如果不同层运行在多个阶段，此项保护第 1 层所在阶段；后续阶段可单独设置“保护面部”。",
        .get = [](const Settings& s) { return Flag(s.keep_faces); },
        .set = [](Settings& s, double v) { s.keep_faces = (v != 0.0); },
    },
    {
        .key = "FaceProtection",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LOOK,
        .label = "面部保护",
        .tooltip = "控制面部保留 NR 精细结构的程度：0 保留游戏原本的面部，1 则完全应用 NR 的结构变化。更改后会重置额外处理层的时序历史。",
        .min = 0.0,
        .max = 1.0,
        .flags = RESTARTS_HISTORY,
        .get = [](const Settings& s) { return static_cast<double>(s.face_protection); },
        .set = [](Settings& s, double v) { s.face_protection = static_cast<float>(v); },
    },
    {
        .key = "LightingScale",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LOOK,
        .label = "光照尺度（画面高度百分比）",
        .tooltip = "角色身上尺度大于此值的 NR 变化会被视为光照并保留；更细微的变化不会保留。数值越大，仅保留范围更广的光照变化；数值越小，面部保留的 NR 变化越多。",
        .min = 0.5,
        .max = 10.0,
        .get = [](const Settings& s) { return static_cast<double>(s.lighting_scale); },
        .set = [](Settings& s, double v) { s.lighting_scale = static_cast<float>(v); },
    },
    {
        .key = "Mask",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::MASK,
        .label = "NR 蒙版",
        .tooltip = "自动：如果某个效果写入名为 UPLIFT_MASK 的纹理，NR 会根据其红色通道限制处理范围：0 保留游戏画面，1 完全应用 NR，0.5 表示应用一半的 NR 变化。"
                   "使用上一帧的蒙版。Uplift 附带的 fx/UpliftMask.fx 可供参考。",
        .choices = MASK_MODES,
        .get = [](const Settings& s) { return Index(s.mask); },
        .set = [](Settings& s, double v) { s.mask = FromIndex<MaskMode>(v); },
    },
    {
        .key = "MaskBlendMode",
        .kind = SettingKind::UINT,
        .section = SettingSection::PLACEMENT,
        .label = "原生蒙版混合模式",
        .tooltip = "NR 310.9.16 的实验性选项，模式为 0–3，默认值为 1。模式 0 使用不同的蒙版组合方式，画面变化可能不明显。此项与 Uplift 的 NR 蒙版相互独立；NR 310.8 会忽略此项。"
                   "Ada 兼容运行时的完整蒙版组合行为仍在验证中。",
        .min = 0.0,
        .max = 3.0,
        .flags = RESTARTS_HISTORY,
        .get = [](const Settings& s) { return static_cast<double>(s.mask_blend_mode); },
        .set = [](Settings& s, double v) { s.mask_blend_mode = static_cast<uint32_t>(v); },
    },
    {
        .key = "UICorrection",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::MASK,
        .label = "UI 校正",
        .tooltip = "NR 的 UI 校正需要不含 HUD 的画面，而 Uplift 不会捕获这类画面：在“DLSS 后”阶段无需校正（HUD 会在 NR 之后绘制）；处理后处理画面时请使用 NR 蒙版。开启时仅会将此开关传递给运行时。",
        .choices = UI_CORRECTION_MODES,
        .get = [](const Settings& s) { return Index(s.ui_correction); },
        .set = [](Settings& s, double v) { s.ui_correction = FromIndex<UiCorrection>(v); },
    },
    {
        .key = "ShapeResult",
        .kind = SettingKind::BOOL,
        .section = SettingSection::RESULT,
        .label = "处理结果调整",
        .tooltip = "实时调整 NR 的处理结果，不会重置时序历史。关闭时保留 NR 的原始输出。若各层位于不同阶段，只调整最后阶段产生的变化；较早阶段的变化会原样保留。",
        .get = [](const Settings& s) { return Flag(s.look.enabled); },
        .set = [](Settings& s, double v) { s.look.enabled = (v != 0.0); },
    },
    {
        .key = "EditStrength",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::RESULT,
        .label = "调整强度",
        .tooltip = "0 表示移除 NR 的变化，1 表示保持原样，2 表示将其加倍；亮度以档位为单位，颜色以比例为单位。",
        .min = 0.0,
        .max = 2.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.edit_strength); },
        .set = [](Settings& s, double v) { s.look.shape.edit_strength = static_cast<float>(v); },
    },
    {
        .key = "Brighten",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::RESULT,
        .label = "提亮",
        .tooltip = "控制保留多少 NR 的提亮效果。",
        .min = 0.0,
        .max = 2.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.brighten); },
        .set = [](Settings& s, double v) { s.look.shape.brighten = static_cast<float>(v); },
    },
    {
        .key = "Darken",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::RESULT,
        .label = "压暗",
        .tooltip = "控制保留多少 NR 的压暗效果，包括其在高亮物体周围可能产生的暗色光晕。",
        .min = 0.0,
        .max = 2.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.darken); },
        .set = [](Settings& s, double v) { s.look.shape.darken = static_cast<float>(v); },
    },
    {
        .key = "ResultColor",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::RESULT,
        .label = "色彩",
        .tooltip = "控制 NR 将颜色向灰色靠近或远离的程度；0 表示保留游戏原有的饱和度。",
        .min = 0.0,
        .max = 2.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.color); },
        .set = [](Settings& s, double v) { s.look.shape.color = static_cast<float>(v); },
    },
    {
        .key = "HueShift",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::RESULT,
        .label = "色相偏移",
        .tooltip = "控制 NR 改变色相的程度；0 表示保留游戏原有的色相。",
        .min = 0.0,
        .max = 2.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.hue); },
        .set = [](Settings& s, double v) { s.look.shape.hue = static_cast<float>(v); },
    },
    {
        .key = "Shadows",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::RESULT,
        .label = "阴影",
        .tooltip = "调整 NR 在游戏画面暗部区域产生的变化。",
        .min = 0.0,
        .max = 2.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.shadows); },
        .set = [](Settings& s, double v) { s.look.shape.shadows = static_cast<float>(v); },
    },
    {
        .key = "Midtones",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::RESULT,
        .label = "中间调",
        .tooltip = "调整 NR 在游戏画面中间调区域产生的变化。",
        .min = 0.0,
        .max = 2.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.midtones); },
        .set = [](Settings& s, double v) { s.look.shape.midtones = static_cast<float>(v); },
    },
    {
        .key = "Highlights",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::RESULT,
        .label = "高光",
        .tooltip = "调整 NR 在游戏画面高亮区域产生的变化。",
        .min = 0.0,
        .max = 2.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.highlights); },
        .set = [](Settings& s, double v) { s.look.shape.highlights = static_cast<float>(v); },
    },
    {
        .key = "MaxBrighten",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LIMITS,
        .label = "最大提亮幅度（档）",
        .tooltip = "限制单个像素可被提亮的最大幅度；0 表示不限制。",
        .min = 0.0,
        .max = 3.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.max_brighten); },
        .set = [](Settings& s, double v) { s.look.shape.max_brighten = static_cast<float>(v); },
    },
    {
        .key = "MaxDarken",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LIMITS,
        .label = "最大压暗幅度（档）",
        .tooltip = "限制单个像素可被压暗的最大幅度；0 表示不限制。",
        .min = 0.0,
        .max = 3.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.max_darken); },
        .set = [](Settings& s, double v) { s.look.shape.max_darken = static_cast<float>(v); },
    },
    {
        .key = "MaxColorChange",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LIMITS,
        .label = "最大色彩变化",
        .tooltip = "限制颜色移动幅度，以归一化色度表示；0 表示不限制。",
        .min = 0.0,
        .max = 0.5,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.max_color); },
        .set = [](Settings& s, double v) { s.look.shape.max_color = static_cast<float>(v); },
    },
    {
        .key = "LargeScaleTone",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LIMITS,
        .label = "大范围色调",
        .tooltip = "调整 NR 造成的大范围光照变化。",
        .min = 0.0,
        .max = 2.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.tone); },
        .set = [](Settings& s, double v) { s.look.shape.tone = static_cast<float>(v); },
    },
    {
        .key = "FineDetail",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LIMITS,
        .label = "精细细节",
        .tooltip = "调整 NR 造成的小范围变化。",
        .min = 0.0,
        .max = 2.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.detail); },
        .set = [](Settings& s, double v) { s.look.shape.detail = static_cast<float>(v); },
    },
    {
        .key = "DetailRadius",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LIMITS,
        .label = "细节半径（画面高度百分比）",
        .tooltip = "控制大范围变化与精细细节的分界（相对于画面高度的比例），同时决定光晕抑制的作用范围。更改后会重置时序稳定的历史记录。",
        .min = 0.25,
        .max = 10.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.detail_radius); },
        .set = [](Settings& s, double v) { s.look.detail_radius = static_cast<float>(v); },
    },
    {
        .key = "HaloSuppression",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LIMITS,
        .label = "光晕抑制",
        .tooltip = "减少 NR 在明亮物体周围留下的暗环；不会影响表面上均匀的暗部变化，也不会影响暗物体旁的阴影。",
        .min = 0.0,
        .max = 1.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.shape.halo); },
        .set = [](Settings& s, double v) { s.look.shape.halo = static_cast<float>(v); },
    },
    {
        .key = "Stabilize",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::LIMITS,
        .label = "时序稳定",
        .tooltip = "平滑 NR 的大范围变化。“静态”适合静止画面；“运动”会跟随运动矢量，并在没有运动矢量且图像发生变化时丢弃历史信息。两种模式都会略微延迟光照变化。",
        .choices = STABILIZE_MODES,
        .get = [](const Settings& s) { return Index(s.look.stabilize); },
        .set = [](Settings& s, double v) { s.look.stabilize = FromIndex<look::StabilizeMode>(v); },
    },
    {
        .key = "StabilizeMs",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::LIMITS,
        .label = "稳定时间（毫秒）",
        .tooltip = "数值越大，画面越稳定，但跟随光照变化的速度越慢。",
        .min = 16.0,
        .max = 2000.0,
        .get = [](const Settings& s) { return static_cast<double>(s.look.stabilize_ms); },
        .set = [](Settings& s, double v) { s.look.stabilize_ms = static_cast<float>(v); },
    },
    {
        .key = "StabilizeDetail",
        .kind = SettingKind::BOOL,
        .section = SettingSection::LIMITS,
        .label = "稳定细节",
        .tooltip = "同时平滑 NR 造成的小范围变化。",
        .get = [](const Settings& s) { return Flag(s.look.stabilize_detail); },
        .set = [](Settings& s, double v) { s.look.stabilize_detail = (v != 0.0); },
    },
    {
        .key = "Source",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::PLACEMENT,
        .label = "NR 阶段",
        .tooltip = "NR 的运行阶段。“自动”：游戏启用 DLSS 后，在其后立即运行；否则处理后处理画面。需 ReShade 6.1 或更新版本才能选择“DLSS 后”。更改后立即生效。",
        .choices = SOURCES,
        .get = [](const Settings& s) { return Index(s.source); },
        .set = [](Settings& s, double v) { s.source = FromIndex<PlacementSource>(v); },
    },
    {
        .key = "PreUpscale",
        .kind = SettingKind::BOOL,
        .section = SettingSection::PLACEMENT,
        .label = "DLSS 前",
        .tooltip = "NR 先增强游戏原生渲染分辨率的画面，再由游戏的 DLSS 将结果升频。使用光线重建（Ray Reconstruction）时，NR 会处理其输入画面，在降噪和升频之前运行。"
                   "如果渲染分辨率低于 " UPLIFT_NR_FLOOR_TEXT "，画面会先补齐至该尺寸，因此 NR 不会在更低分辨率下运行。",
        .get = [](const Settings& s) { return Flag(s.pre_upscale); },
        .set = [](Settings& s, double v) { s.pre_upscale = (v != 0.0); },
    },
    {
        .key = "ResolutionScale",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::PLACEMENT,
        .label = "自定义比例（%）",
        .tooltip = "“分辨率”设为“自定义”时使用的处理尺寸，占输出分辨率的百分比。松开滑块后生效。",
        .min = 25.0,
        .max = 100.0,
        .flags = SLIDER_RANGE_ONLY,  // a share of the output: 100 % at most
        .get = [](const Settings& s) { return static_cast<double>(s.resolution_scale); },
        .set = [](Settings& s, double v) { s.resolution_scale = static_cast<float>(v); },
    },
    {
        .key = "Upsampling",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::PLACEMENT,
        .label = "升采样",
        .tooltip = "选择如何将低于全分辨率时的 NR 变化还原至原尺寸。“边缘感知”会沿 NR 处理后的边缘还原变化；“经典”使用双线性插值，开销略低。",
        .choices = UPSAMPLING_MODES,
        .get = [](const Settings& s) { return Index(s.upsampling); },
        .set = [](Settings& s, double v) { s.upsampling = FromIndex<color::Upsampling>(v); },
    },
    {
        // Split chain (2026-10-10, design 4): a later stage group's own size, keyed by stage so moving a boundary pass keeps each stage's size.
        .key = "ResolutionAfterDlss",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::PLACEMENT,
        .label = "DLSS 后的分辨率",
        .tooltip = "设置在“DLSS 后”阶段运行各层的处理分辨率。“设置”中的“分辨率”控制第 1 层所在阶段的分辨率。如果这些层开销过高，可调低此值。更改后会重置 NR。",
        .choices = RESOLUTION_MODES,
        .flags = RESTARTS_HISTORY,
        .get = [](const Settings& s) { return Index(s.resolution_mode_after_dlss); },
        .set = [](Settings& s, double v) { s.resolution_mode_after_dlss = FromIndex<ResolutionMode>(v); },
    },
    {
        .key = "ResolutionScaleAfterDlss",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::PLACEMENT,
        .label = "DLSS 后自定义比例（%）",
        .tooltip = "“DLSS 后的分辨率”设为“自定义”时使用的处理尺寸，占输出分辨率的百分比。松开滑块后生效。",
        .min = 25.0,
        .max = 100.0,
        .flags = SLIDER_RANGE_ONLY,  // a share of the output: 100 % at most
        .get = [](const Settings& s) { return static_cast<double>(s.resolution_scale_after_dlss); },
        .set = [](Settings& s, double v) { s.resolution_scale_after_dlss = static_cast<float>(v); },
    },
    {
        .key = "ResolutionPresent",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::PLACEMENT,
        .label = "后处理阶段分辨率",
        .tooltip = "设置在后处理阶段运行各层的处理分辨率。“设置”中的“分辨率”控制第 1 层所在阶段的分辨率。如果这些层开销过高，可调低此值。更改后会重置 NR。",
        .choices = RESOLUTION_MODES,
        .flags = RESTARTS_HISTORY,
        .get = [](const Settings& s) { return Index(s.resolution_mode_present); },
        .set = [](Settings& s, double v) { s.resolution_mode_present = FromIndex<ResolutionMode>(v); },
    },
    {
        .key = "ResolutionScalePresent",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::PLACEMENT,
        .label = "后处理阶段自定义比例（%）",
        .tooltip = "“后处理阶段分辨率”设为“自定义”时使用的处理尺寸，占输出分辨率的百分比。松开滑块后生效。",
        .min = 25.0,
        .max = 100.0,
        .flags = SLIDER_RANGE_ONLY,  // a share of the output: 100 % at most
        .get = [](const Settings& s) { return static_cast<double>(s.resolution_scale_present); },
        .set = [](Settings& s, double v) { s.resolution_scale_present = static_cast<float>(v); },
    },
    {
        // Split chain, owner's decision Q5a: a later stage group's own Keep faces, default off. Pass 1's group follows Keep faces itself.
        .key = "KeepFacesAfterDlss",
        .kind = SettingKind::BOOL,
        .section = SettingSection::PLACEMENT,
        .label = "在此阶段保护面部",
        .tooltip = "同时保护 DLSS 后运行的层中的角色面部。每帧会额外运行一次 NR。",
        .get = [](const Settings& s) { return Flag(s.keep_faces_after_dlss); },
        .set = [](Settings& s, double v) { s.keep_faces_after_dlss = (v != 0.0); },
    },
    {
        .key = "KeepFacesPresent",
        .kind = SettingKind::BOOL,
        .section = SettingSection::PLACEMENT,
        .label = "在此阶段保护面部",
        .tooltip = "同时保护后处理阶段运行的层中的角色面部。每帧会额外运行一次 NR。",
        .get = [](const Settings& s) { return Flag(s.keep_faces_present); },
        .set = [](Settings& s, double v) { s.keep_faces_present = (v != 0.0); },
    },
    {
        .key = "MotionVectors",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::PLACEMENT,
        .label = "运动矢量",
        .tooltip = "在游戏帧内运行（NR 阶段为“DLSS 后”或“DLSS 前”）时，“自动”和“DLSS”使用游戏 DLSS 提供的运动矢量；“无”则不使用。NR 在后处理阶段运行时，“自动”和“DLSS”会在 Uplift 能复制运动矢量时使用副本，否则不使用运动矢量。"
                   "“LaunchPad”使用 iMMERSE LaunchPad 提供的运动矢量：请启用 Uplift.fx 中的 Uplift 技术，并将其放在 LaunchPad 下方。“Lumenite”同理：启用 LumeniteFX 的 Kernel，并将 Uplift 放在其下方。“自动”会依次尝试 DLSS、LaunchPad 和 Lumenite。",
        .choices = MOTION_VECTORS,
        .get = [](const Settings& s) { return Index(s.motion_vectors); },
        .set = [](Settings& s, double v) { s.motion_vectors = FromIndex<MotionVectorSource>(v); },
    },
    {
        .key = "PassCount",
        .kind = SettingKind::UINT,
        .section = SettingSection::LOOK,
        .label = "层数",
        .tooltip = "每增加一层，NR 就会对上一层的输出再次处理，开销相当于多运行一层 NR。松开滑块后生效。",
        .min = 1.0,
        .max = 10.0,
        .flags = APPLY_ON_RELEASE,
        .get = [](const Settings& s) { return static_cast<double>(s.pass_count); },
        .set = [](Settings& s, double v) { s.pass_count = static_cast<uint32_t>(v); },
    },
    // User decision (fix round 1): the user does not want to change DLSS modes or presets from
    // Uplift -- it needs a DLSS re-create, and live switching is not practical -- so these 8 rows are
    // hidden. The keys stay readable from the ini and the code stays inert at the GAME defaults.
    {
        .key = "DLSSQualityMode",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::HIDDEN,
        .label = "DLSS 质量模式",
        .tooltip = "覆盖游戏创建 DLSS-SR 时使用的质量模式。游戏下次创建 DLSS 功能时生效；可能需要重启游戏或更改分辨率。",
        .choices = QUALITY_MODES,
        .flags = NEXT_DLSS_CREATE,
        .get = [](const Settings& s) { return Index(s.dlss_quality_mode); },
        .set = [](Settings& s, double v) { s.dlss_quality_mode = FromIndex<DlssQualityOverride>(v); },
    },
    {
        .key = "DLSSPresetDLAA",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::HIDDEN,
        .label = "DLAA 预设",
        .tooltip = PRESET_TOOLTIP,
        .choices = PRESETS,
        .flags = NEXT_DLSS_CREATE,
        .get = [](const Settings& s) { return Index(s.dlss_presets[0]); },
        .set = [](Settings& s, double v) { s.dlss_presets[0] = FromIndex<DlssPresetOverride>(v); },
    },
    {
        .key = "DLSSPresetUltraQuality",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::HIDDEN,
        .label = "超高质量预设",
        .tooltip = PRESET_TOOLTIP,
        .choices = PRESETS,
        .flags = NEXT_DLSS_CREATE,
        .get = [](const Settings& s) { return Index(s.dlss_presets[1]); },
        .set = [](Settings& s, double v) { s.dlss_presets[1] = FromIndex<DlssPresetOverride>(v); },
    },
    {
        .key = "DLSSPresetQuality",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::HIDDEN,
        .label = "质量预设",
        .tooltip = PRESET_TOOLTIP,
        .choices = PRESETS,
        .flags = NEXT_DLSS_CREATE,
        .get = [](const Settings& s) { return Index(s.dlss_presets[2]); },
        .set = [](Settings& s, double v) { s.dlss_presets[2] = FromIndex<DlssPresetOverride>(v); },
    },
    {
        .key = "DLSSPresetBalanced",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::HIDDEN,
        .label = "均衡预设",
        .tooltip = PRESET_TOOLTIP,
        .choices = PRESETS,
        .flags = NEXT_DLSS_CREATE,
        .get = [](const Settings& s) { return Index(s.dlss_presets[3]); },
        .set = [](Settings& s, double v) { s.dlss_presets[3] = FromIndex<DlssPresetOverride>(v); },
    },
    {
        .key = "DLSSPresetPerformance",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::HIDDEN,
        .label = "性能预设",
        .tooltip = PRESET_TOOLTIP,
        .choices = PRESETS,
        .flags = NEXT_DLSS_CREATE,
        .get = [](const Settings& s) { return Index(s.dlss_presets[4]); },
        .set = [](Settings& s, double v) { s.dlss_presets[4] = FromIndex<DlssPresetOverride>(v); },
    },
    {
        .key = "DLSSPresetUltraPerformance",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::HIDDEN,
        .label = "超高性能预设",
        .tooltip = PRESET_TOOLTIP,
        .choices = PRESETS,
        .flags = NEXT_DLSS_CREATE,
        .get = [](const Settings& s) { return Index(s.dlss_presets[5]); },
        .set = [](Settings& s, double v) { s.dlss_presets[5] = FromIndex<DlssPresetOverride>(v); },
    },
    {
        .key = "DLSSAutoExposure",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::HIDDEN,
        .label = "DLSS 自动曝光",
        .tooltip = "强制开启或关闭 DLSS-SR 的自动曝光。游戏下次创建 DLSS 功能时生效。",
        .choices = AUTO_EXPOSURE,
        .flags = NEXT_DLSS_CREATE,
        .get = [](const Settings& s) { return Index(s.dlss_auto_exposure); },
        .set = [](Settings& s, double v) { s.dlss_auto_exposure = FromIndex<AutoExposureOverride>(v); },
    },
    {
        .key = "PresentWithFrameGen",
        .kind = SettingKind::BOOL,
        .section = SettingSection::ADVANCED,
        .label = "帧生成期间处理后处理",
        .tooltip = "允许帧生成开启时在后处理阶段运行 NR：每个后处理帧（包括生成帧）都会处理，开销至少翻倍。生成帧上的处理结果不作保证。",
        .get = [](const Settings& s) { return Flag(s.present_with_frame_gen); },
        .set = [](Settings& s, double v) { s.present_with_frame_gen = (v != 0.0); },
    },
    {
        .key = "Encoding",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::FIXES_COLOR,
        .label = "编码格式",
        .tooltip = "选择画面的编码格式。“自动”会跟随交换链；“DLSS 后”会读取 DLSS 的 HDR 标志。",
        .choices = ENCODINGS,
        .get = [](const Settings& s) { return Index(s.encoding); },
        .set = [](Settings& s, double v) { s.encoding = FromIndex<color::Encoding>(v); },
    },
    {
        .key = "DiffuseWhiteNits",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::FIXES_COLOR,
        .label = "漫反射白（尼特）",
        .tooltip = "NR 将此亮度视为纸白。数值越高，NR 接收到的输入越暗，可用亮度余量越大。",
        .min = 48.0,
        .max = 500.0,
        .special = 0.0,
        .get = [](const Settings& s) { return static_cast<double>(s.diffuse_white_nits); },
        .set = [](Settings& s, double v) { s.diffuse_white_nits = static_cast<float>(v); },
    },
    {
        .key = "TransferStrength",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::FIXES_COLOR,
        .label = "亮度传递强度",
        .tooltip = "控制 NR 造成的亮度变化有多少会应用到画面。",
        .min = 0.0,
        .max = 1.0,
        .get = [](const Settings& s) { return static_cast<double>(s.transfer_strength); },
        .set = [](Settings& s, double v) { s.transfer_strength = static_cast<float>(v); },
    },
    {
        .key = "ColorStrength",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::FIXES_COLOR,
        .label = "色彩传递强度",
        .tooltip = "控制 NR 造成的色彩变化有多少会应用到画面。",
        .min = 0.0,
        .max = 1.0,
        .get = [](const Settings& s) { return static_cast<double>(s.color_strength); },
        .set = [](Settings& s, double v) { s.color_strength = static_cast<float>(v); },
    },
    {
        .key = "InputExposure",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::FIXES_COLOR,
        .label = "输入曝光",
        .tooltip = "设置场景线性画面中 NR 输入亮度的来源。“自动”：优先使用游戏 DLSS 曝光；若该曝光看起来正常，则与 Uplift 测光值混合，否则使用测光值（实际来源见“详细信息”）。"
                   "“游戏曝光”：使用 DLSS 曝光（后处理画面不提供此值）；“测光”：使用 Uplift 自带的测光；“手动”：仅使用“漫反射白”设置。sRGB、scRGB 和 HDR10 画面不会进行测光。",
        .choices = INPUT_EXPOSURES,
        .get = [](const Settings& s) { return Index(s.input_exposure); },
        .set = [](Settings& s, double v) { s.input_exposure = FromIndex<color::InputExposure>(v); },
    },
    {
        // 2026-10-09 (.superpowers/sdd/2026-10-09-auto-exposure): shown only while Input exposure is Auto.
        .key = "AutoExposureMode",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::FIXES_COLOR,
        .label = "自动曝光",
        .tooltip = "自动模式下的曝光处理方式。“混合”会按下方滑块混合游戏曝光和 Uplift 测光值；“切换”则使用游戏自身曝光。若游戏曝光异常，始终回退到测光值。",
        .choices = AUTO_EXPOSURE_MODES,
        .get = [](const Settings& s) { return Index(s.auto_exposure_mode); },
        .set = [](Settings& s, double v) { s.auto_exposure_mode = FromIndex<color::AutoExposureMode>(v); },
    },
    {
        // 2026-10-09: shown only for Auto's Blend. Stored 0..1, shown as a percentage.
        .key = "AutoExposureBlend",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::FIXES_COLOR,
        .label = "混合比例（游戏曝光至测光值）",
        .tooltip = "0% 表示使用游戏曝光，100% 表示使用 Uplift 测光值。数值越低越接近游戏原有观感；数值越高，不同场景间的曝光越平稳。若游戏曝光异常，始终回退到测光值。",
        .min = 0.0,
        .max = 1.0,
        .flags = SLIDER_RANGE_ONLY,  // a blend: 100 % is the meter alone
        .get = [](const Settings& s) { return static_cast<double>(s.auto_exposure_blend); },
        .set = [](Settings& s, double v) { s.auto_exposure_blend = static_cast<float>(v); },
    },
    {
        .key = "ExposureAdapt",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::FIXES_COLOR,
        .label = "曝光适应",
        .tooltip = "在使用测光值时生效（“测光”或“自动”模式）。“平滑”会按照下方速率调整，并设置较小的死区；“关闭”则逐帧跟随变化。",
        .choices = ADAPT_MODES,
        .get = [](const Settings& s) { return Index(s.exposure_adapt); },
        .set = [](Settings& s, double v) { s.exposure_adapt = FromIndex<ExposureAdapt>(v); },
    },
    {
        .key = "AdaptBrighterStops",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::FIXES_COLOR,
        .label = "适应更亮场景（档/秒）",
        .tooltip = "在使用测光值时生效（“测光”或“自动”模式）。",
        .min = 0.1,
        .max = 20.0,
        .get = [](const Settings& s) { return static_cast<double>(s.adapt_brighter_stops); },
        .set = [](Settings& s, double v) { s.adapt_brighter_stops = static_cast<float>(v); },
    },
    {
        .key = "AdaptDarkerStops",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::FIXES_COLOR,
        .label = "适应更暗场景（档/秒）",
        .tooltip = "在使用测光值时生效（“测光”或“自动”模式）。",
        .min = 0.1,
        .max = 20.0,
        .get = [](const Settings& s) { return static_cast<double>(s.adapt_darker_stops); },
        .set = [](Settings& s, double v) { s.adapt_darker_stops = static_cast<float>(v); },
    },
    {
        .key = "LinearUnitNits",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::FIXES_COLOR,
        .label = "线性单位（尼特）",
        .tooltip = "线性或 scRGB 图像每单位值（1.0）对应的亮度（尼特）；设为 0 时自动选择（线性 BT.709 为 100，scRGB 为 80）。点击重置箭头可恢复自动。",
        .min = 1.0,
        .max = 10000.0,
        .special = 0.0,
        .get = [](const Settings& s) { return static_cast<double>(s.linear_unit_nits); },
        .set = [](Settings& s, double v) { s.linear_unit_nits = static_cast<float>(v); },
    },
    {
        .key = "SourcePrimaries",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::FIXES_COLOR,
        .label = "色彩原色",
        .tooltip = "输入图像的色彩原色会先转换为 BT.709 供 NR 处理，再转换回原格式。“自动”会对普通画面使用 BT.709，对 HDR10 使用 BT.2020。",
        .choices = PRIMARIES,
        .get = [](const Settings& s) { return Index(s.source_primaries); },
        .set = [](Settings& s, double v) { s.source_primaries = FromIndex<color::Primaries>(v); },
    },
    {
        .key = "NeuralTransfer",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::FIXES_COLOR,
        .label = "神经渲染传递方式",
        .tooltip = "控制 NR 的处理结果如何还原到 HDR 画面。“一致”会补偿代理曲线对高光的压缩，最高可补偿 4 倍。",
        .choices = NEURAL_TRANSFERS,
        .get = [](const Settings& s) { return Index(s.neural_transfer); },
        .set = [](Settings& s, double v) { s.neural_transfer = FromIndex<color::NeuralTransfer>(v); },
    },
    {
        .key = "ChromaClamp",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::FIXES_COLOR,
        .label = "色度限制（档）",
        .tooltip = "限制各颜色通道的变化幅度，使其不超过亮度变化对应的档数；0 表示关闭。",
        .min = 0.0,
        .max = 3.0,
        .get = [](const Settings& s) { return static_cast<double>(s.chroma_clamp); },
        .set = [](Settings& s, double v) { s.chroma_clamp = static_cast<float>(v); },
    },
    {
        .key = "NearBlackGuard",
        .kind = SettingKind::BOOL,
        .section = SettingSection::FIXES_COLOR,
        .label = "近黑区域色彩保护",
        .tooltip = "在接近黑色的区域逐渐减弱 NR 的色彩变化，避免噪点变成绿色或灰色斑点。",
        .get = [](const Settings& s) { return Flag(s.near_black_guard); },
        .set = [](Settings& s, double v) { s.near_black_guard = (v != 0.0); },
    },
    {
        .key = "PedestalRemoval",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::HIDDEN,
        .label = "暗部基底移除",
        .tooltip = "目前无需启用：Uplift 的 HDR 传递会按比例应用 NR 的变化，因此黑色仍保持为黑色。此选项会在游戏中、构建流程开始前进行检查。",
        .choices = PEDESTAL_MODES,
        .flags = GREYED,
        .get = [](const Settings& s) { return Index(s.pedestal_removal); },
        .set = [](Settings& s, double v) { s.pedestal_removal = FromIndex<PedestalRemoval>(v); },
    },
    {
        .key = "MotionScaleX",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::FIXES_GUIDES,
        .label = "运动矢量 X 缩放",
        .tooltip = "缩放提供给 NR 的水平运动矢量；负值会反转该轴。",
        .min = -4.0,
        .max = 4.0,
        .get = [](const Settings& s) { return static_cast<double>(s.motion_scale_x); },
        .set = [](Settings& s, double v) { s.motion_scale_x = static_cast<float>(v); },
    },
    {
        .key = "MotionScaleY",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::FIXES_GUIDES,
        .label = "运动矢量 Y 缩放",
        .tooltip = "缩放提供给 NR 的垂直运动矢量；负值会反转该轴。",
        .min = -4.0,
        .max = 4.0,
        .get = [](const Settings& s) { return static_cast<double>(s.motion_scale_y); },
        .set = [](Settings& s, double v) { s.motion_scale_y = static_cast<float>(v); },
    },
    {
        .key = "DepthDirection",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::HIDDEN,
        .label = "深度方向（NR 310.8 中无效）",
        .tooltip = "作为 DLSSNR.DepthInverted 传递。NR 310.8 会读取深度信息，但不会使用。",
        .choices = DEPTH_DIRECTIONS,
        .flags = GREYED,
        .get = [](const Settings& s) { return Index(s.depth_direction); },
        .set = [](Settings& s, double v) { s.depth_direction = FromIndex<DepthDirection>(v); },
    },
    {
        .key = "ChainedHistory",
        .kind = SettingKind::BOOL,
        .section = SettingSection::FIXES_GUIDES,
        .label = "串联时序历史",
        .tooltip = "关闭后，每帧都会重置第 2 层及后续层的历史信息（仅用于诊断）。",
        .get = [](const Settings& s) { return Flag(s.chained_history); },
        .set = [](Settings& s, double v) { s.chained_history = (v != 0.0); },
    },
    {
        .key = "StateRestore",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::FIXES_GUIDES,
        .label = "NR 运行后的游戏状态",
        .tooltip = "NR 在游戏帧内运行后需要恢复游戏的图形状态。“完整”会重放游戏的计算状态；“最小”仅恢复描述符堆和根签名。",
        .choices = STATE_RESTORE_MODES,
        .get = [](const Settings& s) { return Index(s.state_restore); },
        .set = [](Settings& s, double v) { s.state_restore = FromIndex<StateRestore>(v); },
    },
    {
        .key = "GlobalTone",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::HIDDEN,
        .label = "全局色调强度",
        .tooltip = "此值会被保存并传递，但 NR 310.8 不会读取。",
        .min = 0.0,
        .max = 1.0,
        .flags = GREYED,
        .get = [](const Settings& s) { return static_cast<double>(s.global_tone); },
        .set = [](Settings& s, double v) { s.global_tone = static_cast<float>(v); },
    },
    {
        .key = "Preset",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::PLACEMENT,
        .label = "NR 渲染模型",
        .tooltip = "NR 310.9.16：预设 A（1）选择旧版 NR 渲染模型（Legacy）；预设 B（2）选择小型 FP8 NR 渲染模型（Small FP8）。“运行时默认”会使用 B。此 DLL 不包含 C–G 的权重。"
                   "更改 NR 渲染模型会重新创建 NR 并重置时序历史。“详细信息”会显示运行时确认的 NR 渲染模型名称和已记录的评估次数。NR 310.8 始终使用旧版 NR 渲染模型（Legacy）。此项与“外观”中的风格 A/B/C 相互独立。",
        .choices = NR_NETWORKS,
        .flags = RESTARTS_HISTORY | NETWORK_PRESET,
        .get = [](const Settings& s) { return static_cast<double>(s.preset); },
        .set = [](Settings& s, double v) { s.preset = static_cast<uint32_t>(v); },
    },
    {
        .key = "Performance",
        .kind = SettingKind::UINT,
        .section = SettingSection::HIDDEN,
        .label = "性能",
        .tooltip = "此值会被保存并传递，但在 NR 310.8 中无效。",
        .min = 1.0,
        .max = 6.0,
        .flags = GREYED,
        .get = [](const Settings& s) { return static_cast<double>(s.performance); },
        .set = [](Settings& s, double v) { s.performance = static_cast<uint32_t>(v); },
    },
    {
        .key = "GraceSeconds",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::ADVANCED,
        .label = "显存保留时间（秒）",
        .tooltip = "关闭 NR 后保留其显存的时长，避免短暂切换开关时重新加载。",
        .min = 0.0,
        .max = 60.0,
        .get = [](const Settings& s) { return static_cast<double>(s.grace_seconds); },
        .set = [](Settings& s, double v) { s.grace_seconds = static_cast<float>(v); },
    },
    {
        .key = "BudgetMarginMB",
        .kind = SettingKind::UINT,
        .section = SettingSection::ADVANCED,
        .label = "显存余量（MiB，0 = 自动）",
        .tooltip = "NR 始终为游戏预留的显存。0 表示自动，默认预留 512 MiB。",
        .min = 0.0,
        .max = 65536.0,
        .get = [](const Settings& s) { return static_cast<double>(s.budget_margin_mb); },
        .set = [](Settings& s, double v) { s.budget_margin_mb = static_cast<uint32_t>(v); },
    },
    {
        // 2026-10-09 (owner): a Budget input, applied at once with no NR reload.
        .key = "VramCheck",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::ADVANCED,
        .label = "显存检查",
        .tooltip = "“保守”会为游戏保留其显存预算，并在游戏需要显存时暂停 NR。“宽松”允许 NR 超出游戏预算约 1 GB。“关闭”则不检查，与其他 DLSS-NR 插件相同。"
                   "如果游戏确实占满显存，“宽松”和“关闭”可能导致卡顿。",
        .choices = VRAM_CHECKS,
        .get = [](const Settings& s) { return Index(s.vram_check); },
        .set = [](Settings& s, double v) { s.vram_check = FromIndex<nr::VramCheck>(v); },
    },
    {
        .key = "AutoResume",
        .kind = SettingKind::BOOL,
        .section = SettingSection::ADVANCED,
        .label = "显存释放后恢复",
        .tooltip = "当 NR 因游戏需要显存而暂停后，显存有空余时自动恢复。",
        .get = [](const Settings& s) { return Flag(s.auto_resume); },
        .set = [](Settings& s, double v) { s.auto_resume = (v != 0.0); },
    },
    {
        .key = "ShowNvIndicator",
        .kind = SettingKind::BOOL,
        .section = SettingSection::ADVANCED,
        .label = "显示 NVIDIA 指示器（下次加载时生效）",
        .tooltip = "显示 NVIDIA 的屏幕内 NR 指示器。",
        .get = [](const Settings& s) { return Flag(s.show_nv_indicator); },
        .set = [](Settings& s, double v) { s.show_nv_indicator = (v != 0.0); },
    },
    {
        .key = "ShowFaceMask",
        .kind = SettingKind::BOOL,
        .section = SettingSection::ADVANCED,
        .label = "显示面部蒙版",
        .tooltip = "将“面部保护”识别并保护的区域染成洋红色，便于调试蒙版。关闭后即可查看实际效果。",
        .get = [](const Settings& s) { return Flag(s.show_face_mask); },
        .set = [](Settings& s, double v) { s.show_face_mask = (v != 0.0); },
    },
    // Keep faces fix round 1 addendum (the owner's request): the face mask's tuning, live in game. Shader constants only: none restarts NR or the extra run.
    {
        .key = "FaceMaskThreshold",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::ADVANCED,
        .label = "蒙版阈值（亮度百分比）",
        .tooltip = "像素亮度相对变化仍视为噪声的阈值。若背景或高亮反光处出现零散蒙版，请调高；若面部着色太淡，请调低。",
        .min = 0.0,
        .max = 10.0,
        .get = [](const Settings& s) { return static_cast<double>(s.face_tuning.threshold); },
        .set = [](Settings& s, double v) { s.face_tuning.threshold = static_cast<float>(v); },
    },
    {
        .key = "FaceMaskFull",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::ADVANCED,
        .label = "完全保护阈值（亮度百分比）",
        .tooltip = "像素变化达到此阈值后，会被视为完整的面部区域。若面部着色不完整，请调低；若头发或衣物也被着色，请调高。此值始终高于“蒙版阈值”。",
        .min = 1.0,
        .max = 30.0,
        .get = [](const Settings& s) { return static_cast<double>(s.face_tuning.full); },
        .set = [](Settings& s, double v) { s.face_tuning.full = static_cast<float>(v); },
    },
    {
        .key = "FaceMaskStrength",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::ADVANCED,
        .label = "蒙版强度",
        .tooltip = "如果面部内部的蒙版有缺口，请调高；如果蒙版扩散到面部以外，请调低。",
        .min = 0.5,
        .max = 4.0,
        .get = [](const Settings& s) { return static_cast<double>(s.face_tuning.strength); },
        .set = [](Settings& s, double v) { s.face_tuning.strength = static_cast<float>(v); },
    },
    {
        .key = "FaceMaskSoftness",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::ADVANCED,
        .label = "蒙版柔和度（画面高度百分比）",
        .tooltip = "如果蒙版呈斑驳状或闪烁，请调高；如果蒙版扩散到面部以外，请调低。",
        .min = 0.1,
        .max = 2.0,
        .get = [](const Settings& s) { return static_cast<double>(s.face_tuning.softness); },
        .set = [](Settings& s, double v) { s.face_tuning.softness = static_cast<float>(v); },
    },
    {
        .key = "FaceEdgeFalloff",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::ADVANCED,
        .label = "边缘衰减（画面高度百分比）",
        .tooltip = "如果光晕越过角色轮廓，请调低；如果轮廓边缘出现暗边，请调高。",
        .min = 0.05,
        .max = 1.0,
        .get = [](const Settings& s) { return static_cast<double>(s.face_tuning.edge_falloff); },
        .set = [](Settings& s, double v) { s.face_tuning.edge_falloff = static_cast<float>(v); },
    },
    {
        // Fix round 3 (the owner's glowing spots): lighting comes back only where the mask is dense at the Lighting scale.
        .key = "FaceLightingDensity",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::ADVANCED,
        .label = "光照密度",
        .tooltip = "只有蒙版密度达到此值的区域，才会完整恢复 NR 的光照。若高光或明亮边缘使角色出现亮点，请调高；若面部边缘附近的光照消失，请调低。",
        .min = 0.05,
        .max = 1.0,
        .get = [](const Settings& s) { return static_cast<double>(s.face_tuning.density); },
        .set = [](Settings& s, double v) { s.face_tuning.density = static_cast<float>(v); },
    },
    // Round 5 (holes in the face mask on shadowed skin): shader constants only.
    {
        .key = "FaceFillSkin",
        .kind = SettingKind::BOOL,
        .section = SettingSection::ADVANCED,
        .label = "按颜色填充皮肤",
        .tooltip = "将面部蒙版扩展到 NR 变化未能标记的邻近同色皮肤（例如阴影中的皮肤），避免后续层再次改变这些区域。若肤色相近的衣物或头发变成洋红色，请关闭。",
        .get = [](const Settings& s) { return Flag(s.face_tuning.fill_skin); },
        .set = [](Settings& s, double v) { s.face_tuning.fill_skin = (v != 0.0); },
    },
    {
        .key = "FaceFillRadius",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::ADVANCED,
        .label = "填充半径（画面高度百分比）",
        .tooltip = "控制填充从已标记皮肤向外扩展的距离。大片皮肤仍未覆盖时请调高；填充扩散到邻近表面时请调低。",
        .min = 0.5,
        .max = 10.0,
        .get = [](const Settings& s) { return static_cast<double>(s.face_tuning.fill_radius); },
        .set = [](Settings& s, double v) { s.face_tuning.fill_radius = static_cast<float>(v); },
    },
    {
        .key = "FaceFillTolerance",
        .kind = SettingKind::FLOAT,
        .section = SettingSection::ADVANCED,
        .label = "颜色容差",
        .tooltip = "控制邻近皮肤颜色与待填充像素之间允许的最大色差。阴影中的皮肤仍未覆盖时请调高；衣物或头发变成洋红色时请调低。",
        .min = 0.005,
        .max = 0.2,
        .get = [](const Settings& s) { return static_cast<double>(s.face_tuning.fill_tolerance); },
        .set = [](Settings& s, double v) { s.face_tuning.fill_tolerance = static_cast<float>(v); },
    },
    {
        // Round 5: replaces round 4's FaceSpeckFilter switch (test builds only: no migration).
        .key = "FaceSpeckSize",
        .kind = SettingKind::UINT,
        .section = SettingSection::ADVANCED,
        .label = "移除亮点（像素）",
        .tooltip = "不超过此尺寸的亮点（反光、闪光）会保留 NR 的常规处理结果，而不应用保护效果。值越大，也越容易停止保护该尺寸的微小面部细节（如眼睛高光或远处的人眼）。0 表示关闭。",
        .min = 0.0,
        .max = 3.0,
        .get = [](const Settings& s) { return static_cast<double>(s.face_tuning.speck_size); },
        .set = [](Settings& s, double v) { s.face_tuning.speck_size = static_cast<uint32_t>(v); },
    },
    {
        .key = "LogLevel",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::ADVANCED,
        .label = "日志级别",
        .tooltip = "设置 Uplift 写入 ReShade.log 的信息量。",
        .choices = LOG_LEVELS,
        .get = [](const Settings& s) { return Index(s.log_level); },
        .set = [](Settings& s, double v) { s.log_level = FromIndex<nr::LogLevel>(v); },
    },
    {
        .key = "SnippetPath",
        .kind = SettingKind::TEXT,
        .section = SettingSection::ADVANCED,
        .label = "运行时路径（下次启动游戏时生效）",
        .tooltip = "指定 nvngx_dlssnr.dll 的路径。留空时会先在 gitc-uplift.addon64 所在目录查找，再在游戏目录查找。",
        .flags = NEXT_START,
        .get_text = [](const Settings& s) { return std::string_view(s.snippet_path); },
        .set_text = [](Settings& s, std::string_view v) { s.snippet_path = std::string(v); },
    },
    {
        .key = "NgxHooks",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::ADVANCED,
        .label = "NGX 挂钩（下次启动游戏时生效）",
        .tooltip = "关闭即安全模式：不启用任何挂钩，NR 仅能在后处理阶段运行。",
        .choices = NGX_HOOK_MODES,
        .flags = NEXT_START,
        .get = [](const Settings& s) { return Index(s.ngx_hooks); },
        .set = [](Settings& s, double v) { s.ngx_hooks = FromIndex<NgxHooksMode>(v); },
    },
    {
        .key = "ForeignNr",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::ADVANCED,
        .label = "其他 NR 生产者运行时",
        .tooltip = "“让出”会在其他工具为此游戏运行 NR 时暂停；“观察”则继续运行。",
        .choices = FOREIGN_NR_MODES,
        .get = [](const Settings& s) { return Index(s.foreign_nr); },
        .set = [](Settings& s, double v) { s.foreign_nr = FromIndex<ForeignNrMode>(v); },
    },
    {
        // Plan 19 (the owner's decision): native by default, the private Direct3D 12 device as the fallback and as a choice. T5: Uplift's helper process
        // after both, and as a choice.
        .key = "VulkanNr",
        .kind = SettingKind::CHOICE,
        .section = SettingSection::ADVANCED,
        .label = "Vulkan 后处理阶段 NR",
        .tooltip = "仅适用于 64 位 Vulkan 游戏。“原生”会在游戏自己的 Vulkan 设备上运行 NR；无法运行时会依次回退到独立的 Direct3D 12 设备和 Uplift 辅助进程，原因会显示在“详情”中。“Direct3D 12”从独立设备开始；“辅助进程”始终通过 Uplift 辅助进程运行 NR。"
                   "只有“原生”能在游戏的 DLSS 流程内运行 NR；选择“Direct3D 12”或“辅助进程”后，NR 会改在后处理阶段运行，因此“DLSS 后”或“DLSS 前”会随之移至后处理。只有游戏启动时选择“原生”，Uplift 才会向游戏设备添加 NGX 扩展。"
                   "原生 NR 首次运行后，驱动会在游戏退出前为游戏设备保留约 0.6 GB 显存。",
        .choices = VULKAN_NR_MODES,
        .get = [](const Settings& s) { return Index(s.vulkan_nr); },
        .set = [](Settings& s, double v) { s.vulkan_nr = FromIndex<VulkanNrMode>(v); },
    },
    {
        .key = "AutoRetry",
        .kind = SettingKind::BOOL,
        .section = SettingSection::ADVANCED,
        .label = "失败后自动重试",
        .tooltip = "NR 失败后会分别等待 1、2、4、8、16 和 30 秒后重试；之后将等待设置更改或手动点击“立即重试”。",
        .get = [](const Settings& s) { return Flag(s.auto_retry); },
        .set = [](Settings& s, double v) { s.auto_retry = (v != 0.0); },
    },
    {
        .key = "UseD3D9Ex",
        .kind = SettingKind::BOOL,
        .section = SettingSection::ADVANCED,
        .label = "使用 Direct3D 9Ex（速度更快；需重启游戏）",
        .tooltip = "请 ReShade 将游戏的 Direct3D 9 设备创建为 9Ex，使帧可通过 GPU 传给 NR。部分游戏无法正常使用 9Ex；如果游戏因此无法启动，Uplift 会自动关闭此选项。",
        .flags = NEXT_START,
        .get = [](const Settings& s) { return Flag(s.use_d3d9ex); },
        .set = [](Settings& s, double v) { s.use_d3d9ex = (v != 0.0); },
    },
    {
        .key = "AdjustVulkanDevices",
        .kind = SettingKind::BOOL,
        .section = SettingSection::HIDDEN,
        .label = "游戏创建 Vulkan 设备时进行调整（需重启游戏）",
        .tooltip = "设为 1 时，Uplift 会在游戏创建 Vulkan 设备时添加 NR 所需的共享扩展；设为 0 时保留游戏原始设备，NR 将使用 CPU 同步。若上次启用此项后游戏未能进入第一帧，Uplift 会自动将其设为 0。",
        .flags = NEXT_START,
        .get = [](const Settings& s) { return Flag(s.adjust_vulkan_devices); },
        .set = [](Settings& s, double v) { s.adjust_vulkan_devices = (v != 0.0); },
    },
    {
        // Plan 19 T1b: the pending marker's first tier. Never in the panel, like AdjustVulkanDevices.
        .key = "AdjustVulkanDevicesForNgx",
        .kind = SettingKind::BOOL,
        .section = SettingSection::HIDDEN,
        .label = "游戏创建 Vulkan 设备时添加 NGX 扩展（需重启游戏）",
        .tooltip = "适用于 64 位 Vulkan 游戏，且需启用 AdjustVulkanDevices。设为 1 时，也会添加 NGX 在游戏设备上原生运行 NR 所需的扩展和 bufferDeviceAddress；设为 0 时不添加这些扩展，后处理阶段的 NR 会使用 Direct3D 12 路径。"
                   "若上次启用这些扩展后游戏未能进入第一帧，Uplift 会自动将此项设为 0。",
        .flags = NEXT_START,
        .get = [](const Settings& s) { return Flag(s.adjust_vulkan_devices_for_ngx); },
        .set = [](Settings& s, double v) { s.adjust_vulkan_devices_for_ngx = (v != 0.0); },
    },
    {
        // Plan 19 T6 (I-4): native NR at Present's latch. Never in the panel, like AdjustVulkanDevicesForNgx.
        .key = "VulkanNativeNr",
        .kind = SettingKind::BOOL,
        .section = SettingSection::HIDDEN,
        .label = "允许 Vulkan 后处理阶段原生运行 NR（需重启游戏）",
        .tooltip = "适用于 64 位 Vulkan 游戏。设为 1 时，如果“Vulkan 后处理阶段 NR”允许，便可在游戏自己的 Vulkan 设备上原生运行；设为 0 时会从下一种方式开始尝试。如果上次启用原生 NR 后，游戏在其运行数秒前就启动失败，或运行期间设备丢失，Uplift 会自动将此项设为 0。",
        .flags = NEXT_START,
        .get = [](const Settings& s) { return Flag(s.vulkan_native_nr); },
        .set = [](Settings& s, double v) { s.vulkan_native_nr = (v != 0.0); },
    },
    {
        // Plan 17: diagnostic only, never in the panel. It exercises Retry now and the 2-strike rule without a real hang.
        .key = "DiagnosticRemoveDevice",
        .kind = SettingKind::UINT,
        .section = SettingSection::HIDDEN,
        .label = "诊断：运行指定帧数后移除 Uplift 独立设备",
        .tooltip = "仅供诊断。N > 0 时，NR 在 Uplift 自有的独立 Direct3D 12 设备上运行 N 帧后，将移除此设备（ID3D12Device5::RemoveDevice；不会移除游戏设备，因此 Direct3D 12 游戏不受影响），并在每次可能重试的停止后约一秒自动点击“立即重试”。0（默认值）表示关闭。",
        .min = 0.0,
        .max = 1000000.0,
        .get = [](const Settings& s) { return static_cast<double>(s.diagnostic_remove_device); },
        .set = [](Settings& s, double v) { s.diagnostic_remove_device = static_cast<uint32_t>(v); },
    },
    {
        .key = "DlssPlacementBlocked",
        .kind = SettingKind::BOOL,
        .section = SettingSection::HIDDEN,
        .label = "设备移除后阻止 DLSS 阶段运行",
        .flags = BOOKKEEPING,
        .get = [](const Settings& s) { return Flag(s.dlss_placement_blocked); },
        .set = [](Settings& s, double v) { s.dlss_placement_blocked = (v != 0.0); },
    },
    {
        .key = "ConfigVersion",
        .kind = SettingKind::UINT,
        .section = SettingSection::HIDDEN,
        .label = "设置版本",
        .min = 1.0,
        .max = 65535.0,
        .flags = BOOKKEEPING,
        .get = [](const Settings& /*s*/) { return static_cast<double>(CONFIG_VERSION); },
        .set = [](Settings& s, double v) { s.config_version = static_cast<uint32_t>(v); },
    },
});

// Plan 5 (v2 design §3.10, P4-P13): pass n = Index + 2's own PassSettings field, through Settings::passes[Index].
// `get`/`set` must stay plain function pointers (SettingDescriptor::get/set), so Index and Member are template
// parameters rather than captured state.
template <size_t Index, auto Member>
double PassGet(const Settings& s) {
  using Field = std::remove_cvref_t<decltype(std::declval<PassSettings&>().*Member)>;
  if constexpr (std::is_same_v<Field, bool>) {
    return Flag(s.passes[Index].*Member);
  } else if constexpr (std::is_enum_v<Field>) {
    return static_cast<double>(static_cast<uint32_t>(s.passes[Index].*Member));
  } else {
    return static_cast<double>(s.passes[Index].*Member);
  }
}

template <size_t Index, auto Member>
void PassSet(Settings& s, double v) {
  using Field = std::remove_cvref_t<decltype(std::declval<PassSettings&>().*Member)>;
  if constexpr (std::is_same_v<Field, bool>) {
    s.passes[Index].*Member = (v != 0.0);
  } else if constexpr (std::is_enum_v<Field>) {
    s.passes[Index].*Member = FromIndex<Field>(v);
  } else {
    s.passes[Index].*Member = static_cast<Field>(v);
  }
}

constexpr size_t PASS_ROW_COUNT = 11u;
constexpr size_t LATER_PASS_COUNT = MAX_PASSES - 1u;

// The 99 per-pass keys ("Pass2FollowPass1".."Pass10Preset"), built once: every per-pass
// SettingDescriptor::key points into this array, which must outlive them (function-local static).
const std::array<std::string, LATER_PASS_COUNT * PASS_ROW_COUNT>& PassKeyStrings() {
  static const std::array<std::string, LATER_PASS_COUNT * PASS_ROW_COUNT> keys = [] {
    constexpr std::array<std::string_view, PASS_ROW_COUNT> SUFFIXES = {
        "FollowPass1", "Intensity", "Style", "LocalTone", "LocalStructure", "SkinStructure", "AutoMask",
        "TransferStrength", "ColorStrength", "Stage", "Preset",
    };
    std::array<std::string, LATER_PASS_COUNT * PASS_ROW_COUNT> result;
    for (size_t index = 0u; index < LATER_PASS_COUNT; ++index) {
      for (size_t field = 0u; field < PASS_ROW_COUNT; ++field) {
        result[index * PASS_ROW_COUNT + field] = std::format("Pass{}{}", index + 2u, SUFFIXES[field]);
      }
    }
    return result;
  }();
  return keys;
}

// Split chain (2026-10-10): pass n's Runs at choices, its first "Same as pass n-1", built once like PassKeyStrings (SettingDescriptor::choices points
// into it).
const std::array<std::array<std::string_view, 4>, LATER_PASS_COUNT>& PassStageChoices() {
  static const std::array<std::string, LATER_PASS_COUNT> same_labels = [] {
    std::array<std::string, LATER_PASS_COUNT> result;
    for (size_t index = 0u; index < LATER_PASS_COUNT; ++index) {
      result[index] = std::format("与第 {} 层相同", index + 1u);
    }
    return result;
  }();
  static const std::array<std::array<std::string_view, 4>, LATER_PASS_COUNT> choices = [] {
    std::array<std::array<std::string_view, 4>, LATER_PASS_COUNT> result;
    for (size_t index = 0u; index < LATER_PASS_COUNT; ++index) {
      result[index] = {same_labels[index], PASS_STAGES[0], PASS_STAGES[1], PASS_STAGES[2]};
    }
    return result;
  }();
  return choices;
}

// Pass n = Index + 2's rows, in the order settings.cpp's SaveSettings and the overlay's Pass tree expect.
template <size_t Index>
std::array<SettingDescriptor, PASS_ROW_COUNT> PassRows() {
  constexpr auto PASS = static_cast<uint8_t>(Index + 2u);
  const std::array<std::string, LATER_PASS_COUNT * PASS_ROW_COUNT>& keys = PassKeyStrings();
  const auto key = [&](size_t field) { return std::string_view(keys[Index * PASS_ROW_COUNT + field]); };
  return std::to_array<SettingDescriptor>({
      {
          .key = key(0),
          .kind = SettingKind::BOOL,
          .section = SettingSection::PASSES,
          .label = "沿用第 1 层外观",
          .tooltip = "关闭后，此层可单独设置外观；运行阶段和 NR 渲染模型预设仍可独立选择。",
          .flags = PASS_SWITCH,
          .get = &PassGet<Index, &PassSettings::follow_pass1>,
          .set = &PassSet<Index, &PassSettings::follow_pass1>,
          .pass = PASS,
      },
      {
          .key = key(1),
          .kind = SettingKind::FLOAT,
          .section = SettingSection::PASSES,
          .label = "强度",
          .tooltip = "控制此层对画面的处理强度，范围为 0（关闭）至 1（最大）。如需更强效果，可提高“处理结果调整”中的“调整强度”。",
          .min = 0.0,
          .max = 10.0,
          .ui_max = 1.0,
          .get = &PassGet<Index, &PassSettings::intensity>,
          .set = &PassSet<Index, &PassSettings::intensity>,
          .pass = PASS,
      },
      {
          .key = key(2),
          .kind = SettingKind::CHOICE,
          .section = SettingSection::PASSES,
          .label = "风格",
          .tooltip = "选择此层的处理风格 A/B/C（0/1/2），与 NR 渲染模型预设相互独立。更改后会重置此层的时序历史。",
          .choices = MODELS,
          .flags = RESTARTS_HISTORY,
          .get = &PassGet<Index, &PassSettings::style>,
          .set = &PassSet<Index, &PassSettings::style>,
          .pass = PASS,
      },
      {
          .key = key(3),
          .kind = SettingKind::FLOAT,
          .section = SettingSection::PASSES,
          .label = "局部色调",
          .tooltip = "控制此层添加的局部对比度。更改后会重置此层的时序历史。",
          .min = 0.0,
          .max = 1.0,
          .flags = RESTARTS_HISTORY,
          .get = &PassGet<Index, &PassSettings::local_tone>,
          .set = &PassSet<Index, &PassSettings::local_tone>,
          .pass = PASS,
      },
      {
          .key = key(4),
          .kind = SettingKind::FLOAT,
          .section = SettingSection::PASSES,
          .label = "结构",
          .tooltip = "控制此层添加的精细结构。更改后会重置此层的时序历史。",
          .min = 0.0,
          .max = 1.0,
          .flags = RESTARTS_HISTORY,
          .get = &PassGet<Index, &PassSettings::local_structure>,
          .set = &PassSet<Index, &PassSettings::local_structure>,
          .pass = PASS,
      },
      {
          .key = key(5),
          .kind = SettingKind::FLOAT,
          .section = SettingSection::PASSES,
          .label = "皮肤结构",
          .tooltip = "控制此层的皮肤区域结构强度；-1 表示跟随此层的“结构”设置。更改后会重置此层的时序历史。",
          .min = 0.0,
          .max = 1.0,
          .special = static_cast<double>(SKIN_SAME_AS_STRUCTURE),
          .flags = RESTARTS_HISTORY,
          .get = &PassGet<Index, &PassSettings::skin_structure>,
          .set = &PassSet<Index, &PassSettings::skin_structure>,
          .pass = PASS,
      },
      {
          .key = key(6),
          .kind = SettingKind::BOOL,
          .section = SettingSection::PASSES,
          .label = "角色遮罩",
          .tooltip = "使用 NR 自带的蒙版保护此层中的角色。更改后会重置此层的时序历史。",
          .flags = RESTARTS_HISTORY,
          .get = &PassGet<Index, &PassSettings::auto_mask>,
          .set = &PassSet<Index, &PassSettings::auto_mask>,
          .pass = PASS,
      },
      {
          .key = key(7),
          .kind = SettingKind::FLOAT,
          .section = SettingSection::PASSES,
          .label = "亮度传递强度",
          .tooltip = "控制此层的亮度变化有多少会传递到画面。",
          .min = 0.0,
          .max = 1.0,
          .get = &PassGet<Index, &PassSettings::transfer_strength>,
          .set = &PassSet<Index, &PassSettings::transfer_strength>,
          .pass = PASS,
      },
      {
          .key = key(8),
          .kind = SettingKind::FLOAT,
          .section = SettingSection::PASSES,
          .label = "色彩传递强度",
          .tooltip = "控制此层的色彩变化有多少会传递到画面。",
          .min = 0.0,
          .max = 1.0,
          .get = &PassGet<Index, &PassSettings::color_strength>,
          .set = &PassSet<Index, &PassSettings::color_strength>,
          .pass = PASS,
      },
      {
          // Split chain (2026-10-10, design 6.2): written and shown whether or not the pass follows pass 1, and kept by the Defaults view.
          .key = key(9),
          .kind = SettingKind::CHOICE,
          .section = SettingSection::PASSES,
          .label = "运行阶段",
          .tooltip = "选择此层在游戏帧中的运行阶段。“DLSS 前”的开销最低：NR 在游戏渲染分辨率下运行，再由 DLSS 升频。“DLSS 后”和“后处理”会处理升频后的画面（后处理阶段包含 HUD）。任何一层都不会早于它的前一层运行。",
          .choices = PassStageChoices()[Index],
          .flags = PASS_SWITCH | PASS_STAGE,
          .get = &PassGet<Index, &PassSettings::stage>,
          .set = &PassSet<Index, &PassSettings::stage>,
          .pass = PASS,
      },
      {
          .key = key(10),
          .kind = SettingKind::CHOICE,
          .section = SettingSection::PASSES,
          .label = "NR 渲染模型",
          .tooltip = "“与第 1 层相同”会沿用“设置”中的 NR 渲染模型预设（默认值，也适用于已有配置）。A：选择旧版 NR 渲染模型（Legacy）；B：选择小型 FP8 NR 渲染模型（Small FP8）。NR 渲染模型选择与是否沿用第 1 层外观相互独立。更改 NR 渲染模型会重新创建 NR 并重置时序历史。“详细信息”会显示各层经运行时确认的 NR 渲染模型。NR 310.8 仅提供旧版 NR 渲染模型（Legacy）。",
          .choices = PASS_NETWORKS,
          .flags = PASS_SWITCH | NETWORK_PRESET | RESTARTS_HISTORY,
          .get = &PassGet<Index, &PassSettings::preset>,
          .set = &PassSet<Index, &PassSettings::preset>,
          .pass = PASS,
      },
  });
}

template <size_t... Index>
std::vector<SettingDescriptor> AllPassRows(std::index_sequence<Index...>) {
  std::vector<SettingDescriptor> rows;
  rows.reserve(sizeof...(Index) * PASS_ROW_COUNT);
  (
      [&] {
        const std::array<SettingDescriptor, PASS_ROW_COUNT> pass_rows = PassRows<Index>();
        rows.insert(rows.end(), pass_rows.begin(), pass_rows.end());
      }(),
      ...);
  return rows;
}

}  // namespace

std::span<const SettingDescriptor> SettingsSchema() {
  static const std::vector<SettingDescriptor> schema = [] {
    std::vector<SettingDescriptor> rows(BASE_ROWS.begin(), BASE_ROWS.end());
    const std::vector<SettingDescriptor> pass_rows = AllPassRows(std::make_index_sequence<LATER_PASS_COUNT>{});
    rows.insert(rows.end(), pass_rows.begin(), pass_rows.end());
    return rows;
  }();
  return schema;
}

const SettingDescriptor* FindSetting(std::string_view key) {
  const std::span<const SettingDescriptor> schema = SettingsSchema();
  const auto found = std::ranges::find(schema, key, &SettingDescriptor::key);
  return (found == schema.end() ? nullptr : &*found);
}

const Settings& DefaultSettings() {
  static const Settings defaults;
  return defaults;
}

bool IsDefault(const Settings& settings, const SettingDescriptor& descriptor) {
  if (descriptor.kind == SettingKind::TEXT) return descriptor.get_text(settings) == descriptor.get_text(DefaultSettings());
  return descriptor.get(settings) == descriptor.get(DefaultSettings());
}

void ResetSetting(Settings* settings, const SettingDescriptor& descriptor) {
  if (descriptor.kind == SettingKind::TEXT) {
    descriptor.set_text(*settings, descriptor.get_text(DefaultSettings()));
    return;
  }
  descriptor.set(*settings, descriptor.get(DefaultSettings()));
}

bool SectionChanged(const Settings& settings, SettingSection section) {
  return std::ranges::any_of(SettingsSchema(), [&settings, section](const SettingDescriptor& descriptor) {
    return descriptor.section == section && !IsDefault(settings, descriptor);
  });
}

void ResetSection(Settings* settings, SettingSection section) {
  for (const SettingDescriptor& descriptor : SettingsSchema()) {
    if (descriptor.section == section) {
      ResetSetting(settings, descriptor);
    }
  }
}

bool InDefaultsView(const SettingDescriptor& descriptor) {
  if ((descriptor.flags & (setting_flags::PASS_STAGE | setting_flags::NETWORK_PRESET)) != 0u) return false;
  switch (descriptor.section) {
    case SettingSection::LOOK:
    case SettingSection::RESULT:
    case SettingSection::LIMITS:
    case SettingSection::PASSES: return true;
    default:                     return false;
  }
}

Settings DefaultsView(Settings mine) {
  for (const SettingDescriptor& descriptor : SettingsSchema()) {
    if (InDefaultsView(descriptor)) {
      ResetSetting(&mine, descriptor);
    }
  }
  return mine;
}

void RestoreAllDefaults(Settings* settings) {
  for (const SettingDescriptor& descriptor : SettingsSchema()) {
    if (descriptor.key != ENABLED_KEY && (descriptor.flags & setting_flags::BOOKKEEPING) == 0u) {
      ResetSetting(settings, descriptor);
    }
  }
}

std::string FormatSettingValue(const SettingDescriptor& descriptor, const Settings& settings) {
  if (descriptor.kind == SettingKind::TEXT) {
    const std::string_view text = descriptor.get_text(settings);
    return (text.empty() ? std::string("空") : std::string(text));
  }
  const double value = descriptor.get(settings);
  switch (descriptor.kind) {
    case SettingKind::BOOL:   return (value != 0.0 ? "开启" : "关闭");
    case SettingKind::UINT:   return std::format("{}", static_cast<uint32_t>(value));
    case SettingKind::KEY:    return (value == 0.0 ? std::string("未设置") : std::format("按键 {:#04x}", static_cast<uint32_t>(value)));
    case SettingKind::CHOICE: {
      const auto index = static_cast<size_t>(value);
      return (index < descriptor.choices.size() ? std::string(descriptor.choices[index]) : std::string("?"));
    }
    case SettingKind::FLOAT:
      if (descriptor.special.has_value() && value == *descriptor.special) {
        return (value < 0.0 ? std::string("与 Structure 相同") : std::string("自动"));
      }
      if (descriptor.key == "AutoExposureBlend") return std::format("{:.0f} %", value * 100.0);  // 2026-10-09: stored 0..1
      return std::format("{:.2f}", value);
    case SettingKind::TEXT: break;
  }
  return {};
}

bool ExposureRowHidden(const SettingDescriptor& descriptor, const Settings& settings) {
  const bool automatic = (settings.input_exposure == color::InputExposure::AUTO);
  // Plan 17: the Adaptation rows matter wherever the meter can be used: Metered, and Auto, which meters when the game's exposure is missing, broken or blended.
  if (descriptor.key == "ExposureAdapt" || descriptor.key == "AdaptBrighterStops" || descriptor.key == "AdaptDarkerStops") {
    return (!automatic && settings.input_exposure != color::InputExposure::METERED);
  }
  if (descriptor.key == "AutoExposureMode") return !automatic;
  if (descriptor.key == "AutoExposureBlend") return (!automatic || settings.auto_exposure_mode != color::AutoExposureMode::BLEND);
  return false;
}

double DescriptorMax(const SettingDescriptor& descriptor) {
  if (descriptor.kind == SettingKind::CHOICE) return static_cast<double>(descriptor.choices.size()) - 1.0;
  // Owner, 2026-10-17: a value typed with Ctrl+click may go past the slider's end, up to ten times it (or `max` when that is higher already, as
  // Intensity's 10), except where the slider's range is the meaning (SLIDER_RANGE_ONLY). The radii only pick pyramid levels, clamped to those that exist.
  if (descriptor.kind == SettingKind::FLOAT && (descriptor.flags & setting_flags::SLIDER_RANGE_ONLY) == 0u && SliderMax(descriptor) > 0.0) {
    return std::max(descriptor.max, TYPED_PAST_SLIDER * SliderMax(descriptor));
  }
  return descriptor.max;
}

double SliderMax(const SettingDescriptor& descriptor) {
  return (descriptor.ui_max > 0.0 ? descriptor.ui_max : descriptor.max);
}

}  // namespace uplift::ui
