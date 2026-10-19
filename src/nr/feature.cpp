#include "nr/feature.hpp"

#include "nr/keys.hpp"
#include "nr/log.hpp"

namespace uplift::nr {
namespace {

void SetResource(NVSDK_NGX_Parameter* parameters, const keys::Resource& key, const BoundResource& bound) {
  parameters->Set(key.resource, bound.resource);
  parameters->Set(key.subrect.base_x, bound.rect.x);
  parameters->Set(key.subrect.base_y, bound.rect.y);
  parameters->Set(key.subrect.width, bound.rect.width);
  parameters->Set(key.subrect.height, bound.rect.height);
}

}  // namespace

Feature::Feature(const Snippet& snippet) : snippet_(snippet) {}

Feature::~Feature() {
  if (handle_ != nullptr) {
    Log(LogLevel::ERR, "Feature 销毁时仍处于已创建状态；为避免不安全释放，句柄已泄漏");
  }
  if (parameters_ != nullptr) {
    snippet_.DestroyParameters(parameters_);
  }
}

NVSDK_NGX_Result Feature::Create(ID3D12GraphicsCommandList* list, const CreateInfo& info) {
  if (handle_ != nullptr || list == nullptr || info.capacity.Empty()) return NVSDK_NGX_Result_FAIL_InvalidParameter;
  if (parameters_ == nullptr) {
    parameters_ = snippet_.AllocateParameters();
  }
  if (parameters_ == nullptr) return NVSDK_NGX_Result_FAIL_NotInitialized;
  parameters_->Set(keys::CREATION_NODE_MASK, 1u);
  parameters_->Set(keys::VISIBILITY_NODE_MASK, 1u);
  parameters_->Set(keys::WIDTH, info.capacity.width);
  parameters_->Set(keys::HEIGHT, info.capacity.height);
  parameters_->Set(keys::PRESET, info.preset);
  parameters_->Set(keys::SCALING_RATIO, 1.f);
  parameters_->Set(keys::PERF_QUALITY_VALUE, static_cast<int>(info.performance) - 1);
  const NVSDK_NGX_Result result = snippet_.CreateFeature(list, parameters_, &handle_, &network_);
  if (NVSDK_NGX_FAILED(result) || handle_ == nullptr) {
    Logf(LogLevel::ERR, "NR 功能创建失败（{}x{}，错误码 {:#010x}）", info.capacity.width, info.capacity.height, static_cast<uint32_t>(result));
    handle_ = nullptr;
    return NVSDK_NGX_FAILED(result) ? result : NVSDK_NGX_Result_FAIL_PlatformError;
  }
  capacity_ = info.capacity;
  enabled_evaluations_ = 0u;
  last_enabled_ = false;
  has_evaluation_ = false;
  Logf(LogLevel::INFO, "已创建 {}x{} 的 NR 功能（D3D12，预设请求值 {}，句柄 {}）；运行时 NR 渲染模型：{}",
       capacity_.width, capacity_.height, info.preset, static_cast<const void*>(handle_),
       network_.empty() ? std::string_view("未确认：没有原生创建消息") : std::string_view(network_));
  return result;
}

NVSDK_NGX_Result Feature::Evaluate(ID3D12GraphicsCommandList* list, const EvaluateInputs& inputs, const Controls& controls) {
  if (handle_ == nullptr || list == nullptr || inputs.color == nullptr || inputs.output == nullptr) {
    return NVSDK_NGX_Result_FAIL_InvalidParameter;
  }
  // The NR floor (spec §6.2), enforced here too so no caller can reach NGX below it.
  const D3D12_RESOURCE_DESC output_desc = inputs.output->GetDesc();
  const Size output_size = {static_cast<uint32_t>(output_desc.Width), output_desc.Height};
  if (!MeetsNrFloor(output_size)) {
    Logf(LogLevel::ERR, "NR 拒绝处理 {}x{} 的输出画面：最低尺寸为 {}x{}", output_size.width, output_size.height,
         MIN_NR_LONG_SIDE, MIN_NR_SHORT_SIDE);
    return NVSDK_NGX_Result_FAIL_InvalidParameter;
  }
  SetResource(parameters_, keys::COLOR, {inputs.color, {}});
  SetResource(parameters_, keys::OUTPUT, {inputs.output, {}});
  SetResource(parameters_, keys::MVEC, inputs.motion);
  SetResource(parameters_, keys::CONTROL_MASK, inputs.control_mask);
  SetResource(parameters_, keys::BACKBUFFER, inputs.backbuffer);
  SetResource(parameters_, keys::UI, inputs.ui);
  SetResource(parameters_, keys::UI_ALPHA, inputs.ui_alpha);
  parameters_->Set(keys::MVEC_SCALE_X, inputs.motion_scale_x);
  parameters_->Set(keys::MVEC_SCALE_Y, inputs.motion_scale_y);
  parameters_->Set(keys::INTENSITY, controls.intensity);
  parameters_->Set(keys::LOCAL_TONE, controls.local_tone);
  parameters_->Set(keys::LOCAL_STRUCTURE, controls.local_structure);
  parameters_->Set(keys::GLOBAL_TONE, controls.global_tone);
  parameters_->Set(keys::USE_AUTO_MASK, controls.auto_mask ? 1 : 0);
  parameters_->Set(keys::MASK_BLEND_MODE, controls.mask_blend_mode);
  parameters_->Set(keys::SKIN_STRUCTURE, controls.skin_structure);
  parameters_->Set(keys::STYLE, controls.style);
  parameters_->Set(keys::RESET, inputs.reset ? 1 : 0);
  parameters_->Set(keys::ENABLED, inputs.bypass ? 0 : 1);
  parameters_->Set(keys::UI_CORRECTION, inputs.ui_correction ? 1 : 0);
  parameters_->Set(keys::DEPTH_INVERTED, inputs.depth_inverted);
  const NVSDK_NGX_Result result = snippet_.EvaluateFeature(list, handle_, parameters_);
  if (NVSDK_NGX_SUCCEED(result)) {
    has_evaluation_ = true;
    const bool enabled = !inputs.bypass && controls.intensity > 0.f;
    if (enabled && (enabled_evaluations_ == 0u || !last_enabled_ || last_style_ != controls.style || last_mask_mode_ != controls.mask_blend_mode)) {
      Logf(LogLevel::INFO, "NR 诊断：已录制启用状态下的 D3D12 评估调用（NGX {:#010x}，句柄 {}，风格 {}，蒙版混合模式 {}，强度 {}，自动角色遮罩 {}，运动矢量 {}，原生控制蒙版 {}）；运行时 NR 渲染模型：{}",
           static_cast<uint32_t>(result), static_cast<const void*>(handle_), controls.style, controls.mask_blend_mode, controls.intensity,
           controls.auto_mask, inputs.motion.resource != nullptr, inputs.control_mask.resource != nullptr,
           network_.empty() ? std::string_view("未确认") : std::string_view(network_));
    }
    if (enabled) ++enabled_evaluations_;
    last_enabled_ = enabled;
    last_style_ = controls.style;
    last_mask_mode_ = controls.mask_blend_mode;
  }
  return result;
}

std::string Feature::Diagnostic(uint32_t pass) const {
  if (handle_ == nullptr) return {};
  const std::string_view network = network_.empty() ? "未确认（没有原生创建消息）" : std::string_view(network_);
  if (!has_evaluation_) return std::format("第 {} 层 NR 渲染模型：{} | 等待评估", pass, network);
  return std::format("第 {} 层 NR 渲染模型：{} | 已记录的启用调用次数：{} | {} | 风格 {} | 原生蒙版混合模式 {}",
                     pass, network, enabled_evaluations_, last_enabled_ ? "已启用" : "已旁路/强度为 0", last_style_, last_mask_mode_);
}

NVSDK_NGX_Result Feature::Release() {
  if (handle_ == nullptr) return NVSDK_NGX_Result_Success;
  const NVSDK_NGX_Result result = snippet_.ReleaseFeature(handle_);
  if (NVSDK_NGX_FAILED(result)) {
    Logf(LogLevel::ERR, "ReleaseFeature 调用失败：{:#010x}", static_cast<uint32_t>(result));
  }
  handle_ = nullptr;
  capacity_ = {};
  return result;
}

void Feature::Abandon() {
  // Device-lost path: never call ReleaseFeature or DestroyParameters. The
  // handle and the parameter object are deliberately leaked; the dtor's
  // "leaked" warning is suppressed because both are now null.
  handle_ = nullptr;
  parameters_ = nullptr;
  capacity_ = {};
}

}  // namespace uplift::nr
