#include "sources/auto_exposure.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <string_view>

namespace uplift::sources {
namespace {

std::string_view LatchWord(AutoLatch latch) {
  switch (latch) {
    case AutoLatch::ABOVE: return "偏高";
    case AutoLatch::BELOW: return "偏低";
    case AutoLatch::NONE:  break;
  }
  return {};
}

}  // namespace

float AutoExposure::OutsideSpan(const ExposureSample& sample) {
  if (!(sample.game > 0.f) || !std::isfinite(sample.game) || !std::isfinite(sample.metered_stops) || !std::isfinite(sample.target_stops)) {
    return std::numeric_limits<float>::quiet_NaN();
  }
  const float game = std::log2(sample.game);
  const float low = std::min(sample.metered_stops, sample.target_stops);
  const float high = std::max(sample.metered_stops, sample.target_stops);
  if (game > high) return game - high;
  if (game < low) return game - low;
  return 0.f;
}

void AutoExposure::NoteHeld(float game_stops, float anchor_stops, double seconds) {
  if (!held_stops_ || std::abs(game_stops - *held_stops_) > HELD_EPSILON_STOPS) {
    held_stops_ = game_stops;  // the game's exposure moved: a new value to hold
    held_anchors_.clear();
    held_through_move_ = false;
    held_anchor_span_ = 0.f;
  }
  held_anchors_.emplace_back(seconds, anchor_stops);
  while (seconds - held_anchors_.front().first > SUSTAIN_SECONDS) {
    held_anchors_.pop_front();
  }
  const auto [lowest, highest] = std::minmax_element(held_anchors_.begin(), held_anchors_.end(),
                                                     [](const auto& first, const auto& second) { return first.second < second.second; });
  const float span = highest->second - lowest->second;
  if (span > HELD_ANCHOR_STOPS) {
    held_through_move_ = true;
    held_anchor_span_ = std::max(held_anchor_span_, span);
  }
}

std::optional<float> AutoExposure::HeldThroughMove() const {
  if (!held_through_move_) return std::nullopt;
  return held_anchor_span_;
}

bool AutoExposure::Observe(const ExposureSample& sample, double seconds) {
  if (latched_) return false;
  const float outside = OutsideSpan(sample);
  // An unusable sample neither counts nor breaks a run.
  if (!std::isfinite(outside) || !std::isfinite(sample.anchor_stops)) return false;
  const float game_stops = std::log2(sample.game);
  last_gap_ = game_stops - sample.metered_stops;
  NoteHeld(game_stops, sample.anchor_stops, seconds);  // the debug line's information only: never invalidates a sample
  AutoLatch rule = AutoLatch::NONE;
  if (outside > ABOVE_LIMIT_STOPS) {
    rule = AutoLatch::ABOVE;
  } else if (outside < -BELOW_LIMIT_STOPS) {
    rule = AutoLatch::BELOW;
  }
  if (rule == AutoLatch::NONE) {
    since_.reset();
    run_samples_ = 0u;
    return false;
  }
  if (!since_) {
    since_ = seconds;
  }
  ++run_samples_;
  if (run_samples_ < SUSTAIN_SAMPLES || seconds - *since_ < SUSTAIN_SECONDS) return false;
  latched_ = true;
  reason_ = rule;
  stops_off_ = std::abs(*last_gap_);
  return true;
}

bool AutoExposure::DebugDue(double seconds) {
  if (last_debug_ && seconds - *last_debug_ < DEBUG_SECONDS) return false;
  last_debug_ = seconds;
  return true;
}

float AutoStops(color::AutoExposureMode mode, float blend, bool latched, float game_stops, float metered_stops) {
  if (latched) return metered_stops;
  switch (mode) {
    case color::AutoExposureMode::BLEND:  return game_stops + blend * (metered_stops - game_stops);
    case color::AutoExposureMode::SWITCH: break;
  }
  return game_stops;
}

std::string ExposureLine(const ExposureReport& report) {
  switch (report.use) {
    case ExposureUse::GAME: return "输入曝光：游戏曝光";
    case ExposureUse::METERED:
      if (report.stops_off && report.latch != AutoLatch::NONE) {
        return std::format("输入曝光：测光（游戏曝光偏差 {:.1f} 档，{}）", *report.stops_off, LatchWord(report.latch));
      }
      return "输入曝光：测光";
    case ExposureUse::BLEND:
      if (report.gap) return std::format("输入曝光：游戏曝光/测光混合 {:.0f} %（差值 {:+.1f} 档）", report.blend * 100.f, *report.gap);
      return std::format("输入曝光：游戏曝光/测光混合 {:.0f} %", report.blend * 100.f);
    case ExposureUse::NONE: break;
  }
  return {};
}

std::string AutoLatchMessage(const ExposureSample& sample, const AutoExposure& judge) {
  const std::string rule = std::format("与 Uplift 测光值相差 {:.1f} 档，{}（容差范围为低于 {:.0f} 档至高于 {:.0f} 档）",
                                       std::abs(AutoExposure::OutsideSpan(sample)), LatchWord(judge.Reason()), AutoExposure::BELOW_LIMIT_STOPS,
                                       AutoExposure::ABOVE_LIMIT_STOPS);
  return std::format("输入曝光自动切换：游戏曝光{}已持续超过 {:.0f} 秒，因此 NR 从现在起改用测光输入（ExposureTexture {:.4g}，系数 "
                     "{:.4g}：游戏值 {:.4g} = {:+.2f} 档；测光值 {:.4g} = {:+.2f} 档，目标 {:+.2f} 档；偏差 {:.1f} 档）",
                     rule, AutoExposure::SUSTAIN_SECONDS, sample.texture_value, sample.factor, sample.game, std::log2(sample.game),
                     std::exp2(sample.metered_stops), sample.metered_stops, sample.target_stops, judge.StopsOff());
}

std::string AutoDebugMessage(const ExposureSample& sample, color::AutoExposureMode mode, float blend, std::optional<float> held_move) {
  const float game = std::log2(sample.game);
  const float result = AutoStops(mode, blend, false, game, sample.metered_stops);
  const std::string mode_text = (mode == color::AutoExposureMode::BLEND ? std::format("混合 {:.0f} %", blend * 100.f) : std::string("切换"));
  std::string message = std::format("输入曝光自动模式：游戏曝光 {:+.2f} 档，测光 E {:+.2f} 档（目标 {:+.2f} 档），差值 {:+.2f} 档；{} 得出 {:+.2f} 档",
                                    game, sample.metered_stops, sample.target_stops, game - sample.metered_stops, mode_text, result);
  if (held_move) {
    message += std::format("；场景变化 {:.1f} 档期间保持使用游戏曝光", *held_move);
  }
  return message;
}

}  // namespace uplift::sources
