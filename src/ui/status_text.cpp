#include "ui/status_text.hpp"
#include "nr/types.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <utility>

namespace uplift::ui {

SetupView ResolveSetup(const SetupFacts& facts) {
  SetupView view;
  const auto at = [](auto& options, auto value) -> OptionState& { return options[static_cast<size_t>(value)]; };
  const auto selectable = [](const OptionState& option) { return option.why.empty(); };
  const auto grey_for_dlss = [&facts](OptionState* option) {
    if (!facts.dlss_unavailable.empty()) {
      *option = {.why = facts.dlss_unavailable};
    } else if (!facts.dlss_seen) {
      *option = {.why = DLSS_NOT_SEEN_REASON, .temporary = true};
    } else if (facts.dlss_off) {
      *option = {.why = DLSS_OFF_REASON, .temporary = true};  // Plan 18 Task 12
    }
  };

  // NR stage.
  grey_for_dlss(&at(view.stage_options, SourcePick::AFTER_DLSS));
  OptionState& before = at(view.stage_options, SourcePick::BEFORE_UPSCALING);
  grey_for_dlss(&before);
  if (selectable(before) && facts.ray_reconstruction && facts.ray_reconstruction_after_dlss) {
    before = {.why = "游戏使用 Ray Reconstruction，因此 NR 需在 DLSS 后运行", .temporary = true};
  }
  view.before_upscaling_on_rr = (facts.ray_reconstruction && !facts.ray_reconstruction_after_dlss);
  // Plan 18: a stage whose image the Direct3D 11 bridge cannot share is a fixed cause for that stage alone; it beats the temporary ones.
  const auto grey_fixed = [](OptionState* option, std::string_view fixed) {
    if (!fixed.empty() && (option->why.empty() || option->temporary)) {
      *option = {.why = fixed};
    }
  };
  grey_fixed(&at(view.stage_options, SourcePick::AFTER_DLSS), facts.after_dlss_fixed);
  grey_fixed(&before, facts.before_upscaling_fixed);
  if (!facts.present_fixed.empty()) {
    at(view.stage_options, SourcePick::PRESENT) = {.why = facts.present_fixed};  // a fixed cause beats the temporary one: it would not clear
  } else if (facts.frame_generation_blocks_present) {
    at(view.stage_options, SourcePick::PRESENT) = {.why = "帧生成已开启：请在“高级”中启用“帧生成期间处理后处理”", .temporary = true};
  }
  // Plan 15: held after the game's NGX shutdown on Direct3D 12, no stage can run until the game's DLSS starts again. That beats the other temporary causes
  // (it is why nothing runs now); a fixed cause keeps its own text, since it would not clear with the hold.
  const auto hold = [&facts](OptionState* option) {
    if (!facts.held.empty() && (option->why.empty() || option->temporary)) {
      *option = {.why = facts.held, .temporary = true};
    }
  };
  for (OptionState& option : view.stage_options) {
    hold(&option);
  }
  const OptionState stage_wish = at(view.stage_options, facts.preferred_stage);  // the preference's option, before the running one is un-greyed
  if (facts.stage) {
    view.stage = *facts.stage;
  } else if (selectable(stage_wish)) {
    view.stage = facts.preferred_stage;
  } else if (facts.preferred_stage == SourcePick::BEFORE_UPSCALING && selectable(at(view.stage_options, SourcePick::AFTER_DLSS))) {
    view.stage = SourcePick::AFTER_DLSS;  // Ray Reconstruction keeps NR after DLSS
  } else {
    view.stage = SourcePick::PRESENT;
  }

  // Motion vectors (Launchpad depends on the stage just decided).
  OptionState& dlss = at(view.motion_options, MotionPick::DLSS);
  grey_for_dlss(&dlss);
  if (dlss.why.empty() || dlss.temporary) {
    // A fixed reason beats "not seen yet": it would not clear when DLSS starts.
    if (!facts.dlss_motion_fixed.empty()) {
      dlss = {.why = facts.dlss_motion_fixed};
    } else if (selectable(dlss) && !facts.game_passes_motion) {
      dlss = {.why = "游戏未向 DLSS 传递运动矢量", .temporary = true};
    }
  }
  OptionState& launchpad = at(view.motion_options, MotionPick::LAUNCHPAD);
  if (!facts.launchpad_fixed.empty()) {
    launchpad = {.why = facts.launchpad_fixed};  // a fixed cause beats the temporary ones: it would not clear
  } else if (view.stage != SourcePick::PRESENT && !facts.present_fixed.empty()) {
    launchpad = {.why = facts.present_fixed};  // Launchpad runs at Present only, which cannot run again
  } else if (view.stage != SourcePick::PRESENT) {
    launchpad = {.why = "LaunchPad 仅能在后处理阶段为 NR 提供运动矢量", .temporary = true};
  } else if (!facts.launchpad_ready) {
    launchpad = {.why = "请启用 iMMERSE LaunchPad，并在其下方启用 Uplift.fx 的 Uplift 技术", .temporary = true};
  }
  // 2026-10-08: Lumenite, as Launchpad.
  OptionState& lumenite = at(view.motion_options, MotionPick::LUMENITE);
  if (!facts.lumenite_fixed.empty()) {
    lumenite = {.why = facts.lumenite_fixed};
  } else if (view.stage != SourcePick::PRESENT && !facts.present_fixed.empty()) {
    lumenite = {.why = facts.present_fixed};
  } else if (view.stage != SourcePick::PRESENT) {
    lumenite = {.why = "Lumenite 仅能在后处理阶段为 NR 提供运动矢量", .temporary = true};
  } else if (!facts.lumenite_ready) {
    lumenite = {.why = "请启用 LumeniteFX 的 Kernel，并在其下方启用 Uplift.fx 的 Uplift 技术", .temporary = true};
  }
  hold(&dlss);
  hold(&launchpad);
  hold(&lumenite);
  const OptionState motion_wish = at(view.motion_options, facts.preferred_motion);
  MotionPick resolved_motion = MotionPick::OFF;
  if (selectable(motion_wish)) {
    resolved_motion = facts.preferred_motion;
  } else if (facts.motion_auto && selectable(launchpad)) {
    resolved_motion = MotionPick::LAUNCHPAD;
  } else if (facts.motion_auto && selectable(lumenite)) {
    resolved_motion = MotionPick::LUMENITE;  // Auto's order: DLSS, Launchpad, Lumenite
  } else if ((facts.preferred_motion == MotionPick::LAUNCHPAD || facts.preferred_motion == MotionPick::LUMENITE) && view.stage != SourcePick::PRESENT
             && selectable(dlss)) {
    resolved_motion = MotionPick::DLSS;  // inside the game's frame a Launchpad or Lumenite preference runs on the game's own vectors (the engine's rule)
  }
  view.motion = facts.motion.value_or(resolved_motion);

  // Resolution.
  OptionState& match_game = at(view.resolution_options, ResolutionMode::MATCH_GAME);
  if (!facts.match_game_readable && facts.match_game_at_dlss_stages) {
    match_game = {.why = VULKAN_MATCH_GAME_REASON, .temporary = true};  // final review, minor 3: a DLSS stage here reads it
  } else if (!facts.match_game_readable) {
    match_game = {.why = "“匹配游戏”需要游戏的 DLSS 渲染尺寸，但 Uplift 在此处无法读取"};
  } else if (!facts.dlss_seen) {
    match_game = {.why = DLSS_NOT_SEEN_REASON, .temporary = true};
  } else if (facts.dlss_off) {
    match_game = {.why = DLSS_OFF_REASON, .temporary = true};  // Plan 18 Task 12 (fix round 1, M-1): it reads DLSS's render size, which is off too
  }
  if (!facts.below_full_fixed.empty()) {
    for (size_t index = 1u; index < view.resolution_options.size(); ++index) {
      view.resolution_options[index] = {.why = facts.below_full_fixed};
    }
  }
  const OptionState resolution_wish = at(view.resolution_options, facts.preferred_resolution);
  view.resolution = facts.resolution.value_or(selectable(resolution_wish) ? facts.preferred_resolution : ResolutionMode::FULL);

  // What runs is possible by definition: its option is never greyed.
  at(view.stage_options, view.stage) = {};
  at(view.motion_options, view.motion) = {};
  at(view.resolution_options, view.resolution) = {};

  // Split chain (Task 3): passes 2..N's "Runs at" rows. Where each pass runs is the context's chain; without one, the stored picks from pass 1's highlight,
  // raised to the pass before's stage (DecideChain's never-backwards rule; its fallbacks need the engine's facts).
  const uint32_t passes = std::clamp(facts.pass_count, 1u, MAX_PASSES);
  std::array<SourcePick, MAX_PASSES> runs = {};
  runs.fill(view.stage);
  if (facts.chain) {
    runs = *facts.chain;
  } else if (facts.split_capable) {
    for (uint32_t pass = 1u; pass < passes; ++pass) {
      const PassStage stored = facts.stored_stages[pass - 1u];
      runs[pass] = runs[pass - 1u];
      if (stored != PassStage::SAME) {
        runs[pass] = std::max(runs[pass - 1u], static_cast<SourcePick>(static_cast<uint32_t>(stored) - 1u));
      }
    }
  }
  view.split_capable = facts.split_capable;
  const auto runs_at = [](SourcePick stage) -> std::string_view {
    switch (stage) {
      case SourcePick::BEFORE_UPSCALING: return "DLSS 前";
      case SourcePick::AFTER_DLSS:       return "DLSS 后";
      case SourcePick::PRESENT:          break;
    }
    return "后处理阶段";
  };
  for (uint32_t pass = 1u; pass < passes; ++pass) {
    PassStageRow& row = view.pass_stages[pass - 1u];
    const SourcePick pass_before = runs[pass - 1u];
    row.runs = runs[pass];
    row.starts_group = (facts.split_capable && runs[pass] != pass_before);
    view.chain_split = view.chain_split || (runs[pass] != runs[0]);
    const PassStage stored = facts.stored_stages[pass - 1u];
    const auto runs_pick = static_cast<PassStage>(static_cast<uint32_t>(runs[pass]) + 1u);
    const bool runs_as_stored = (stored == runs_pick || (stored == PassStage::SAME && runs[pass] == pass_before));
    row.shown = (runs_as_stored ? stored : runs_pick);
    if (!facts.split_capable) {
      for (PassStageOption& option : row.options) {
        option = {.why = std::string(ONE_STAGE_REASON)};  // the highlight too: nothing here is a choice
      }
      continue;
    }
    for (uint32_t stage = 0u; stage < static_cast<uint32_t>(view.stage_options.size()); ++stage) {
      PassStageOption& option = row.options[stage + 1u];
      const auto pick = static_cast<SourcePick>(stage);
      const OptionState& setup_option = view.stage_options[stage];
      if (pick < pass_before) {
        option = {.why = std::format("会导致叠层顺序倒置：第 {} 层将运行于{}", pass, runs_at(pass_before))};
      } else if (pick != pass_before && !setup_option.why.empty()) {
        option = {.why = std::string(setup_option.why), .temporary = setup_option.temporary};  // never the pass before's stage, which always takes it
      }
    }
    row.options[static_cast<size_t>(row.shown)] = {};  // what runs is possible by definition
  }

  // Plan 15 fix round (minor 6): stopped for the session, nothing can run, the highlights included: every option of the three rows is greyed with the fixed
  // reason (the card says it too), and there is no Why line. Split chain fix round 1 (M5): the Runs at rows' too.
  if (!facts.stopped.empty()) {
    view.stopped = true;
    const auto stop = [&facts](auto& options) {
      for (OptionState& option : options) {
        option = {.why = facts.stopped};
      }
    };
    stop(view.stage_options);
    stop(view.motion_options);
    stop(view.resolution_options);
    for (PassStageRow& row : view.pass_stages) {  // fix round 1 (M5): the Runs at rows too
      for (PassStageOption& option : row.options) {
        option = {.why = std::string(facts.stopped)};
      }
    }
    return view;
  }
  // Plan 15: while held, the card says why NR waits (CardFacts::held), and a held context's card is never the working one, which alone draws the Why line.
  if (!facts.held.empty()) return view;

  // The Why line: temporary fallbacks from the preference only (a fixed cause has its tooltip). The first sentence as written (it starts with a name),
  // later ones with a capital.
  const auto add = [&view](std::string sentence) {
    if (!view.why.empty()) {
      sentence[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(sentence[0])));
      view.why += ' ';
    }
    view.why += sentence;
  };
  // A DLSS stage waits for the game's DLSS: not seen yet, or seen (a create) but not evaluated yet, or Vulkan's hand-off from Present.
  const bool stage_waits = (view.stage != facts.preferred_stage && facts.preferred_stage != SourcePick::PRESENT
                            && (selectable(stage_wish) || stage_wish.why == DLSS_NOT_SEEN_REASON));
  const bool motion_falls_back = (facts.preferred_motion == MotionPick::DLSS && view.motion != MotionPick::DLSS);
  const bool motion_waits = (motion_falls_back
                             && (motion_wish.why == DLSS_NOT_SEEN_REASON || (selectable(motion_wish) && facts.dlss_motion_gap == MotionGap::NONE)));
  const std::string_view stage_name = (facts.preferred_stage == SourcePick::BEFORE_UPSCALING ? "DLSS 前" : "DLSS 后");
  // Plan 18 Task 12: the game switched its DLSS off: what fell back returns when it is on again.
  const bool stage_off = (view.stage != facts.preferred_stage && facts.preferred_stage != SourcePick::PRESENT && stage_wish.why == DLSS_OFF_REASON);
  const bool motion_off = (motion_falls_back && motion_wish.why == DLSS_OFF_REASON);
  if (stage_off) {
    add(motion_off ? std::format("{}阶段和 DLSS 运动矢量会在游戏重新启用 DLSS 后恢复。", stage_name)
                   : std::format("{}阶段会在游戏重新启用 DLSS 后恢复。", stage_name));
  } else if (stage_waits) {
    add(motion_waits ? std::format("{}阶段和 DLSS 运动矢量会在游戏使用 DLSS 渲染时启动。", stage_name)
                     : std::format("{}阶段会在游戏使用 DLSS 渲染时启动。", stage_name));
  } else if (view.stage != facts.preferred_stage && stage_wish.temporary && facts.preferred_stage == SourcePick::BEFORE_UPSCALING) {
    add("游戏不再使用 Ray Reconstruction 时，DLSS 前阶段会恢复。");
  }
  if (motion_off) {
    if (!stage_off) {
      add("游戏重新启用 DLSS 后，DLSS 运动矢量会恢复。");  // else said with the stage above
    }
  } else if (motion_waits && !stage_waits) {
    add("游戏使用 DLSS 渲染时，DLSS 运动矢量会启用。");
  } else if (motion_falls_back && (facts.dlss_motion_gap == MotionGap::GAME_PASSED_NONE || (motion_wish.temporary && !motion_waits))) {
    add("游戏未向 DLSS 传递运动矢量。");
  } else if (motion_falls_back && selectable(motion_wish) && facts.dlss_motion_gap == MotionGap::NO_COPY) {
    add("DLSS 运动矢量副本未能送达后处理路径（GPU 执行进度落后过多）。");
  } else if (motion_falls_back && selectable(motion_wish) && facts.dlss_motion_gap == MotionGap::DLSS_PAUSED) {
    add("游戏重新运行 DLSS 后，DLSS 运动矢量会恢复（菜单或加载画面等场景下，DLSS 可能会暂停运行）。");
  }
  if (facts.preferred_motion == MotionPick::LAUNCHPAD && view.motion != MotionPick::LAUNCHPAD) {
    if (motion_wish.temporary && view.stage != SourcePick::PRESENT) {
      add("NR 在后处理阶段运行时会恢复使用 LaunchPad 的运动矢量；在游戏帧内运行时则使用游戏自己的运动矢量。");
    } else if (motion_wish.temporary) {
      add("启用 LaunchPad 和 Uplift.fx 的 Uplift 技术后，会恢复使用 LaunchPad 的运动矢量。");
    } else if (selectable(motion_wish) && facts.launchpad_gap == MotionGap::NO_UPLIFT_MV) {
      add("未收到 UPLIFT_MV：请将 Uplift.fx 的 Uplift 技术放在 LaunchPad 下方。");
    }
  }
  if (facts.preferred_motion == MotionPick::LUMENITE && view.motion != MotionPick::LUMENITE) {
    if (motion_wish.temporary && view.stage != SourcePick::PRESENT) {
      add("NR 在后处理阶段运行时会恢复使用 Lumenite 的运动矢量；在游戏帧内运行时则使用游戏自己的运动矢量。");
    } else if (motion_wish.temporary) {
      add("启用 Lumenite 的 Kernel 和 Uplift.fx 的 Uplift 技术后，会恢复使用 Lumenite 的运动矢量。");
    } else if (selectable(motion_wish) && facts.launchpad_gap == MotionGap::NO_UPLIFT_MV) {
      add("未收到 UPLIFT_MV：请将 Uplift.fx 的 Uplift 技术放在 Lumenite 的 Kernel 下方。");
    }
  }
  if (view.resolution != facts.preferred_resolution && facts.preferred_resolution == ResolutionMode::MATCH_GAME
      && (selectable(resolution_wish) || resolution_wish.why == DLSS_NOT_SEEN_REASON)) {
    add("游戏使用 DLSS 渲染时，“匹配游戏”设置会生效。");
  } else if (view.resolution != facts.preferred_resolution && facts.preferred_resolution == ResolutionMode::MATCH_GAME
             && resolution_wish.why == VULKAN_MATCH_GAME_REASON) {
    add("NR 在 DLSS 后或 DLSS 前运行时，“匹配游戏”设置会生效。");
  } else if (view.resolution != facts.preferred_resolution && facts.preferred_resolution == ResolutionMode::MATCH_GAME
             && resolution_wish.why == DLSS_OFF_REASON) {
    add("游戏重新启用 DLSS 后，“匹配游戏”设置会生效。");  // Plan 18 Task 12 (fix round 1, M-1)
  }
  return view;
}

