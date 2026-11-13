// Typed fixed-total latency allocation: implementation.
#include "arbiter/latency.hpp"
#include "arbiter/errors.hpp"

#include <cstdio>
#include <charconv>
#include <cmath>
#include <stdexcept>

namespace arbiter {

std::string LatencyAllocation::to_string() const {
    char buf[96];
    std::snprintf(buf, sizeof buf, "md+%lld,dec+%lld,ord+%lld",
                  static_cast<long long>(md_transit_ns),
                  static_cast<long long>(decision_ns),
                  static_cast<long long>(order_transit_ns));
    return buf;
}

Result<LatencyAllocation> LatencyAllocation::from_string(std::string_view s) {
    LatencyAllocation a;
    // strict parse: exactly "md+X,dec+Y,ord+Z" with non-negative integers
    if (s.empty()) return ErrT<LatencyAllocation>(Errc::InvalidArgument, 0);
    const char* p = s.data();
    const char* end = s.data() + s.size();
    auto parse_tag = [&](const char* tag, size_t tag_len, i64& out) -> bool {
        if (static_cast<size_t>(end - p) < tag_len) return false;
        if (s.compare(static_cast<size_t>(p - s.data()), tag_len, tag, tag_len) != 0) return false;
        p += tag_len;
        if (p == end || *p < '0' || *p > '9') return false;
        const auto parsed = std::from_chars(p, end, out);
        if (parsed.ec != std::errc{}) return false;
        p = parsed.ptr;
        return true;
    };
    if (!parse_tag("md+", 3, a.md_transit_ns)) return ErrT<LatencyAllocation>(Errc::InvalidArgument, 0);
    if (p == end || *p != ',') return ErrT<LatencyAllocation>(Errc::InvalidArgument, 1);
    ++p;
    if (!parse_tag("dec+", 4, a.decision_ns)) return ErrT<LatencyAllocation>(Errc::InvalidArgument, 2);
    if (p == end || *p != ',') return ErrT<LatencyAllocation>(Errc::InvalidArgument, 3);
    ++p;
    if (!parse_tag("ord+", 4, a.order_transit_ns)) return ErrT<LatencyAllocation>(Errc::InvalidArgument, 4);
    if (p != end) return ErrT<LatencyAllocation>(Errc::InvalidArgument, 5);
    if (a.md_transit_ns > std::numeric_limits<i64>::max() - a.decision_ns ||
        a.md_transit_ns + a.decision_ns > std::numeric_limits<i64>::max() - a.order_transit_ns)
        return ErrT<LatencyAllocation>(Errc::InvalidArgument, 6);
    return OkT(a);
}

void LatencyAllocation::hash_into(Hasher& h) const {
    h.tag("ARBITERX/ALLOC/1");
    h.i64v(md_transit_ns);
    h.i64v(decision_ns);
    h.i64v(order_transit_ns);
    h.i64v(total_ns());
}

WorldConfig LatencyAllocation::apply_to(WorldConfig cfg) const {
    cfg.md_channel.base_delay_ns = md_transit_ns;
    cfg.md_channel.jitter_ns = 0;           // placement is exact under the
    cfg.order_channel.base_delay_ns = order_transit_ns;
    cfg.order_channel.jitter_ns = 0;        // flagship: no stochastic confound
    cfg.exec_channel.jitter_ns = 0;
    cfg.algo_latency_ns = decision_ns;
    return cfg;
}

LatencyAllocation allocation_on_slice(i64 total_ns, double md_share) {
    if (total_ns < 0 || !std::isfinite(md_share))
        throw std::invalid_argument("invalid latency allocation slice");
    if (md_share < 0.0) md_share = 0.0;
    if (md_share > 1.0) md_share = 1.0;
    const long double rounded = std::floor(static_cast<long double>(total_ns) * md_share + 0.5L);
    const i64 md = rounded >= static_cast<long double>(total_ns)
                       ? total_ns : static_cast<i64>(rounded);
    const i64 act_total = total_ns - md;
    // split the action budget evenly; decision carries the rounding residual
    const i64 ord = act_total / 2;
    const i64 dec = act_total - ord;
    return LatencyAllocation{md, dec, ord};
}

}  // namespace arbiter
