#include "addon/swapchain_usage.hpp"

#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "addon/reshade_api.hpp"
#include "addon/uplift_catch.hpp"
#include "nr/log.hpp"
#include "vk/frame.hpp"

namespace uplift::addon {
namespace {

namespace api = reshade::api;

std::atomic<bool> g_vulkan_swapchain_event{false};
std::atomic<bool> g_desc_has_fullscreen_state{false};
std::atomic<bool> g_fullscreen_skip_logged{false};
std::atomic<bool> g_d3d9ex_requested{false};  // 1.1.5: NoteD3D9ExRequested
std::atomic<bool> g_d3d9ex_windowed_logged{false};

// ReShade 6.8's swapchain_desc (include/reshade_api_device.hpp of 6.8): the pinned 6.0.0 type's members, then fullscreen_state, the refresh rate and
// sync_interval. The handler gets a reference to ReShade's own object, so when ReShade is 6.8 or later these bytes are there to read.
struct SwapchainDesc68 {
  api::resource_desc back_buffer;
  uint32_t back_buffer_count;
  uint32_t present_mode;
  uint32_t present_flags;
  bool fullscreen_state;
  float fullscreen_refresh_rate;
  uint32_t sync_interval;
};
static_assert(offsetof(SwapchainDesc68, back_buffer) == offsetof(api::swapchain_desc, back_buffer));
static_assert(offsetof(SwapchainDesc68, back_buffer_count) == offsetof(api::swapchain_desc, back_buffer_count));
static_assert(offsetof(SwapchainDesc68, present_mode) == offsetof(api::swapchain_desc, present_mode));
static_assert(offsetof(SwapchainDesc68, present_flags) == offsetof(api::swapchain_desc, present_flags));
static_assert(sizeof(SwapchainDesc68) >= sizeof(api::swapchain_desc));

// Vulkan only, and no Uplift lock, ever (Plan 10 C-1): ReShade raises this inside vkCreateSwapchainKHR, and other APIs' swap chains pass through
// it too. Returning true makes ReShade rebuild the create info from `desc`; the round trip keeps every other field and every unmapped usage bit
// (vulkan_hooks_swapchain.cpp:220-255, vulkan_impl_type_convert.cpp:860-910), except for a swap chain that asks for exclusive fullscreen (below).
bool OnCreateSwapchain(api::device_api device_api, api::swapchain_desc& desc, void* /*hwnd*/) {
  try {
    if (device_api == api::device_api::d3d9) {
      // 1.1.5: a Direct3D 9Ex device in exclusive fullscreen cannot be made through ReShade's upgrade (see NoteD3D9ExRequested): run it windowed at the
      // same size instead. Returning true makes ReShade write Windowed = TRUE and a zero refresh rate into the present parameters (d3d9.cpp:151-155).
      if (!g_d3d9ex_requested.load(std::memory_order_relaxed) || !g_desc_has_fullscreen_state.load(std::memory_order_relaxed)) return false;
      auto& desc68 = reinterpret_cast<SwapchainDesc68&>(desc);
      if (!desc68.fullscreen_state) return false;
      desc68.fullscreen_state = false;
      desc68.fullscreen_refresh_rate = 0.f;
      if (!g_d3d9ex_windowed_logged.exchange(true, std::memory_order_relaxed)) {
        nr::Log(nr::LogLevel::INFO,
                "Direct3D 9Ex：游戏请求独占全屏，但 ReShade 无法以 Direct3D 9Ex 设备实现独占全屏；Uplift 将以相同分辨率的无边框窗口运行");
      }
      return true;
    }
    if (device_api != api::device_api::vulkan) return false;
    if (g_desc_has_fullscreen_state.load(std::memory_order_relaxed) && reinterpret_cast<const SwapchainDesc68&>(desc).fullscreen_state) {
      // Final review I-2 (see the header): no rebuild. The swap chain keeps ReShade's own usage bits, without TRANSFER_DST, and the card says why.
      vk::NoteExclusiveFullscreenSwapchain();
      if (!g_fullscreen_skip_logged.exchange(true, std::memory_order_relaxed)) {
        nr::Log(nr::LogLevel::INFO,
                "Vulkan：未添加交换链复制权限（create_swapchain）：游戏请求独占全屏，NR 无法写回此交换链；如需使用 NR，请切换至无边框窗口或窗口模式");
      }
      return false;
    }
    desc.back_buffer.usage |= api::resource_usage::copy_dest | api::resource_usage::copy_source;
    if (!g_vulkan_swapchain_event.exchange(true, std::memory_order_relaxed)) {
      nr::Log(nr::LogLevel::INFO, "Vulkan：已添加交换链复制权限（create_swapchain）");
    }
    return true;
  }
  UPLIFT_CATCH("create_swapchain", false)
}

}  // namespace

void NoteD3D9ExRequested() {
  g_d3d9ex_requested.store(true, std::memory_order_relaxed);
}

void RegisterCreateSwapchainEvent(bool desc_has_fullscreen_state) {
  g_desc_has_fullscreen_state.store(desc_has_fullscreen_state, std::memory_order_relaxed);
  const auto register_event = reinterpret_cast<void (*)(reshade::addon_event, void*)>(
      GetProcAddress(reshade::internal::get_reshade_module_handle(), "ReShadeRegisterEvent"));
  if (register_event != nullptr) {
    register_event(static_cast<reshade::addon_event>(CREATE_SWAPCHAIN_EVENT), reinterpret_cast<void*>(&OnCreateSwapchain));
  }
}

bool CreateSwapchainEventSeen() {
  return g_vulkan_swapchain_event.load(std::memory_order_relaxed);
}

}  // namespace uplift::addon