LiveCard BuildLiveCard(const SetupFacts& facts, const SetupView& shown) {
  LiveCard card;
  switch (shown.stage) {
    case SourcePick::BEFORE_UPSCALING: card.stage = "DLSS 前"; break;
    case SourcePick::AFTER_DLSS:       card.stage = "DLSS 后"; break;
    case SourcePick::PRESENT:          card.stage = "后处理"; break;
  }
  switch (shown.motion) {
    case MotionPick::OFF:       card.motion = "不使用"; break;
    case MotionPick::DLSS:      card.motion = (facts.motion_copied ? "DLSS（游戏提供，已复制至后处理）" : "DLSS（游戏提供）"); break;
    case MotionPick::LAUNCHPAD: card.motion = "LaunchPad"; break;
    case MotionPick::LUMENITE:  card.motion = "Lumenite"; break;
  }
  std::string mode;
  switch (shown.resolution) {
    case ResolutionMode::FULL:        mode = "全分辨率"; break;
    case ResolutionMode::QUALITY:     mode = "质量"; break;
    case ResolutionMode::BALANCED:    mode = "均衡"; break;
    case ResolutionMode::PERFORMANCE: mode = "性能"; break;
    case ResolutionMode::MATCH_GAME:  mode = "匹配游戏"; break;
    case ResolutionMode::CUSTOM:      mode = std::format("自定义 {:.0f} %", facts.custom_scale); break;
  }
  const std::string_view upsampling = (facts.upsampling == color::Upsampling::CLASSIC ? "经典升采样" : "边缘感知升采样");
  const nr::Size work = facts.work;
  const bool full_size = (facts.frame.Empty() || work == facts.frame);
  if (work.Empty()) {
    card.resolution = mode;
  } else if (shown.stage == SourcePick::BEFORE_UPSCALING) {
    if (!facts.canvas.Empty() && facts.canvas != work) {
      card.resolution = std::format("{}x{}（{}，已将 {}x{} 渲染画面补齐）", facts.canvas.width, facts.canvas.height, mode, work.width, work.height);
    } else if (full_size) {
      card.resolution = std::format("{}x{}（{}，渲染分辨率）", work.width, work.height, mode);
    } else {
      card.resolution = std::format("{}x{} / {}x{} 渲染画面（{}，{}）", work.width, work.height, facts.frame.width, facts.frame.height, mode, upsampling);
    }
  } else if (full_size) {
    card.resolution = std::format("{}x{}（{}）", work.width, work.height, mode);
  } else {
    card.resolution = std::format("{}x{} / {}x{}（{}，{}）", work.width, work.height, facts.frame.width, facts.frame.height, mode, upsampling);
  }
  for (const std::string_view note : {facts.latch_note, facts.passes_note, facts.chain_note, facts.frame_generation_line}) {
    if (!note.empty()) {
      card.note = std::string(note);
      break;
    }
  }
  card.footer = std::format("{} · {}/{} 层", facts.api, facts.passes_run, facts.passes_requested);
  if (facts.vram_bytes > 0u) {
    card.footer += std::format(" · 显存 {:.0f} MiB", static_cast<double>(facts.vram_bytes) / (1024.0 * 1024.0));
  }
  return card;
}

