#include "addon/placement.hpp"

#include <format>

namespace uplift::addon {

PlacementChoice ChoosePlacement(ui::PlacementSource source, std::string_view dlss_unavailable_reason, bool dlss_seen, bool before_upscaling,
                                const StageProblems& problems, bool dlss_off) {
  const bool dlss_available = dlss_unavailable_reason.empty();
  // Plan 18: the DLSS stage that can run: Before upscaling when asked and it can, else After DLSS when it can (Ray Reconstruction's own fallback), else none.
  std::optional<Placement> stage;
  if (before_upscaling && problems.before_upscaling.empty()) {
    stage = Placement::BEFORE_UPSCALING;
  } else if (problems.after_dlss.empty()) {
    stage = Placement::AFTER_DLSS;
  }
  switch (source) {
    case ui::PlacementSource::PRESENT:
      return {.placement = Placement::PRESENT};
    case ui::PlacementSource::DLSS:
      if (!dlss_available) return {.placement = Placement::NONE, .reason = std::string(dlss_unavailable_reason)};
      if (dlss_off) return {.placement = Placement::PRESENT};  // Plan 18 Task 12: never a wait for a DLSS the game switched off
      if (!dlss_seen) return {.placement = Placement::NONE, .reason = "等待 DLSS：游戏尚未运行 DLSS"};
      if (!stage) return {.placement = Placement::NONE, .reason = std::string(problems.after_dlss)};
      return {.placement = *stage};
    case ui::PlacementSource::AUTO:
      break;
  }
  return {.placement = ((dlss_available && dlss_seen && !dlss_off && stage) ? *stage : Placement::PRESENT)};
}

bool PreSrFeatureAllowed(NVSDK_NGX_Feature feature, ngx_hooks::NgxApi api) {
  switch (feature) {
    case NVSDK_NGX_Feature_SuperSampling:
      return true;
    case NVSDK_NGX_Feature_RayReconstruction:
      return (api == ngx_hooks::NgxApi::D3D12 || api == ngx_hooks::NgxApi::VULKAN);
    default:
      return false;
  }
}

std::string DlssOnAgainLine(const PlacementChoice& resumed) {
  constexpr std::string_view LEAD = "此设备上的 DLSS 已恢复：";
  switch (resumed.placement) {
    case Placement::AFTER_DLSS:       return std::format("{}DLSS 后阶段将恢复运行", LEAD);
    case Placement::BEFORE_UPSCALING: return std::format("{}DLSS 前阶段将恢复运行", LEAD);
    case Placement::PRESENT:          return std::format("{}NR 将继续在后处理阶段运行", LEAD);
    case Placement::NONE:             break;
  }
  return std::format("{}NR 将继续关闭（{}）", LEAD, resumed.reason);
}

bool DlssReleased(const ngx_hooks::FeatureRegistry& registry, const void* device, ngx_hooks::NgxApi api, bool dlss_seen) {
  return dlss_seen && !registry.UpscalerLive(device, api);
}

bool SetupDlssSeen(bool vulkan, bool upscaler_created, bool context_evaluated) {
  return context_evaluated || (vulkan && upscaler_created);
}

bool PresentMotionWanted(const PresentMotionGates& gates) {
  bool nr_on = (gates.nr_state == nr::SessionState::LOADING || gates.nr_state == nr::SessionState::ACTIVE || gates.nr_state == nr::SessionState::GRACE);
  if (gates.native_present && gates.nr_state == nr::SessionState::OFF) {
    // Stress round: natively the copies are this NR load's motion input from its very first recording, so they are made while NR is about to load too (the
    // gates are read before the present's BeginFrame loads it); otherwise that load would start without them and reload for them.
    nr_on = true;
  }
  // Split chain (Task 5): a native DLSS stage binds DLSS's vectors directly, unless its chain also has a later group at Present, which takes these copies.
  return gates.enabled && nr_on && (gates.bridge_gpu_ordered || gates.native_present) && gates.same_queue && (!gates.native_wants_nr || gates.native_present_group)
         && gates.native_can_copy && !gates.problem && gates.dlss_motion && gates.upscaler_created;
}

VkStageDecision DecideVkStages(const VkStageFacts& facts) {
  VkStageDecision decision;
  if (facts.setting == ui::VulkanNrMode::DIRECT3D_12 || facts.setting == ui::VulkanNrMode::HELPER) {
    decision.stages_off = std::format("仅原生 Vulkan NR 可在游戏的 DLSS 阶段运行（当前 Vulkan NR 模式为 {}）",
                                      (facts.setting == ui::VulkanNrMode::DIRECT3D_12 ? "Direct3D 12" : "辅助进程"));
  } else if (!facts.native_dlss_off.empty()) {
    decision.stages_off = std::string(facts.native_dlss_off);  // the failure's own words: native cannot run inside the game's DLSS either
  }
  decision.native_present_only = !decision.stages_off.empty();
  // Fix round 1 (I2): a failure of native Present alone (the route fell back) leaves the DLSS stages native. The decision is this present's, so the owner
  // moves in the frame the stage does; a context stopped at a DLSS stage (the game's NGX shutdown) keeps it, and with it NR's claim.
  decision.native_at_dlss_stage = facts.stopped_at_dlss_stage || (!decision.native_present_only && facts.dlss_stage_wanted);
  return decision;
}

bool VkRoutePickMovesStage(ui::VulkanNrMode before, ui::VulkanNrMode after, ui::SourcePick stage) {
  return after != before && after != ui::VulkanNrMode::NATIVE && stage != ui::SourcePick::PRESENT;
}

VkRouteChoice ChooseVkNrRoute(const VkRouteFacts& facts) {
  VkRouteChoice choice;
  if (!facts.create_failure.empty()) {
    choice.native_unavailable = std::format("NGX 无法在游戏的 Vulkan 设备上创建 NR 功能（{}）", facts.create_failure);
  } else if (!facts.native_off.empty()) {
    choice.native_unavailable = std::string(facts.native_off);
  } else if (!facts.start_error.empty()) {
    choice.native_unavailable = std::format("原生 Vulkan NR 无法启动（{}）", facts.start_error);
  } else if (!facts.native_needs.empty()) {
    choice.native_unavailable = std::format("原生 Vulkan NR 需要 {}", facts.native_needs);
  }
  if (!facts.d3d12_create_failure.empty()) {
    choice.d3d12_unavailable = std::format("NGX 无法在专用 Direct3D 12 设备上创建 NR 功能（{}）", facts.d3d12_create_failure);
  } else if (!facts.d3d12_start_error.empty()) {
    choice.d3d12_unavailable = std::format("专用 Direct3D 12 设备无法启动（{}）", facts.d3d12_start_error);
  }
  switch (facts.setting) {
    case ui::VulkanNrMode::NATIVE:
      if (choice.native_unavailable.empty()) {
        choice.route = VkNrRoute::NATIVE;
      } else if (choice.d3d12_unavailable.empty()) {
        choice.route = VkNrRoute::DIRECT3D_12;
        choice.reason = choice.native_unavailable;
      } else {
        choice.route = VkNrRoute::HELPER;
        choice.reason = std::format("{}；{}", choice.native_unavailable, choice.d3d12_unavailable);
      }
      break;
    case ui::VulkanNrMode::DIRECT3D_12:
      if (choice.d3d12_unavailable.empty()) {
        choice.route = VkNrRoute::DIRECT3D_12;
        choice.reason = (choice.native_unavailable.empty() ? std::string("“高级”中的“Vulkan 后处理阶段 NR”已设为“Direct3D 12”") : choice.native_unavailable);
      } else {
        choice.route = VkNrRoute::HELPER;
        choice.reason = choice.d3d12_unavailable;
      }
      break;
    case ui::VulkanNrMode::HELPER:
      choice.route = VkNrRoute::HELPER;
      choice.reason = "“高级”中的“Vulkan 后处理阶段 NR”已设为“辅助进程”";
      break;
  }
  return choice;
}

VkNrOwner ChooseVkNrOwner(const VkOwnerFacts& facts) {
  if (facts.abandoned) return VkNrOwner::NONE;
  if (facts.native_at_dlss_stage) return VkNrOwner::NATIVE_DLSS_STAGE;
  switch (facts.route) {
    case VkNrRoute::NATIVE:      return VkNrOwner::NATIVE_PRESENT;
    case VkNrRoute::DIRECT3D_12: return VkNrOwner::BRIDGE;
    case VkNrRoute::HELPER:      return VkNrOwner::HELPER;
  }
  return VkNrOwner::NONE;
}

bool VkHolderDrainsNow(VkNrOwner owner, VkNrHolder holder) {
  switch (holder) {
    case VkNrHolder::NATIVE: return owner != VkNrOwner::NATIVE_PRESENT && owner != VkNrOwner::NATIVE_DLSS_STAGE;
    case VkNrHolder::BRIDGE: return owner != VkNrOwner::BRIDGE;
    case VkNrHolder::HELPER: return owner != VkNrOwner::HELPER;
  }
  return true;
}

VkOwnerChange VkOwnerChangeOf(VkNrOwner previous, VkNrOwner now, bool stage_changed, bool setting_changed) {
  if (previous == VkNrOwner::NONE || now == VkNrOwner::NONE || previous == now) return {};
  // Fix round 1 (M5): a route pick that also moves the stage names the route change.
  const VkOwnerCause cause = (setting_changed ? VkOwnerCause::SETTING : stage_changed ? VkOwnerCause::STAGE : VkOwnerCause::FALLBACK);
  return {.changed = true, .cause = cause};
}

std::string_view VkOwnerName(VkNrOwner owner) {
  switch (owner) {
    case VkNrOwner::NATIVE_PRESENT:    return "原生后处理阶段 NR";
    case VkNrOwner::NATIVE_DLSS_STAGE: return "原生 DLSS 阶段 NR";
    case VkNrOwner::BRIDGE:            return "专用 Direct3D 12 设备";
    case VkNrOwner::HELPER:            return "Uplift 辅助进程";
    case VkNrOwner::NONE:              break;
  }
  return "无";
}

std::string VkOwnerChangeLine(VkNrOwner previous, VkNrOwner now, VkOwnerCause cause, std::string_view detail) {
  std::string why;
  switch (cause) {
    case VkOwnerCause::STAGE:    why = "NR 阶段已更改"; break;
    case VkOwnerCause::SETTING:  why = "Vulkan NR 设置已更改"; break;
    case VkOwnerCause::FALLBACK: why = (detail.empty() ? std::string("自动回退") : std::format("自动回退：{}", detail)); break;
  }
  return std::format("NR 已从{}切换至{}（原因：{}）：旧设备正在释放资源，新设备将从关闭状态启动", VkOwnerName(previous), VkOwnerName(now), why);
}

std::string VkRouteLine(const VkRouteChoice& choice) {
  switch (choice.route) {
    case VkNrRoute::NATIVE:      return "Vulkan：NR 在后处理阶段通过游戏的 Vulkan 设备原生运行";
    case VkNrRoute::DIRECT3D_12: return std::format("Vulkan：NR 在后处理阶段通过专用 Direct3D 12 设备运行：{}", choice.reason);
    case VkNrRoute::HELPER:      break;
  }
  return std::format("Vulkan：NR 在后处理阶段通过 Uplift 辅助进程运行：{}", choice.reason);
}

std::string_view QualityName(int perf_quality) {
  switch (perf_quality) {
    case 0:  return "性能";
    case 1:  return "均衡";
    case 2:  return "质量";
    case 3:  return "超高性能";
    case 4:  return "超高质量";
    case 5:  return "DLAA";
    default: return "自定义";
  }
}

std::string FormatPlacementLine(const PlacementChoice& choice, const std::optional<ngx_hooks::CreateSnapshot>& main, bool vulkan) {
  switch (choice.placement) {
    case Placement::PRESENT:          return "运行阶段：后处理";
    case Placement::NONE:             return std::format("运行阶段：无（{}）", choice.reason);
    case Placement::AFTER_DLSS:
    case Placement::BEFORE_UPSCALING: break;
  }
  const std::string_view name = (choice.placement == Placement::AFTER_DLSS ? "DLSS 后" : "DLSS 前");
  const std::string_view api = (vulkan ? "Vulkan，" : "");
  if (!main) return (vulkan ? std::format("运行阶段：{}（Vulkan）", name) : std::format("运行阶段：{}", name));
  return std::format("运行阶段：{}（{}{}，{}x{} → {}x{}）", name, api, QualityName(main->perf_quality), main->render.width,
                     main->render.height, main->output.width, main->output.height);
}

std::string ContextMessage(const MessageInputs& inputs) {
  for (const std::string_view first : {inputs.blocked, inputs.output, inputs.placement}) {
    if (!first.empty()) return std::string(first);
  }
  if (inputs.skip_reason.empty()) return {};
  if (inputs.skip_from_session && !inputs.session_message.empty()) return std::string(inputs.session_message);
  return std::format("NR 已跳过：{}", inputs.skip_reason);
}

}  // namespace uplift::addon
