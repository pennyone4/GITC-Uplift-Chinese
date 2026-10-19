#include "addon/frame_trigger.hpp"

#include <utility>

#include "nr/log.hpp"

namespace uplift::addon {

std::string_view TriggerPointName(TriggerPoint point) {
  switch (point) {
    case TriggerPoint::NONE:          return "无";
    case TriggerPoint::PRESENT:       return "后处理阶段（ReShade 效果前）";
    case TriggerPoint::MARKER:        return "Uplift 技术";
    case TriggerPoint::AFTER_EFFECTS: return "ReShade 效果后（Uplift 技术未运行）";
  }
  return "无";
}

TriggerPoint FrameTrigger::OnPresent(bool marker_expected, bool effects_on) {
  const bool was_waiting = waiting_;
  waiting_ = false;
  const bool defer = (std::exchange(begin_seen_, false) && effects_on);
  waiting_begin_ = false;
  if (!marker_expected) {
    // The technique is off this very frame, so a still-pending wait from last frame (an
    // in-present effect reload can leave one, with neither effect event reaching this class) is
    // moot, not a real miss: nothing is "pending until it is toggled" when it already just was.
    marker_missed_ = false;
    return BeforeEffects(defer);
  }
  if (was_waiting && !marker_missed_) {
    marker_missed_ = true;
    nr::Log(nr::LogLevel::WARN,
            "Uplift 技术未在 ReShade 的命令列表中执行；重新启用该技术前，NR 会在效果处理前运行");
  }
  if (marker_missed_) return BeforeEffects(defer);
  waiting_ = true;
  return TriggerPoint::NONE;
}

TriggerPoint FrameTrigger::OnBeginEffects(bool usable) {
  begin_seen_ = usable;
  if (!waiting_begin_) return TriggerPoint::NONE;
  waiting_begin_ = false;
  return (usable ? TriggerPoint::PRESENT : TriggerPoint::NONE);
}

TriggerPoint FrameTrigger::BeforeEffects(bool deferred) {
  waiting_begin_ = deferred;
  return (deferred ? TriggerPoint::NONE : TriggerPoint::PRESENT);
}

TriggerPoint FrameTrigger::OnTechnique(bool is_marker) {
  if (!waiting_ || !is_marker) return TriggerPoint::NONE;
  waiting_ = false;
  return TriggerPoint::MARKER;
}

TriggerPoint FrameTrigger::OnFinishEffects() {
  if (!waiting_) return TriggerPoint::NONE;
  waiting_ = false;
  return TriggerPoint::AFTER_EFFECTS;
}

}  // namespace uplift::addon