SetupView SetupSettle::Update(SetupView resolved, std::chrono::steady_clock::time_point now, std::chrono::milliseconds settle) {
  if (!last_update_ || now - *last_update_ > 2 * settle) {
    rows_ = {};  // the first draw, or the overlay was closed for a while: what runs shows at once
  }
  last_update_ = now;
  const std::array<uint32_t, 3> wanted = {
      static_cast<uint32_t>(resolved.stage),
      static_cast<uint32_t>(resolved.motion),
      static_cast<uint32_t>(resolved.resolution),
  };
  std::array<uint32_t, 3> shown = {};
  bool settling = false;
  for (size_t index = 0u; index < rows_.size(); ++index) {
    Row& row = rows_[index];
    if (!row.shown || row.shown == wanted[index]) {
      row.shown = wanted[index];
      row.candidate.reset();
    } else {
      if (row.candidate != wanted[index]) {
        row.candidate = wanted[index];
        row.since = now;
      }
      if (now - row.since >= settle) {
        row.shown = wanted[index];
        row.candidate.reset();
      }
    }
    shown[index] = *row.shown;
    settling = settling || (*row.shown != wanted[index]);
  }
  resolved.stage = static_cast<SourcePick>(shown[0]);
  resolved.motion = static_cast<MotionPick>(shown[1]);
  resolved.resolution = static_cast<ResolutionMode>(shown[2]);
  // What is shown is what ran a moment ago: never greyed, and the Why belongs to the resolved highlights. Plan 15 fix round: unless nothing can run again.
  if (!resolved.stopped) {
    resolved.stage_options[shown[0]] = {};
    resolved.motion_options[shown[1]] = {};
    resolved.resolution_options[shown[2]] = {};
  }
  if (settling) {
    resolved.why.clear();
  }
  return resolved;
}

