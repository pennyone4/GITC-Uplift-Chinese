#pragma once

#include <vulkan/vulkan.h>

#include <dxgiformat.h>

#include <cstdint>
#include <string>

#include "nr/types.hpp"
#include "vk/game_host.hpp"

namespace uplift::vk {

// One Vulkan image as the host describes it (from ReShade's get_resource_desc, whose api::format is DXGI-numbered).
struct ImageInfo {
  VkImage image = VK_NULL_HANDLE;  // null: none
  nr::Size size;
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
  uint32_t samples = 1u;
  bool copy_dest = false;  // the image can be a copy's destination (ReShade gives a swap chain TRANSFER_DST only through create_swapchain)
};

inline constexpr char COPY_ACCESS_PROBLEM[] =
    "Vulkan 交换链无法接收 NR 结果：当前 ReShade 未向 Uplift 提供添加复制权限所需的 create_swapchain 加载项事件（已针对 ReShade 6.8 进行测试）。"
    "请使用 ReShade 安装程序更新 Vulkan 层，然后重启游戏";
// Final review I-2: what the same missing copy access means when the add-on's create_swapchain handler left the swap chain alone on purpose.
inline constexpr char EXCLUSIVE_FULLSCREEN_PROBLEM[] =
    "Vulkan 交换链无法接收 NR 结果：游戏请求独占全屏，Uplift 不会为此类交换链添加复制权限（ReShade 6.8 会重建交换链请求并丢失部分设置）。"
    "如需使用 NR，请切换至无边框窗口或窗口模式";

// Final review I-2: the add-on's create_swapchain handler saw a swap chain that asks for exclusive fullscreen and left its usage alone. Process-wide
// and set once it happens, so BackBufferProblem names the real cause for a back buffer without copy access.
void NoteExclusiveFullscreenSwapchain();
[[nodiscard]] bool ExclusiveFullscreenSwapchainSeen();

// Plan 11 (Vulkan design §3.4-§3.6): the frame's Vulkan recording, shared by the in-process bridge (VkBridge) and the 32-bit client (VkClient). Every
// function records only through the GameHost, into the immediate list of the queue Uplift runs on; none waits or submits.

// Why `back_buffer` cannot reach NR, as the card says it; empty when it can: no copy access (COPY_ACCESS_PROBLEM, or EXCLUSIVE_FULLSCREEN_PROBLEM
// when the handler skipped an exclusive-fullscreen swap chain), multisampled, a format Uplift does not process, or a device without
// VK_KHR_external_memory_win32 (`memory_win32`).
[[nodiscard]] std::string BackBufferProblem(const ImageInfo& back_buffer, bool memory_win32);

// Batch 2 review I-1 (a), at the present event, before ReShade records its effects: a global memory barrier that orders the game's whole frame on this
// queue before the effects (which Uplift's early flushes submit without the game's present semaphores) and before the copy in.
void RecordOpeningBarrier(GameHost& host);

// The copy in (design §3.2): the back buffer (in `entry_state`) into `shared_color`, left in general; and `motion` (UPLIFT_MV, shader_resource) into
// `shared_motion` when `motion` is not null. A real execution and memory dependency on everything the game submitted on this queue comes first.
void RecordCopyIn(GameHost& host, VkImage back_buffer, Usage entry_state, VkImage shared_color, VkImage motion, VkImage shared_motion);

// The copy out: `shared_color` (general) into the back buffer, returned to `entry_state`, with the closing global barrier (batch 2 review I-1 (b)).
void RecordCopyOut(GameHost& host, VkImage back_buffer, Usage entry_state, VkImage shared_color);

// UPLIFT_MASK (shader_resource between techniques) into `shared_mask`, left in general, for the next frame's recording.
void RecordMaskCopy(GameHost& host, VkImage mask, VkImage shared_mask);

}  // namespace uplift::vk
