// MIT. Coalesce network progress while keeping local checkpoint updates intact.
#pragma once
#include <chrono>
#include <cstdint>
#include <optional>
namespace openpuzzle::vanityPreview {
class CoordinatorPacer {
public:
 using Clock=std::chrono::steady_clock;
 explicit CoordinatorPacer(Clock::duration interval=std::chrono::seconds(5)):interval_(interval){}
 bool due(uint64_t checked,Clock::time_point now,bool force=false) const {
  return force||(!positive_&&checked>0)||(!ack_||(checked!=*ack_&&now>=next_));
 }
 void acknowledged(uint64_t checked,Clock::time_point now){ack_=checked;positive_=positive_||checked>0;next_=now+interval_;}
 bool confirmed(uint64_t checked) const {return ack_&&*ack_==checked;}
private:
 Clock::duration interval_;Clock::time_point next_{};std::optional<uint64_t> ack_;bool positive_=false;
};
}