void SetupSettle::Clicked(SetupRow row, uint32_t index) {
  Row& clicked = rows_[static_cast<size_t>(row)];
  clicked.shown = index;
  clicked.candidate.reset();
}

void SetupSettle::ClickedBest(SetupRow row, uint32_t index, const SetupView& options, bool motion_auto) {
  const auto runs = [](const auto& states, auto pick) { return states[static_cast<size_t>(pick)].why.empty(); };
  uint32_t best = index;
  switch (row) {
    case SetupRow::STAGE:
      if (!runs(options.stage_options, index)) {
        const bool after_dlss = (index == static_cast<uint32_t>(SourcePick::BEFORE_UPSCALING) && runs(options.stage_options, SourcePick::AFTER_DLSS));
        best = static_cast<uint32_t>(after_dlss ? SourcePick::AFTER_DLSS : SourcePick::PRESENT);
      }
      break;
    case SetupRow::MOTION:
      if (!runs(options.motion_options, index)) {
        const std::optional<uint32_t>& stage = rows_[static_cast<size_t>(SetupRow::STAGE)].shown;
        const bool present = (!stage || *stage == static_cast<uint32_t>(SourcePick::PRESENT));
        const bool launchpad = (motion_auto && present && runs(options.motion_options, MotionPick::LAUNCHPAD));
        const bool lumenite = (motion_auto && present && runs(options.motion_options, MotionPick::LUMENITE));  // Auto's order
        best = static_cast<uint32_t>(launchpad ? MotionPick::LAUNCHPAD : lumenite ? MotionPick::LUMENITE : MotionPick::OFF);
      }
      break;
    case SetupRow::RESOLUTION:
      if (!runs(options.resolution_options, index)) {
        best = static_cast<uint32_t>(ResolutionMode::FULL);
      }
      break;
  }
  Clicked(row, best);
}

