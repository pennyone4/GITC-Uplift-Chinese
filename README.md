# GITC Uplift

GITC Uplift 是一款专为虚拟摄影打造的 DLSS 5 模组，通过 ReShade 使用 NVIDIA DLSS Neural Rendering（DLSS-NR），并针对灵活性、易用性、稳定性和跨 API 兼容性进行了优化。支持 DirectX 9、10、11、12、Vulkan 和 OpenGL，也支持 32 位与 64 位游戏。安装后启用即可自动完成其余工作；关闭 NR 后，Uplift 会释放其占用的显存。

GITC Uplift 与 NVIDIA、ReShade 均无隶属关系。

## 所需组件

- NVIDIA GeForce RTX 显卡及较新的驱动程序。
- [ReShade](https://reshade.me)，并且必须支持完整的插件功能（Addon 版本）。请使用最新版；Vulkan 和 OpenGL 游戏需要 6.8 或更高版本。
- NVIDIA DLSS-NR 运行时 `nvngx_dlssnr.dll`，需要由你自行提供。310.8 仍是较旧 RTX 显卡上经过测试的版本；310.9.16 提供下文介绍的实验性控制项，并且只包含 50 系列显卡的 GPU 内核。Uplift 不包含任何 NVIDIA 文件，请选择适用于你显卡的版本。

## 其他 DLSS 5 插件

- **请使用 GITC Uplift 替代其他 DLSS-NR 插件**，例如 RenoDX 的 DLSS 和 DLSS 5 插件（`renodx-dlss*.addon64`）。请将它们从 ReShade 插件目录中移除；如果其他插件仍在加载，Uplift 将无法工作。

## 开始之前

- **仅限单人游戏。** Uplift 会挂接到游戏进程，反作弊系统可能会因此产生问题。请勿在联网游戏中使用。
- **杀毒软件警告：** 部分杀毒软件会将 ReShade 插件误报为威胁。每个发行版都会在 `SHA256SUMS.txt` 中列出文件的 SHA-256 校验值；你也可以自行从源码构建 Uplift（见“从源码构建”）。

## 安装

安装步骤很简单：
1. 从 [GitHub Releases](https://github.com/ghostinthecamera/GITC-Uplift/releases) 下载最新发行版。
2. 将发行版 `Addons` 文件夹中的所有内容（文件和 `GITC-Uplift` 文件夹）复制到 ReShade 插件目录。
3. 将 `nvngx_dlssnr.dll` 复制到 `GITC-Uplift` 文件夹中。
4. 将发行版 `Shaders` 文件夹中的文件复制到 ReShade 的 `Shaders` 文件夹。

安装完成！

- 从旧版 Uplift 升级？请先删除旧版的 `uplift.addon64`、`uplift.addon32` 和 `uplift-helper64.exe`，避免同时加载两个版本。还要用新版 `Uplift.fx` 覆盖旧文件。
- 除非游戏的 `ReShade.ini` 中通过 `AddonPath` 指定了其他位置，否则 ReShade 插件目录就是游戏目录。
- 游戏内界面使用 ReShade 提供的字体；如需显示简体中文，请在 ReShade 设置中将界面语言设为简体中文。

## 使用方法

1. 启动游戏并按 Home 键打开 ReShade。
2. 切换到 **GITC Uplift** 选项卡，勾选 **启用**。

完成。选项卡顶部的状态卡会实时显示 NR 的运行状态：运行时为绿色；未运行时会说明原因和处理方法。

所有设置的图文说明请见[使用指南](https://github.com/ghostinthecamera/GITC-Uplift/wiki/Guide)。

**设置**区域包含以下选项：

- **NR 阶段：** **后处理**适用于所有游戏，处理最终画面。**DLSS 后**（仅限使用 DLSS 的游戏）会在游戏帧内、HUD 绘制前处理画面。**DLSS 前**为实验性选项。
- **运动矢量：** 自动模式会代为选择。其他选项为 DLSS（游戏自带）、LaunchPad（iMMERSE LaunchPad 提供）、Lumenite（LumeniteFX 的 Kernel）和关闭。
- **原生蒙版混合模式：** 选择 NR 原生蒙版的组合方式，默认值为 1。
- **网络预设：** NR 310.9.16 支持 A：Legacy 或 B：Small FP8。
- **分辨率：** 全分辨率画质最好；较低的分辨率运行更快、显存占用更少。

当前游戏不支持的选项会变灰；将鼠标悬停在选项上即可查看原因。设置会被保存，并会在条件允许时自动恢复生效。

在大多数游戏中，如果你在游戏内关闭 DLSS，NR 会自动切换到后处理；重新开启 DLSS 后也会自动切回。

下方的**外观**区域可用于调整画面效果。关闭 NR 后，Uplift 会在数秒后释放其显存。

使用 **NR 310.9.16** 时，**设置 → 网络预设**可选择 **Legacy** 或 **Small FP8**；**运行时默认**会在该版本中选择 Small FP8。Uplift 默认仍使用 Legacy。切换网络会重新创建 NR 并重置历史记录。**设置 → 原生蒙版混合模式**提供 0–3 的实验性模式，与 Uplift 自带的蒙版相互独立。NR 310.8 始终使用 Legacy 网络，并忽略蒙版混合模式。Uplift 识别到 310.8 运行时后，会禁用这两个控制项并说明原因；网络选项会显示 Legacy。切换运行时路径会在下次启动游戏时生效。

预设 **A（1）**为 Legacy，**B（2）**为 Small FP8。提供的 DLL 不包含 C–G 预设所需的权重。**外观 → 风格**可选择独立的 A/B/C 处理风格（0/1/2），后续各层也可以单独设置风格。

**详细信息**会逐层显示当前层数范围内已创建的 NR 功能所用网络名称（取自 NVIDIA 成功创建功能时的消息）、已记录的启用评估调用次数，以及最近一次成功调用的风格和原生蒙版模式。如果运行时日志格式或相关消息不可用，则会显示“网络未经确认”。调用次数表示命令已成功录制，不代表 GPU 已完成处理或画面效果已通过验证。

将 **日志级别**设为默认的**信息**后，可在 `ReShade.log` 中搜索以下内容：

- `snippet loaded`：实际运行时路径及版本；
- `BuildActiveNetwork config=` 和 `Weight heap request`：所选计算图和权重张量数量；
- `DLSSNR: Created feature`：运行时实际选择的预设和网络，包括回退结果；
- `NR diagnostic: enabled`：成功启用的评估调用，以及其网络、风格、蒙版模式和绑定的运动矢量/蒙版输入。

Small FP8 的运行时网络名称为 `hnet-crazy-cuckoo-small-fp8`，预设为 2，提供的构建版本包含 77 个权重张量（对齐后共 113,907,200 字节）。张量分配量不等于总显存占用。每个功能都会分别记录评估调用，包括后续层和“保护面部”处理；风格或原生蒙版模式变化时也会记录。NR 在辅助进程中运行时，这些日志同样会写入 `ReShade.log`。

Uplift 不会绑定深度或双向畸变输入。原生引导升频尚未验证可用；降低分辨率时会使用 Uplift 自带的升频功能。全局色调和隐藏的原生“性能”设置尚未确认有效，因此仍保持隐藏。

**标准版 310.9.16 DLL** 不包含 Ada GPU 内核：仅绕过架构检查，仍无法在 RTX 4090 上加载其 FP8 网络。另行提供的 Ada 兼容性原型会使用其他 Ada 内核执行 Small FP8。原生蒙版混合模式默认为 **1**；模式 0 使用不同的蒙版组合方式，画面变化可能不明显。已保存的选择会保留。兼容运行时下完整的蒙版组合行为仍在验证中：供体预处理内核缺少新增的蒙版字段。已在 1080p/D3D12 下演示运行，但尚未验证其效果与原生 Blackwell 是否一致。

另行提供的 Shortfuse **compat-fp16 310.9.16 / CUDA 13.4.3** 构建也受支持。它保留相同网络权重和 310.9.16 内核接口，并包含适用于 SM75、SM86、SM89 和 SM120 的 GPU 代码。Uplift 的原生网络诊断支持其扩展后的 DLL 布局。该文件已在 RTX 4090 上通过 Direct3D 12 和原生 Vulkan 测试；此处尚未实测其他 GPU 架构。请在**高级 → 运行时路径**中直接选择此 DLL，重启游戏后即可照常使用“设置”中的网络和蒙版模式选项。该 DLL 会替换当前选择的运行时，不会与 Uplift 早期提供的 NVIDIA DLL 实验性补丁叠加使用。

在独立测试中，已确认原生蒙版模式 0–3 配合原生棋盘格 ControlMask 会产生不同输出。Uplift 会传递所选模式，但常规路径不会绑定该原生蒙版；现有的 `UPLIFT_MASK` 仍在 NR 处理完成后进行合成。界面默认值仍为模式 1。

**保护面部**（位于“外观”区域）会保留角色的光照和阴影，同时避免 NR 改变角色面部形状。此功能开销更高：每帧会额外运行一层 NR，并需要更多显存（4K 下约 1 GB）。调节方法请见[使用指南](https://github.com/ghostinthecamera/GITC-Uplift/wiki/Guide#keep-faces)。

着色器为可选组件：

- **`Uplift.fx`** 会添加 `Uplift` 技术；可在 ReShade 效果列表中拖动它，选择 NR 相对于其他效果的运行位置。不使用该着色器时，NR 会在所有效果之前运行。LaunchPad 和 Lumenite 的运动矢量需要此着色器：
  - **LaunchPad：** 启用 LaunchPad，并将 Uplift 放在它后面。
  - **Lumenite：** 启用 **LUMENITE: Kernel 2.0**，并将 Uplift 放在它后面。Lumenite 需要 Uplift 1.2.0 或更高版本中的 `Uplift.fx`，且不支持 DX9 游戏。
  - 然后将 **运动矢量**设为自动，或选择 LaunchPad / Lumenite。两者同时启用时，自动模式会优先使用 LaunchPad。
- **`UpliftMask.fx`** 是一个示例蒙版，可限制 NR 的处理范围。

## 各 API 和位数的支持情况

| | DX12 | DX11 | DX10 | DX9 | Vulkan | OpenGL |
|---|---|---|---|---|---|---|
| **64 位** | ✓ + DLSS | ✓ + DLSS | ✓ | ✓ | ✓ + DLSS | ✓ |
| **32 位** | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |

- **✓** NR 会处理最终画面；所有控制项都位于 Uplift 选项卡中。
- **+ DLSS** 对使用 DLSS 的游戏，NR 也可以在游戏帧内、DLSS 之后运行，并使用游戏自带的运动矢量。在 DX12 和 Vulkan 下也支持帧生成。Vulkan 下需手动选择“DLSS 后”；自动模式会保持在后处理（见“进阶用法”）。
- 通过模组添加 DLSS 的 DX11 游戏也可以使用。如果模组在自己的 DirectX 12 设备上运行 DLSS，NR 会保持在后处理，Uplift 选项卡也会显示原因。
- **64 位 Vulkan：** NR 会优先在游戏自己的 Vulkan 设备上运行；如果无法运行，则切换到专用 DX12 设备，再不行则切换到辅助进程。**详细信息**会说明当前运行位置和原因。**高级 → Vulkan 后处理阶段 NR**可用于手动选择运行位置。只有游戏自身设备（**原生**）支持在游戏 DLSS 阶段运行 NR。选择 Direct3D 12 或辅助进程后，NR 会切换到后处理，并禁用“DLSS 后”和“DLSS 前”。如果原生 NR 仅在后处理阶段失败，“DLSS 后”和“DLSS 前”仍可使用。
- DXVK 游戏按 Vulkan 游戏处理；dgVoodoo2 游戏按 32 位 DX11 或 DX12 游戏处理。

**已测试游戏：**《鬼武者：剑之道》（DX12）、《最终幻想 XV》（DX11）、《无人深空》（Vulkan）、《最终幻想 XIII：雷霆归来》（32 位 DX9，以及通过 dgVoodoo2 运行的 DX11）。

## 故障排查

Uplift 选项卡顶部的状态卡会说明问题。常见情况如下：

| 状态卡提示 | 处理方法 |
|---|---|
| “未找到 NR 运行时” | 将 `nvngx_dlssnr.dll` 放入插件旁的 `GITC-Uplift` 文件夹，然后重启游戏。 |
| “移动 nvngx_dlssnr.dll” | 将 `nvngx_dlssnr.dll` 从游戏 `.exe` 所在目录移至插件旁的 `GITC-Uplift` 文件夹，然后重启游戏。 |
| “NR 运行失败”，并显示 NVIDIA 错误代码 | `nvngx_dlssnr.dll` 可能不适用于你的显卡（参见“所需组件”），也可能是显卡驱动版本过旧。 |
| “Uplift 的 64 位辅助进程已停止” | 将同一发行版中的 `gitc-uplift-helper64.exe` 复制到插件旁，然后点击 **立即重试**。 |
| “GPU 设备已移除” | 如果状态卡提供按钮，请点击 **立即重试**；否则重启游戏。设备停止后，部分显存（4K 下为 1 GB 或更多）会一直占用到游戏退出。如果连续发生两次，请重启游戏并附上 `ReShade.log` 报告问题。 |
| “NR 至少需要 640x360” | 提高游戏分辨率或窗口尺寸。 |
| “NR 已跳过：显存需求 1.2 GB，可用 0.6 GB” | 游戏已占用大部分显存。请降低**显存余量**，或在“高级”中将**显存检查**设为**宽松**或**关闭**。 |
| Vulkan 游戏在启动时因 Uplift 崩溃 | 再次启动游戏；Uplift 会自动关闭对游戏 Vulkan 设备的扩展。如果仍然崩溃，请在“高级”中将**Vulkan 后处理阶段 NR**设为**辅助进程**。 |
| 极低分辨率下 NR 开销很大 | 将**层数**设为 1。分辨率低于约 720p 时，NR 的运算开销不会随画面尺寸明显下降，因此降低**分辨率**的帮助有限。 |
| **后处理**阶段选择 **DLSS** 运动矢量，且移动镜头时画面闪烁 | 游戏可能以倒置方向保存图像（许多 Unity 游戏如此）。请在 ReShade 的全局预处理器定义中将 `RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN` 设为 1（与深度效果的设置相同）；Uplift 随后会校正 DLSS 运动矢量的方向。 |
| 启用**保护面部**后，角色出现亮点或闪光 | 勾选“高级”中的**显示面部蒙版**并进行调节；参见[使用指南](https://github.com/ghostinthecamera/GITC-Uplift/wiki/Guide#keep-faces)。 |
| “检测到其他 NR 工具正在运行” | 移除其他 DLSS 5 插件（见“其他 DLSS 5 插件”），然后重启游戏。 |
| “Vulkan 交换链无法接收 NR 的结果” | 将 ReShade 更新至 6.8（安装程序中选择**仅更新 ReShade**）。如果提示独占全屏问题，请切换到无边框或窗口模式。 |
| “32 位游戏地址空间不足” | 为游戏应用 4 GB（大地址感知）补丁。 |

仍无法解决？请[提交 Issue](https://github.com/ghostinthecamera/GITC-Uplift/issues)，并附上：

- `ReShade.log`（位于 `ReShade.ini` 旁，通常在游戏目录中）；
- 状态卡内容及**详细信息**中的各行；
- 显卡型号、驱动版本和 ReShade 版本；
- 游戏名称。

## 已知限制

- 如果 NVIDIA NR 在 GPU 上崩溃，游戏也可能崩溃（与任何 GPU 崩溃相同）。请重启游戏，并附上 `ReShade.log` 报告问题。

## 进阶用法

发行版包含 `gitc-uplift.addon64`、`gitc-uplift.addon32`、`gitc-uplift-helper64.exe`、`Uplift.fx` 和 `UpliftMask.fx`。游戏需要哪些文件取决于所用 API 和位数；如果需要精简安装，不必放入所有文件。

| 游戏类型 | ReShade 插件目录所需文件 |
|---|---|
| 64 位 Direct3D 10、11 或 12、Vulkan、OpenGL | `gitc-uplift.addon64` 和 `GITC-Uplift\nvngx_dlssnr.dll` |
| 64 位 Direct3D 9 | `gitc-uplift.addon64`、`gitc-uplift-helper64.exe` 和 `GITC-Uplift\nvngx_dlssnr.dll` |
| 任意 32 位游戏 | `gitc-uplift.addon32`、`gitc-uplift-helper64.exe` 和（64 位）`GITC-Uplift\nvngx_dlssnr.dll` |

- `nvngx_dlssnr.dll` 也可以直接放在插件旁，或放在你通过“高级 → 运行时路径”指定的位置。请勿放在游戏 `.exe` 旁：RTX 50 系列显卡上的 NVIDIA DLSS 会优先从那里加载 DLL，Uplift 无法使用该副本。
- 插件文件和 `gitc-uplift-helper64.exe` 必须来自同一发行版。

### 不同类型游戏所需的 ReShade 安装方式

- **DX10、DX11、DX12：** 常规 ReShade 安装。
- **DX9：** 将 ReShade 安装为 `d3d9.dll`。NR 每帧会通过内存复制画面，1080p 下开销约 3 ms。启用“高级”中的**使用 Direct3D 9Ex**并重启游戏后，开销可降至约 0.4 ms。如果游戏无法启动，请再次启动；Uplift 会自动关闭该选项。
- **Vulkan（含 DXVK）：** 使用 ReShade 6.8 的 Vulkan Layer。运行安装程序，选择游戏可执行文件并选择 **Vulkan**。如果安装程序找到 `ReShade.ini`，请选择**仅更新 ReShade**（卸载会删除 `ReShade.ini`）。通过 DXVK 运行的游戏必须使用 Vulkan Layer，不能使用 `d3d9.dll` 或 `dxgi.dll` 版 ReShade。
- **OpenGL：** 将 ReShade 6.8 安装为与游戏位数匹配的 `opengl32.dll`。
- **通过 dgVoodoo2 运行的旧版 DX8 和 DX9 游戏：** 性能优于纯 DX9。将 dgVoodoo2 的 `MS\x86\D3D9.dll`（或 `D3D8.dll`）放在游戏目录中，在 `dgVoodooCpl.exe` 中将 **Output API** 设为 Direct3D 11 或“Direct3D 12（feature level 12.0）”，再将 32 位 ReShade 安装为 `dxgi.dll`。其他 `d3d9.dll` 包装器（例如 DXVK、游戏修复补丁）会与其冲突。

### NR 的运行位置与显存

- **64 位游戏**会在游戏进程内运行 NR。首次启用后，约 165–190 MB 显存会一直由游戏占用到游戏退出，因此再次启用 NR 时无需重新加载。
- **32 位和 DX9 游戏**会在 `gitc-uplift-helper64.exe` 中运行 NR。Uplift 会在 NR 启用时启动此轻量级后台进程；关闭 NR 数秒后，进程会退出并释放其占用的显存。
- **64 位 Vulkan 游戏**会在游戏自己的 Vulkan 设备上运行 NR，包括后处理和 DLSS 阶段。首次使用后，NVIDIA 驱动会为游戏保留约 0.5–0.6 GB 显存，直到游戏退出。如果无法通过该方式运行，NR 会切换到游戏进程中的专用 DX12 设备，再切换到 `gitc-uplift-helper64.exe`。部分程序（例如会预留大部分显存的模拟器）需要最后一种方式。后两种方式只支持在后处理阶段运行 NR。
- **Vulkan 设备：** 游戏创建 Vulkan 设备时，Uplift 会为其添加三个共享扩展，使 GPU 能与游戏保持同步；当**Vulkan 后处理阶段 NR**设为“原生”时，还会添加 NR 在游戏设备上运行所需的四个 NVIDIA 扩展。如果游戏因此出现问题，Uplift 会自动先移除 NVIDIA 扩展，再移除其余扩展（见下文的 `AdjustVulkanDevicesForNgx` 和 `AdjustVulkanDevices`）。NR 仍可运行，但速度会稍慢。
- **多重采样图像（MSAA）：** DX9 和 OpenGL 需要启用 `Uplift` 技术。DX10 和 32 位 DX12 无法在 NR 下使用 MSAA；请在游戏中关闭它。

### DLSS 后图像覆盖范围

1.4.2 会在 NGX 创建功能时重写参数块的情况下，保留已提交的 DLSS 渲染尺寸和输出尺寸。此修复解决了《Forspoken》中观察到的 DLSS 后处理 NR 仅覆盖部分画面的问题。输出子区域的起点和动态渲染尺寸仍按原有方式处理。

### 后处理阶段的 DLSS 运动矢量

在 DX12 和 Vulkan 的后处理阶段，如果游戏将 DLSS 运动矢量标记为包含相机抖动，Uplift 会移除这部分抖动。计算使用游戏当前和上一帧的渲染分辨率抖动值，无需新增设置。未标记为抖动的运动矢量仍按原有方式处理。

如果遇到闪烁，请在“高级 → 日志级别”中选择“调试”，复现几秒并附上 `ReShade.log`。首次 DLSS 摘要会报告运动矢量是否含抖动；“后处理阶段运动矢量”日志会显示当前、较旧及缺失的副本、繁忙副本数量和最近一次校正量。DX12 日志还会统计流水线重置提示。

### 不同阶段的叠层处理

**层数**设为 2 或更高时，每个后续层的设置区域都会显示**运行阶段**，用于选择该层在游戏帧的哪个阶段运行（默认跟随前一层）。DLSS 前运行的层会使用游戏渲染分辨率，因此开销更低。例如，第 1 层可在 DLSS 前运行，第 2 层在后处理运行。此功能适用于原生 DX12、使用 DLSS 的 DX11，以及将**Vulkan 后处理阶段 NR**设为“原生”的 Vulkan 游戏；其他情况下所有层都会在同一阶段运行。详情请见[使用指南](https://github.com/ghostinthecamera/GITC-Uplift/wiki/Guide#passes-at-different-stages)。

### 各层可独立选择网络预设

使用 NR 310.9.16 时，第 2–10 层的设置区域会在**运行阶段**旁显示**网络预设**：

- **与第 1 层相同**会沿用“设置”中的网络预设（默认值，也适用于已有配置）。
- **A：旧版（Legacy）**或**B：小型 FP8（Small FP8）**可为该层单独选择网络。

例如，可在“设置”中选择 Legacy 并将运行阶段设为“DLSS 前”，再将第 2 层设为 Small FP8 和“DLSS 后”。**沿用第 1 层外观**只共享外观设置；网络预设和运行阶段仍可独立选择。更改网络会重建功能池并重置其时序历史。“保护面部”使用所在阶段组中第 1 层的网络。**详细信息**会显示每个当前已创建且在层数范围内的层经运行时确认的网络和已录制评估次数。NR 310.8 会禁用所有网络选择器并显示 Legacy，同时保留已保存的设置供较新运行时使用。混合网络链采用保守的显存估算；使用 Small FP8 不保证显存预留更少。

### ReShade.ini 设置项

设置保存在游戏的 `ReShade.ini` 文件的 `[Uplift]` 区段中。大多数选项都可以在 Uplift 选项卡中调整；以下列出可能需要手动设置的项目：

| 键 | 默认值 | 说明 |
|---|---|---|
| `SnippetPath` | 空 | **运行时路径**（高级）：`nvngx_dlssnr.dll` 的完整路径。留空时，依次从插件旁的 `GITC-Uplift` 文件夹、插件目录和游戏目录查找。 |
| `UseD3D9Ex` | `0` | **使用 Direct3D 9Ex**（高级）：设为 `1` 可大幅提升 DX9 游戏性能（见上文）。游戏处于独占全屏时会切换为无边框窗口。需要重启游戏。 |
| `NgxHooks` | `0` | **NGX 挂钩**（高级）：设为 `1` 即启用安全模式，不安装挂钩，NR 只在后处理阶段运行。需要重启游戏。 |
| `AdjustVulkanDevices` | `1` | 隐藏选项。设为 `0` 时不调整 Vulkan 游戏设备（见上文）。如果游戏在启用此项后启动崩溃，Uplift 会自动将其设为 `0`。需要重启游戏。 |
| `VulkanNr` | `0` | **Vulkan 后处理阶段 NR**（高级）：`0` 原生（游戏自己的设备）；`1` 专用 DX12 设备；`2` Uplift 辅助进程。每种方式都会自动回退到下一种。选择“原生”后需要重启游戏。只有原生模式支持“DLSS 后”和“DLSS 前”；选择 `1` 或 `2` 后，NR 会在后处理阶段运行，并自动切换 NR 阶段。 |
| `AdjustVulkanDevicesForNgx` | `1` | 隐藏选项。设为 `0` 时不添加原生 Vulkan NR 所需的 NVIDIA 扩展。如果游戏在启用这些扩展后启动崩溃，Uplift 会自动将其设为 `0`。需要重启游戏。 |
| `VulkanNativeNr` | `1` | 隐藏选项。设为 `0` 时跳过原生 Vulkan NR。如果原生 NR 导致游戏崩溃，Uplift 会自动将其设为 `0`；可将其重新设为 `1`，或点击“清除锁定”后重试。需要重启游戏。 |
| `AutoExposureMode` | `0` | **自动曝光**（修正，仅当**输入曝光**为“自动”时生效）：`0` 混合游戏曝光与 Uplift 测光结果；`1` 切换为仅使用游戏曝光。游戏曝光异常时始终回退到测光结果。 |
| `AutoExposureBlend` | `0.8` | **混合比例（游戏曝光到测光）**（修正），范围 0–1，界面显示为 0–100%：`0` 使用游戏曝光，`1` 使用测光结果。数值越低越接近游戏原有观感；数值越高，不同场景间的曝光越平稳。 |
| `GraceSeconds` | `5` | **宽限时间（秒）**（高级）：关闭 NR 后保留其显存的时长，避免快速切换时重新加载。 |
| `VramCheck` | `0` | **显存检查**（高级）：`0` 谨慎，为游戏保留显存预算，并在游戏需要显存时暂停 NR；`1` 宽松，允许 NR 超出游戏显存预算约 1 GB；`2` 关闭检查，行为与其他 DLSS-NR 插件相同。在显存占用较高的游戏中，宽松和关闭模式可能造成卡顿。 |
| `KeepFaces` | `0` | **保护面部**（外观）：设为 `1` 后保留角色光照和阴影，同时避免 NR 改变面部形状。每帧会额外运行一层 NR，4K 下约需额外 1 GB 显存。 |
| `Pass2Preset` … `Pass10Preset` | `0` | 各层的网络预设：`0` 沿用“设置”中的预设，`1` 旧版（Legacy），`2` 小型 FP8（Small FP8）。与是否沿用第 1 层外观相互独立；需 NR 310.9.16。 |
| `Pass2Stage` … `Pass10Stage` | `0` | 各后续层的**运行阶段**：`0` 与前一层相同；`1` DLSS 前；`2` DLSS 后；`3` 后处理。某一层不会运行在前一层之前；如果指定阶段当前不可用，则会切换到距离最近的可用阶段。 |
| `ResolutionAfterDlss`、`ResolutionPresent` | `0` | **DLSS 后分辨率** / **后处理阶段分辨率**：当第 1 层运行在更早阶段时，用于设置这些阶段的处理分辨率。`0` 全分辨率；`1` 质量；`2` 均衡；`3` 性能；`4` 匹配游戏；`5` 自定义。 |
| `ResolutionScaleAfterDlss`、`ResolutionScalePresent` | `100` | 上述阶段的**自定义比例**，范围 25–100%。 |
| `KeepFacesAfterDlss`、`KeepFacesPresent` | `0` | **在此阶段保护面部**：启用“保护面部”时，设为 `1` 可在该阶段也保护面部。每帧会额外运行一层 NR。 |
| `FaceProtection` | `0` | **面部保护强度**（外观），范围 0–1：控制 NR 对面部细节结构的保留程度。`0` 保留游戏原始面部；`1` 完全应用 NR 的结构变化。 |
| `LightingScale` | `2.5` | **光照尺度（画面高度百分比）**（外观），范围 0.5–10：角色身上尺度大于此值的 NR 变化会被视为光照并保留。数值越大，只保留范围更广的光照变化；数值越小，面部上保留的 NR 变化越多。 |
| `ShowFaceMask` | `0` | **显示面部蒙版**（高级）：设为 `1` 后以洋红色显示“保护面部”实际保护的区域，方便调节。 |
| `FaceMaskThreshold` | `1.5` | **蒙版阈值（亮度百分比）**（高级），范围 0–10：如果背景或高亮反光处出现零散蒙版，提高此值；如果面部着色太淡，则降低此值。 |
| `FaceMaskFull` | `6` | **达到完全保护的亮度值（百分比）**（高级），范围 1–30：如果面部只被部分着色，降低此值；如果头发或衣服也被着色，提高此值。 |
| `FaceMaskStrength` | `2` | **蒙版强度**（高级），范围 0.5–4：如果面部蒙版内部有空洞，提高此值；如果蒙版范围过大，则降低此值。 |
| `FaceMaskSoftness` | `0.5` | **蒙版柔化（画面高度百分比）**（高级），范围 0.1–2：如果蒙版斑驳或闪烁，提高此值；如果蒙版蔓延到面部以外，则降低此值。 |
| `FaceEdgeFalloff` | `0.15` | **边缘衰减（画面高度百分比）**（高级），范围 0.05–1：如果光照溢出到角色轮廓外，降低此值；如果轮廓出现暗边，则提高此值。 |
| `FaceLightingDensity` | `0.3` | **光照密度**（高级），范围 0.05–1：如果高光或明亮边缘使角色出现亮点，提高此值；如果面部边缘附近的光照消失，则降低此值。 |
| `FaceSpeckSize` | `2` | **去除亮点（像素）**（高级），范围 0–3：不超过此尺寸的亮点（反光、闪光）会保留 NR 的常规结果。尺寸越大，越可能停止保护微小面部细节（例如远处的人眼）。`0` 表示关闭。 |
| `FaceFillSkin` | `1` | **按颜色填充皮肤区域**（高级）：设为 `1` 后，会将蒙版扩展到附近颜色相同的皮肤区域（例如阴影中的皮肤）。如果类似肤色的衣物或头发变成洋红色，请设为 `0`。 |
| `FaceFillRadius` | `3` | **填充半径（画面高度百分比）**（高级），范围 0.5–10：控制填充从已标记皮肤向外延伸的距离。大块皮肤仍未覆盖时可提高此值；填充蔓延到附近表面时可降低此值。 |
| `FaceFillTolerance` | `0.04` | **颜色容差**（高级），范围 0.005–0.2：阴影中的皮肤仍未覆盖时可提高此值；衣物或头发变成洋红色时可降低此值。 |

### 状态卡的其他提示

- **“与旁边的 Uplift 插件版本不一致”：** 插件和 `gitc-uplift-helper64.exe` 来自不同发行版。请从同一发行版重新复制两者。
- **“NR 可用显存不足”**或**“已暂停：游戏需要更多显存”：** 降低**分辨率**或**层数**，关闭其他 GPU 程序，或在“高级”中将**显存检查**设为**宽松**或**关闭**。
- **“保护面部已暂停”：** 显存不足以支持额外的 NR 处理层。NR 会在不保护面部的情况下继续运行；显存充足后会自动恢复。请降低**分辨率**或**层数**以腾出显存。
- **“保护面部已停止”：** 额外的 NR 处理失败，但 NR 会在不保护面部的情况下继续运行。取消勾选后重新勾选**保护面部**即可重试。
- **“DLSS 后运行 NR 已停用”**（GPU 崩溃后）：NR 会保持在后处理阶段。点击**清除锁定**后，从下次启动起会重新尝试在 DLSS 后运行。
- Uplift 在 `ReShade.log` 中的日志行以 `[Uplift]` 开头；如需更多信息，请在“高级”中将**日志级别**设为“调试”。64 位游戏日志末尾出现 `Add-on "GITC Uplift" was not unregistered!` 属于正常情况。

## 从源码构建

需要准备：

- **Visual Studio 2026** 或 **2022**（17.14），任意版本，并安装 **使用 C++ 的桌面开发**工作负载（其中包含 CMake、Ninja 和 Windows SDK；Windows SDK 提供用于编译 Direct3D 着色器的 `dxc.exe`）；
- **Vulkan SDK**（设置 `VULKAN_SDK` 环境变量），用于提供头文件和编译 SPIR-V 着色器所需的 `dxc.exe`；
- **Git**，以及首次构建时所需的网络连接。

然后在命令提示符中执行以下命令（在 PowerShell 中输入 `.\build.cmd` 和 `.\tools\fetch_deps.cmd`）：

```bat
git clone https://github.com/ghostinthecamera/GITC-Uplift.git
cd GITC-Uplift
tools\fetch_deps.cmd
build.cmd release
build.cmd release-x86
```

- `tools\fetch_deps.cmd` 会将构建所需的头文件下载到 `third_party\_deps`（NVIDIA NGX SDK 头文件，不含 NVIDIA 二进制文件；ReShade 6.0.0 插件头文件和 Dear ImGui；MinHook）。
- `build.cmd release` 会生成 `build\release\gitc-uplift.addon64` 和 `build\release\gitc-uplift-helper64.exe`。
- `build.cmd release-x86` 会生成 `build\release-x86\gitc-uplift.addon32`。
- 插件和辅助进程会检查彼此的构建 ID，因此必须始终使用同一次构建生成的文件。

## 许可证

GITC Uplift 基于 MIT 许可证发布，详见 [LICENSE](LICENSE)。其衍生代码或依赖项所适用的许可证见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。NVIDIA、GeForce RTX 和 DLSS 均为 NVIDIA Corporation 的商标。
