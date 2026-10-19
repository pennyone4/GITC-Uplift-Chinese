#include "addon/dlss_summary.hpp"

#include <Windows.h>

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <filesystem>
#include <format>

#include "addon/environment.hpp"
#include "addon/placement.hpp"
#include "addon/reshade_version.hpp"
#include "nr/d3d11_handles.hpp"
#include "nr/log.hpp"
#include "nr/vk_handles.hpp"

namespace uplift::addon {
namespace {

std::string DescribeD3D12(ID3D12Resource* resource) {
  if (resource == nullptr) return "无";
  const D3D12_RESOURCE_DESC description = resource->GetDesc();
  return std::format("DXGI_FORMAT {}，资源 {}x{}，标志 {:#x}", static_cast<int>(description.Format), description.Width, description.Height,
                     static_cast<uint32_t>(description.Flags));
}

std::string DescribeD3D11(ID3D12Resource* punned) {
  ID3D11Resource* const resource = nr::D3D11ResourceOf(punned);
  if (resource == nullptr) return "无";
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  if (FAILED(resource->QueryInterface(IID_PPV_ARGS(&texture)))) return "非二维纹理";
  D3D11_TEXTURE2D_DESC desc = {};
  texture->GetDesc(&desc);
  return std::format("DXGI_FORMAT {}", static_cast<int>(desc.Format));
}

std::string DescribeVulkan(ID3D12Resource* punned) {
  if (punned == nullptr) return "无";
  const NVSDK_NGX_Resource_VK& resource = *nr::VkResourceOf(punned);
  if (resource.Type != NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW) return "Vulkan 缓冲区";
  return std::format("VkFormat {}", static_cast<int>(resource.Resource.ImageViewInfo.Format));
}

}  // namespace

std::string FormatDlssSummary(const DlssSummary& s) {
  const std::string feature = (s.feature == NVSDK_NGX_Feature_SuperSampling       ? std::string("DLSS-SR")
                               : s.feature == NVSDK_NGX_Feature_RayReconstruction ? std::string("光线重建（Ray Reconstruction）")
                                                                                  : std::format("功能 {}", static_cast<int>(s.feature)));
  const std::string motion = (s.motion_vectors ? std::format("{}x{}（{}），{}，缩放 {} x {}，含抖动：{}", s.motion.width, s.motion.height, s.motion_format,
                                                             (s.snapshot.MvLowRes() ? "低分辨率" : "输出分辨率"), s.mv_scale_x, s.mv_scale_y,
                                                             s.snapshot.MvJittered() ? "是" : "否")
                                               : std::string("无"));
  const std::string file = (s.dlss_file.empty() ? std::string("未作为 nvngx_dlss.dll 加载")
                                                : std::format("{}（{}）", s.dlss_file, (s.dlss_version.empty() ? "版本未知" : s.dlss_version)));
  return std::format("首次 DLSS 评估：{}，{}（{}），{}；渲染画面 {}x{}（{}）→输出 {}x{}（{}），{}；运动矢量：{}；曝光纹理：{}，预曝光 {}，"
                     "曝光比例 {}，自动曝光：{}；DLSS 文件：{}",
                     s.api, feature, QualityName(s.snapshot.perf_quality), s.device, s.render.width, s.render.height, s.color_format, s.output.width,
                     s.output.height, s.output_format, (s.snapshot.IsHdr() ? "HDR" : "LDR"), motion, (s.exposure_texture ? "有" : "无"),
                     s.pre_exposure, s.exposure_scale, (s.snapshot.AutoExposure() ? "开启" : "关闭"), file);
}

DlssSummary SummarizeEvaluate(ngx_hooks::NgxApi api, const ngx_hooks::DlssFrame& frame, std::string_view device) {
  const auto describe = [api](ID3D12Resource* resource) {
    switch (api) {
      case ngx_hooks::NgxApi::D3D11:  return DescribeD3D11(resource);
      case ngx_hooks::NgxApi::VULKAN: return DescribeVulkan(resource);
      case ngx_hooks::NgxApi::D3D12:  break;
    }
    return DescribeD3D12(resource);
  };
  if (api == ngx_hooks::NgxApi::D3D12) {
    nr::Logf(nr::LogLevel::INFO,
             "DLSS 尺寸诊断（首次评估）：句柄 {}，序号 {}；创建时记录的渲染尺寸 {}x{} → 输出 {}x{}；当前渲染尺寸 {}x{}；"
             "色彩区域 ({},{}) {}x{}，运动矢量区域 ({},{}) {}x{}，输出区域 ({},{}) {}x{}；色彩 [{}]，输出 [{}]，运动矢量 [{}]",
             static_cast<const void*>(frame.handle), frame.serial, frame.snapshot.render.width, frame.snapshot.render.height,
             frame.snapshot.output.width, frame.snapshot.output.height, frame.render.width, frame.render.height,
             frame.color_region.x, frame.color_region.y, frame.color_region.width, frame.color_region.height,
             frame.motion_region.x, frame.motion_region.y, frame.motion_region.width, frame.motion_region.height,
             frame.output_region.x, frame.output_region.y, frame.output_region.width, frame.output_region.height,
             describe(frame.color), describe(frame.output), describe(frame.motion_vectors));
  }
  DlssSummary summary = {
      .api = (api == ngx_hooks::NgxApi::D3D11 ? "Direct3D 11" : api == ngx_hooks::NgxApi::VULKAN ? "Vulkan" : "Direct3D 12"),
      .device = device,
      .feature = frame.feature,
      .snapshot = frame.snapshot,
      .render = frame.render,
      .color_format = describe(frame.color),
      .output = {frame.output_region.width, frame.output_region.height},
      .output_format = describe(frame.output),
      .motion_vectors = (frame.motion_vectors != nullptr),
      .motion = {frame.motion_region.width, frame.motion_region.height},
      .motion_format = describe(frame.motion_vectors),
      .mv_scale_x = frame.mv_scale_x,
      .mv_scale_y = frame.mv_scale_y,
      .exposure_texture = (frame.exposure_texture != nullptr),
      .pre_exposure = frame.pre_exposure,
      .exposure_scale = frame.exposure_scale,
  };
  // The DLSS file the game (or its mod) loaded, if it is nvngx_dlss.dll; a game that ships another name shows as "not loaded as nvngx_dlss.dll".
  if (const HMODULE dlss = GetModuleHandleW(L"nvngx_dlss.dll"); dlss != nullptr) {
    std::wstring path(32768u, L'\0');
    const DWORD length = GetModuleFileNameW(dlss, path.data(), static_cast<DWORD>(path.size()));
    if (length > 0u && length < path.size()) {
      path.resize(length);
      summary.dlss_file = Utf8FromPath(std::filesystem::path(path));
      if (const std::optional<ModuleVersion> version = ReadModuleVersion(dlss)) summary.dlss_version = FormatModuleVersion(*version);
    }
  }
  return summary;
}

}  // namespace uplift::addon