std::string FormatStatusLine(const StatusView& view) {
  std::string line;
  switch (view.state) {
    case nr::SessionState::OFF: line = "已关闭"; break;
    case nr::SessionState::LOADING: line = "正在加载"; break;
    case nr::SessionState::ACTIVE: line = "运行中"; break;
    case nr::SessionState::GRACE: line = "等待释放显存"; break;
    case nr::SessionState::DRAINING: line = "正在释放显存"; break;
    case nr::SessionState::FAILED: line = "运行失败"; break;
  }
  if (view.suspended) {
    line += "（已暂停：游戏需要显存）";
  }
  if (view.state == nr::SessionState::GRACE) {
    // Round up to whole seconds: a caller must never see "released in 0 s" while still counting down.
    const auto seconds_left = (view.grace_remaining.count() + 999) / 1000;
    line += std::format("：NR 将在 {} 秒后释放显存", seconds_left);
    return line;
  }
  if (view.state != nr::SessionState::ACTIVE) return line;
  line += std::format(" | 已运行 {}/{} 层", view.passes_run, view.passes_requested);
  if (!view.frame.Empty()) {
    line += std::format(" | {}x{}", view.frame.width, view.frame.height);
  }
  if (!view.trigger.empty()) {
    line += std::format(" | {}", view.trigger);
  }
  if (view.encoding) {
    line += std::format(" | {}", color::EncodingName(*view.encoding));
    if (*view.encoding != color::Encoding::SRGB && view.diffuse_white_nits > 0.f) {
      line += std::format("，漫反射白 {} 尼特", view.diffuse_white_nits);
    }
  }
  return line;
}

