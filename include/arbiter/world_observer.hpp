#pragma once
// Synchronous, optional observation of World boundaries. Callbacks receive
// borrowed data for the duration of the call and must contain all failures.
#include "arbiter/book_common.hpp"
#include "arbiter/event.hpp"

#include <span>
#include <string_view>

namespace arbiter {

struct WorldEvent;
class OptBook;
class RiskEngine;
enum class ScheduleBoundary { Scheduled, Begin, Complete };
enum class ReceiveOutcome { Accepted, DecodeRejected, Deduplicated, Held, Retired };
enum class FeedBoundary { GapScheduled, Applied, SnapshotFallback };

struct ObserverContext {
    LogicalNs now;
    u64 processed_ordinal;  // 0 outside a processing boundary
};

struct ObservedFrame {
    u8 channel;
    u64 sequence;
    u64 order_id;
    LogicalNs departure;
    std::string_view bytes;
    bool dropped;
    bool duplicate;
    u64 arrival_event;  // scheduler insertion ID; unused when dropped
    u8 action;         // order: 0 send / 1 cancel; exec: Notify::kind
    const Order* intent; // send actions only; borrowed
    u64 maker_id = 0;   // fill report only
    bool retransmit = false;
};

class WorldObserver {
public:
    virtual ~WorldObserver() = default;
    virtual void scheduler(ObserverContext, ScheduleBoundary, const WorldEvent&) noexcept {}
    virtual void frame(ObserverContext, const ObservedFrame&) noexcept {}
    virtual void receiver(ObserverContext, u8, u64, u64, ReceiveOutcome) noexcept {}
    virtual void risk(ObserverContext, u64, bool, u64) noexcept {}
    virtual void book(ObserverContext, u64, Side, std::span<const Fill>, const OptBook&,
                      bool) noexcept {}
    virtual void accounting(ObserverContext, i64, i64, i64, i64, const RiskEngine&) noexcept {}
    virtual void feed(ObserverContext, FeedBoundary, u64, u64) noexcept {}
    virtual void journal(ObserverContext, u64, const Event&) noexcept {}
};

} // namespace arbiter