std::string FormatFrameGenerationLine(bool active, uint32_t multiplier, bool from_ngx) {
  if (!active) return "帧生成：关闭";
  return std::format("帧生成：开启（{}x{}）", multiplier, (from_ngx ? "，NGX" : ""));
}

std::optional<std::string> FrameGenerationWarning(bool active, uint32_t passes) {
  if (!active || passes <= 1u) return std::nullopt;
  return std::format("帧生成已开启：运行 {} 层 NR 可能超出帧预算；建议使用 1 层", passes);
}

std::string_view KeepFacesUnavailable(const KeepFacesFacts& facts) {
  if (!facts.nr_runs) return "NR 不会在此游戏中运行";
  if (!facts.gpu_ready) return "此 GPU 无法运行“保护面部”（原因详见 ReShade.log）";
  return {};
}

std::string_view LocalizeStatusReason(std::string_view reason) {
  if (reason == "off") return "NR 已关闭";
  if (reason == "loading") return "正在加载 NR";
  if (reason == "grace") return "NR 处于宽限期";
  if (reason == "draining") return "正在等待 NR 处理任务完成";
  if (reason == "failed") return "NR 运行失败";
  if (reason == "budget") return "可用显存不足";
  if (reason == "frame too small") return "当前画面尺寸低于 NR 的最低要求";
  if (reason == "resizing") return "正在等待画面尺寸调整完成";
  if (reason == "evaluate failed") return "NR 评估失败";
  if (reason == "create failed") return "NR 创建失败";
  if (reason == "frame generation is on") return "帧生成已开启";
  if (reason == "device removed") return "设备已移除";
  if (reason == "not placed after DLSS") return "运动矢量仅能在 DLSS 后处理阶段使用";
  if (reason == "NR is off") return "NR 已关闭";
  if (reason == "invalid input") return "输入参数无效";
  if (reason == "list state unknown") return "无法确认命令列表状态";
  if (reason == "DLSS runs on a compute queue") return "DLSS 正在计算队列上运行";
  if (reason == "unsupported format") return "不支持的格式";
  if (reason == "intensity 0") return "处理强度为 0";
  if (reason == "inactive") return "NR 当前未运行";
  if (reason == "no pipeline for the target format") return "目标格式没有可用的处理管线";
  if (reason == "already ran this frame") return "本帧已处理";
  if (reason == "descriptors busy") return "资源描述符仍在使用中";
  if (reason == "encode failed") return "编码失败";
  if (reason == "decode failed") return "解码失败";
  if (reason == "pipeline failed") return "处理管线创建失败";
  if (reason == "out of memory") return "显存不足";
  if (reason == "motion vectors outside their texture") return "运动矢量区域超出纹理范围";
  if (reason == "not DLSS-SR or Ray Reconstruction") return "当前功能不是 DLSS-SR 或光线重建";
  if (reason == "no Launchpad vectors this frame") return "本帧未收到 LaunchPad 运动矢量";
  if (reason == "waiting for the earlier stages' NR") return "正在等待前序阶段的 NR 完成";
  if (reason == "no readable motion vectors") return "未检测到可读取的运动矢量";
  if (reason == "the game's present queue is still busy") return "游戏的呈现队列仍在执行命令";
  if (reason == "a submission on the game's present queue failed") return "向游戏的呈现队列提交命令失败";
  if (reason == "Uplift's command buffers are busy") return "Uplift 的命令缓冲区仍在 GPU 上执行";
  if (reason == "Uplift's command buffer could not be begun") return "无法开始记录 Uplift 的命令缓冲区";
  if (reason == "Uplift's command buffer could not be submitted") return "无法提交 Uplift 的命令缓冲区";
  if (reason == "no DLSS colour to swap") return "没有可供替换的 DLSS 颜色缓冲区";
  if (reason == "preparing the new format") return "正在准备新的格式";
  if (reason == "NGX shut down") return "NGX 已关闭";
  if (reason == "device lost") return "设备已丢失";
  return reason;
}

StatusCard BuildStatusCard(const CardFacts& facts) {
  const auto card = [](CardStage stage, std::string title, std::string_view reason, std::vector<std::string> fixes,
                       CardButton button) {
    return StatusCard{.stage = stage, .working = false, .title = std::move(title), .reason = std::string(reason),
                      .fixes = std::move(fixes), .button = button};
  };
  if (!facts.device_problem.empty()) {
    return card(CardStage::DEVICE, "Uplift 未在此设备上运行", facts.device_problem, {}, CardButton::NONE);
  }
  // Only while NR is on: a user who turned it off sees the normal off state, and the failure (remembered by the add-on) comes
  // back with Retry now when they turn it on again.
  // Plan 17: the 2-strike rule's second stop, for a bridge's private device and the helper alike.
  if (facts.private_stops_final) {
    return card(CardStage::DEVICE, "NR 已停止两次", PRIVATE_DEVICE_STOPPED_TWICE_REASON, {"重启游戏后才能再次使用 NR。"}, CardButton::NONE);
  }
  // Test phase (review M-3): a stop on the adapter's shared device is final at once (no new device can be made while the game runs).
  if (facts.private_stop_unretryable && !facts.private_stopped.empty()) {
    return card(CardStage::DEVICE, (facts.device_lost ? "GPU 设备已移除" : "NR 已停止"), facts.private_stopped,
                {"重启游戏后才能再次使用 NR。"}, CardButton::NONE);
  }
  if (!facts.helper_problem.empty() && facts.enabled) {
    return card(CardStage::DEVICE, "Uplift 的 64 位辅助进程已停止", facts.helper_problem, {"点击“立即重试”以重新启动辅助进程。"},
                CardButton::RETRY_NOW);
  }
  // Plan 17: a bridge's private device stopped (the first time this session): Retry now builds a new one, as the helper's card restarts the helper.
  if (!facts.private_stopped.empty() && facts.enabled) {
    return card(CardStage::DEVICE, (facts.device_lost ? "GPU 设备已移除" : "NR 已停止"), facts.private_stopped,
                {"点击“立即重试”，在新设备上重新启动 NR。"}, CardButton::RETRY_NOW);
  }
  if (facts.device_lost) {
    return card(CardStage::DEVICE, "GPU 设备已移除", facts.output_problem, {"重启游戏后才能再次使用 NR。"},
                CardButton::NONE);
  }
  // Plan 15 fix round (minor 6): NR stopped on this device for the session (the game shut NVIDIA's NGX down while NR's work was still running).
  if (!facts.stopped.empty()) return card(CardStage::DEVICE, "NR 已停止", facts.stopped, {"重启游戏后才能再次使用 NR。"}, CardButton::NONE);
  if (facts.claimed_elsewhere) {
    return card(CardStage::DEVICE, "NR 正在另一台设备上运行", "NR 正在此游戏中的另一台 D3D12 设备上运行。",
                {"每个游戏仅有一台设备运行 NR；此设备将等待。"}, CardButton::NONE);
  }
  if (!facts.blocked.empty()) {
    if (facts.runtime_in_game_folder) {
      return card(CardStage::RUNTIME, "移动 nvngx_dlssnr.dll", facts.blocked,
                  {std::format("请将其移入与 {} 同级的 GITC-Uplift 文件夹，然后重启游戏。", facts.addon_file)}, CardButton::NONE);
    }
    if (facts.blocked_stage == CardStage::RUNTIME) {
      return card(CardStage::RUNTIME, "未找到 NR 运行时", facts.blocked,
                  {std::format("请将 nvngx_dlssnr.dll 复制到与 {} 同级的 GITC-Uplift 文件夹，或在“高级”中设置“运行时路径”。", facts.addon_file)},
                  CardButton::NONE);
    }
    return card(CardStage::CONFLICTS, "检测到其他 NR 工具正在运行", facts.blocked,
                {"请移除其他 NR 插件，或在“高级”中将“检测到其他 NR 处理程序时”设为“观察”。"},
                CardButton::NONE);
  }
  if (!facts.enabled) return card(CardStage::SWITCH, "NR 已关闭", "神经渲染当前处于关闭状态。", {}, CardButton::TURN_ON);
  // Plan 15: after the game's NGX shutdown on Direct3D 12 NR waits for the game's DLSS, whatever the stage (a temporary cause; Setup greys with the same text).
  if (!facts.held.empty()) return card(CardStage::PLACEMENT, "等待游戏启动 DLSS", facts.held, {}, CardButton::NONE);
  const nr::SessionStatus& session = facts.session;
  if (session.state == nr::SessionState::FAILED) {
    std::string next = (session.retry_in ? std::format("将在 {} 秒后自动重试。", (session.retry_in->count() + 999) / 1000)
                        : session.retries_exhausted ? std::string("已停止自动重试：请更改设置或立即重试。")
                                                    : std::string("请立即重试或更改设置。"));
    return card(CardStage::SESSION, "NR 运行失败", session.message, {std::move(next)}, CardButton::RETRY_NOW);
  }
  // 2026-10-09 (owner): the video memory cards also offer the looser Video memory checks there are.
  std::string_view looser_checks;
  switch (facts.vram_check) {
    case nr::VramCheck::CAREFUL: looser_checks = "宽松或关闭"; break;
    case nr::VramCheck::RELAXED: looser_checks = "关闭"; break;
    case nr::VramCheck::OFF:     break;
  }
  if (session.suspended) {
    std::string need_less = (looser_checks.empty() ? std::string("请降低“分辨率”或“层数”。")
                             : std::format("请降低“分辨率”或“层数”，或在“高级”中将“显存检查”设为“{}”。", looser_checks));
    return card(CardStage::SESSION, "已暂停：游戏需要更多显存", session.message,
                {"释放显存后 NR 将自动恢复运行。", std::move(need_less)}, CardButton::NONE);
  }
  if (!facts.output_problem.empty()) {
    return card(CardStage::PLACEMENT, "NR 无法处理此画面", facts.output_problem, {}, CardButton::NONE);
  }
  if (!facts.placement_note.empty()) {
    if (facts.dlss_latched) {
      return card(CardStage::PLACEMENT, "DLSS 后运行 NR 已停用", facts.placement_note,
                  {"NR 在游戏帧内运行后不久，设备即被移除。",
                   "清除锁定后，下次启动时将再次尝试。"},
                  CardButton::CLEAR_LATCH);
    }
    return card(CardStage::PLACEMENT, "正在等待", facts.placement_note, {}, CardButton::NONE);
  }
  // FAILED already returned above: only ACTIVE falls through to the placement and skip checks below.
  switch (session.state) {
    case nr::SessionState::OFF:
    case nr::SessionState::LOADING:
      return card(CardStage::SESSION, "正在启动 NR", "NR 将在下一帧加载。", {}, CardButton::NONE);
    case nr::SessionState::GRACE:
    case nr::SessionState::DRAINING:
      return card(CardStage::SESSION, "NR 正在释放显存", "释放完成后，NR 将重新启动。", {},
                  CardButton::NONE);
    case nr::SessionState::ACTIVE:
    default: break;
  }
  if (!facts.nr_applied) {
    const std::string_view reason = LocalizeStatusReason(
        (facts.skip_from_session && !session.message.empty() ? std::string_view(session.message) : facts.skip_reason));
    if (facts.skip_reason == "budget") {
      std::string need_less =
          (looser_checks.empty() ? std::string("请降低“分辨率”或“层数”，或关闭其他 GPU 程序。")
           : std::format("请降低“分辨率”或“层数”、关闭其他 GPU 程序，或在“高级”中将“显存检查”设为“{}”。", looser_checks));
      return card(CardStage::BUDGET, "NR 可用显存不足", reason, {std::move(need_less)}, CardButton::NONE);
    }
    if (facts.skip_reason == "frame too small" || facts.skip_reason == "resizing") {
      return card(CardStage::FRAME, "等待可处理的画面", reason, {"NR 需要至少 " UPLIFT_NR_FLOOR_TEXT " 的画面尺寸。"}, CardButton::NONE);
    }
    if (facts.skip_reason == "evaluate failed" || facts.skip_reason == "create failed") {
      return card(CardStage::EVALUATE, "NR 处理失败", reason, {}, CardButton::NONE);
    }
    if (facts.skip_reason == "frame generation is on") {
      return card(CardStage::PLACEMENT, "NR 已跳过此帧", reason,
                  {"请选择“DLSS 后”，或在“高级”中启用“帧生成期间处理后处理”。"}, CardButton::NONE);
    }
    if (reason.empty()) return card(CardStage::PLACEMENT, "等待画面", "NR 尚未运行。", {}, CardButton::NONE);
    return card(CardStage::PLACEMENT, "NR 已跳过此帧", reason, {}, CardButton::NONE);
  }
  StatusCard working = {.stage = CardStage::WORKING, .working = true, .title = "NR 正在运行",
                        .reason = std::string(facts.working_line)};
  if (facts.passes_run < session.passes_requested) {
    working.fixes.push_back(session.message.empty()
                                ? std::format("正在运行 {}/{} 层。", facts.passes_run, session.passes_requested)
                                : session.message);
  }
  if (!session.faces_note.empty()) {
    working.fixes.emplace_back(session.faces_note);  // Keep faces fix round 1 (C2): paused or stopped while NR works
  }
  return working;
}

}  // namespace uplift::ui
