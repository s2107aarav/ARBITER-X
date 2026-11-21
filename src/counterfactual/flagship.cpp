// Flagship experiment implementation. See header for the design contract.
#include "arbiter/flagship.hpp"
#include "arbiter/errors.hpp"
#include "arbiter/hash.hpp"
#include "arbiter/build_info.hpp"
#include "arbiter/stats.hpp"
#include "../core/json.hpp"

#include <bit>
#include <cmath>
#include <cerrno>
#include <cctype>
#include <charconv>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace arbiter {

namespace {

WorldConfig base_world_config(const FlagshipConfig& fc) {
    WorldConfig cfg;
    cfg.journal_events = false;
    cfg.hash_every_events = 0;
    cfg.horizon_ns = fc.horizon_ns;
    cfg.warmup_ns = fc.warmup_ns;
    cfg.bots_observe_shadow = fc.bots_observe_shadow;
    cfg.sim.taker_self_excitation =
        fc.flow_model == FlowModel::Clustered ? fc.taker_self_excitation : 0.0;
    // deterministic clean channels for every cell: the flagship isolates
    // PLACEMENT; stochastic faults are a separate axis (network_grid)
    cfg.md_channel.jitter_ns = 0;
    cfg.order_channel.jitter_ns = 0;
    cfg.exec_channel.jitter_ns = 0;
    return cfg;
}

CellSeedOutcome run_one(const FlagshipConfig& fc, const LatencyAllocation& alloc, u64 seed) {
    WorldConfig cfg = alloc.apply_to(base_world_config(fc));
    World w(cfg, seed);
    const RunStats& s = w.run();
    if (s.events_processed == cfg.max_events)
        throw std::runtime_error("flagship hit the world event cap before verifying horizon completion");
    CellSeedOutcome o;
    o.mm_marked_pnl = saturating_add_i64(s.mm_cash, saturating_mul_i64(s.mm_inventory, s.final_mid));
    o.mom_marked_pnl = saturating_add_i64(s.mom_cash, saturating_mul_i64(s.mom_inventory, s.final_mid));
    o.mm_max_drawdown = s.mm_max_drawdown;
    o.mm_fills = s.strategy_fills;
    o.mm_filled_qty = s.mm_real_filled_qty;
    o.mom_fills = s.momentum_fills;
    o.mom_filled_qty = s.mom_filled_qty;  // delivered lots at the study horizon
    o.mm_orders_sent = w.orders_sent_by(cfg.mm.account);
    o.mm_cancels_sent = s.strategy_cancels_sent;
    o.mm_orders_rejected = w.risk_rejected_by(cfg.mm.account);
    o.final_state_hash = w.state_hash();
    o.events = s.events_processed;
    o.mm_markout_samples = s.adverse_100us.count();
    o.mom_markout_samples = s.mom_adverse_100us.count();
    o.mm_markout_100us = s.adverse_100us.mean();
    o.mom_markout_100us = s.mom_adverse_100us.mean();
    o.md_transit_samples = s.md_transit_samples;
    o.mean_md_transit_ns = s.md_transit_mean_ns;
    o.action_trace_samples = s.stage_traced_fills;
    if (o.action_trace_samples) {
        o.mean_decision_ns = static_cast<double>(s.stage_sum_decision_to_depart) / o.action_trace_samples;
        o.mean_order_transit_ns = static_cast<double>(s.stage_sum_depart_to_gateway) / o.action_trace_samples;
    }
    return o;
}

std::string cell_name(double md_share) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "md%.3f", md_share);
    if (std::strtod(buf + 2, nullptr) != md_share)
        std::snprintf(buf, sizeof buf, "md%.17g", md_share);
    return buf;
}

std::string number_or_null(double value, u64 samples, const char* format = "%.17g") {
    if (samples == 0) return "null";
    char number[64];
    std::snprintf(number, sizeof number, format, value);
    return number;
}

u64 definition_hash(const FlagshipConfig& fc) {
    Hasher h;
    h.tag("ARBITERX/FLAGSHIP/2");
    h.str(fc.name);
    h.i64v(fc.total_latency_ns);
    h.u64v(fc.md_shares.size());
    for (double x : fc.md_shares) h.u64v(std::bit_cast<u64>(x));
    h.u8v(static_cast<u8>(fc.flow_model));
    h.u64v(std::bit_cast<u64>(fc.taker_self_excitation));
    h.boolean(fc.bots_observe_shadow);
    h.u64v(fc.seeds.size());
    for (u64 s : fc.seeds) h.u64v(s);
    h.i64v(fc.horizon_ns);
    h.i64v(fc.warmup_ns);
    h.u64v(std::bit_cast<u64>(fc.economic_threshold_ticks_qty_per_lot));
    h.u64v(fc.permutation_reps);
    h.u64v(static_cast<u64>(fc.hac_lag));
    return h.value();
}

}  // namespace

const char* PairComparison::verdict() const noexcept {
    if (!per_lot_available)
        return statistically_significant ? "H0 rejected after Holm correction. Per-lot effect unavailable."
                                         : "The primary contrast is inconclusive after Holm correction. Per-lot effect unavailable.";
    if (statistically_significant)
        return economically_material ? "H0 rejected after Holm correction. The effect meets the materiality threshold."
                                     : "H0 rejected after Holm correction. The effect falls below the materiality threshold.";
    return economically_material ? "The primary contrast is inconclusive after Holm correction. The point estimate meets the materiality threshold."
                                 : "The primary contrast is inconclusive after Holm correction. The point estimate falls below the materiality threshold.";
}

FlagshipResult run_flagship(const FlagshipConfig& fc) {
    if (fc.seeds.empty() || fc.md_shares.size() < 2 || fc.total_latency_ns < 0 ||
        fc.permutation_reps == 0 ||
        (fc.flow_model != FlowModel::Poisson && fc.flow_model != FlowModel::Clustered) ||
        !std::isfinite(fc.taker_self_excitation) || fc.taker_self_excitation < 0.0 ||
        fc.taker_self_excitation >= 1.0 ||
        !std::isfinite(fc.economic_threshold_ticks_qty_per_lot) ||
        fc.economic_threshold_ticks_qty_per_lot < 0.0)
        throw std::invalid_argument("invalid flagship definition");
    if (std::set<u64>(fc.seeds.begin(), fc.seeds.end()).size() != fc.seeds.size())
        throw std::invalid_argument("duplicate flagship seeds");
    LogicalNs previous_md = -1;
    for (size_t i = 0; i < fc.md_shares.size(); ++i) {
        const double share = fc.md_shares[i];
        if (!std::isfinite(share) || share < 0.0 || share > 1.0 ||
            (i > 0 && share <= fc.md_shares[i - 1]))
            throw std::invalid_argument("flagship md shares must increase within [0,1]");
        const auto alloc = allocation_on_slice(fc.total_latency_ns, share);
        if (alloc.md_transit_ns == previous_md)
            throw std::invalid_argument("flagship shares round to the same latency allocation");
        previous_md = alloc.md_transit_ns;
        const WorldConfig cfg = alloc.apply_to(base_world_config(fc));
        if (const std::string invalid = cfg.validate(); !invalid.empty())
            throw std::invalid_argument("invalid flagship world: " + invalid);
    }
    FlagshipResult res;
    res.cfg = fc;
    res.flow_model_name = fc.flow_model == FlowModel::Poisson ? "poisson" : "clustered";
    res.compiler = build::compiler_id();
    res.platform = build::platform_id();
    res.created_utc = build::utc_now();
    res.core_version = build::version();
    res.core_revision = build::revision();
    res.core_source_state = build::source_state();

    res.definition_hash = definition_hash(fc);

    // run the grid: every cell over every seed (paired by construction:
    // same seed -> same master RNG streams)
    res.cells.reserve(fc.md_shares.size());
    for (double share : fc.md_shares) {
        CellResult cell;
        cell.name = cell_name(share);
        cell.alloc = allocation_on_slice(fc.total_latency_ns, share);
        cell.per_seed.reserve(fc.seeds.size());
        for (u64 seed : fc.seeds) cell.per_seed.push_back(run_one(fc, cell.alloc, seed));
        double n = static_cast<double>(cell.per_seed.size());
        double mm_pnl = 0, mom_pnl = 0, mm_f = 0, mm_q = 0, mom_q = 0, mm_m = 0, mom_m = 0;
        for (const auto& o : cell.per_seed) {
            mm_pnl += o.mm_marked_pnl;
            mom_pnl += o.mom_marked_pnl;
            mm_f += o.mm_fills;
            mm_q += o.mm_filled_qty;
            mom_q += o.mom_filled_qty;
            if (o.mm_markout_samples) {
                mm_m += o.mm_markout_100us;
                ++cell.mm_markout_seeds;
            }
            if (o.mom_markout_samples) {
                mom_m += o.mom_markout_100us;
                ++cell.mom_markout_seeds;
            }
        }
        cell.mean_mm_pnl = mm_pnl / n;
        cell.mean_mom_pnl = mom_pnl / n;
        cell.mean_mm_fills = mm_f / n;
        cell.mean_mm_filled_qty = mm_q / n;
        cell.mean_mom_filled_qty = mom_q / n;
        if (cell.mm_markout_seeds) cell.mean_mm_markout = mm_m / cell.mm_markout_seeds;
        if (cell.mom_markout_seeds) cell.mean_mom_markout = mom_m / cell.mom_markout_seeds;
        res.cells.push_back(std::move(cell));
    }

    // contrast family: adjacent cells along the slice + the two extremes.
    // The extreme pair (md0.000 vs md1.000) is the PRIMARY contrast.
    auto add_pair = [&](size_t ia, size_t ib) {
        PairComparison p;
        p.cell_a = res.cells[ia].name;
        p.cell_b = res.cells[ib].name;
        p.alloc_a = res.cells[ia].alloc;
        p.alloc_b = res.cells[ib].alloc;
        const size_t nseeds = fc.seeds.size();
        p.mm_pnl_diff.reserve(nseeds);
        p.mom_pnl_diff.reserve(nseeds);
        p.mm_fills_diff.reserve(nseeds);
        p.mm_markout_diff.reserve(nseeds);
        for (size_t k = 0; k < nseeds; ++k) {
            const CellSeedOutcome& a = res.cells[ia].per_seed[k];
            const CellSeedOutcome& b = res.cells[ib].per_seed[k];
            p.mm_pnl_diff.push_back(static_cast<double>(static_cast<long double>(b.mm_marked_pnl) - a.mm_marked_pnl));
            p.mom_pnl_diff.push_back(static_cast<double>(static_cast<long double>(b.mom_marked_pnl) - a.mom_marked_pnl));
            p.mm_fills_diff.push_back(signed_fill_count_difference(b.mm_fills, a.mm_fills));
            if (a.mm_markout_samples && b.mm_markout_samples)
                p.mm_markout_diff.push_back(b.mm_markout_100us - a.mm_markout_100us);
        }
        const stats::MeanCI ci = stats::mean_ci(p.mm_pnl_diff, fc.hac_lag);
        p.mean_diff = ci.mean;
        p.se_iid = ci.se_iid;
        p.ci95_lo = ci.ci95_lo;
        p.ci95_hi = ci.ci95_hi;
        p.p_permutation = stats::paired_sign_flip_pvalue(p.mm_pnl_diff, res.definition_hash,
                                             fc.permutation_reps);
        // economic scale: per-lot PnL difference (ticks*qty per filled lot,
        // using the two cells' average traded volume as the denominator)
        const double dq = res.cells[ib].mean_mm_filled_qty + res.cells[ia].mean_mm_filled_qty;
        p.per_lot_available = dq > 0;
        p.pnl_diff_per_lot = p.per_lot_available ? (p.mean_diff / (dq / 2.0)) : 0.0;
        p.economically_material = p.per_lot_available &&
            std::fabs(p.pnl_diff_per_lot) >= fc.economic_threshold_ticks_qty_per_lot;
        res.pairs.push_back(std::move(p));
    };
    for (size_t i = 0; i + 1 < res.cells.size(); ++i) add_pair(i, i + 1);
    if (res.cells.size() > 2) add_pair(0, res.cells.size() - 1);  // primary

    std::vector<double> raw_p;
    raw_p.reserve(res.pairs.size());
    for (const auto& pair : res.pairs) raw_p.push_back(pair.p_permutation);
    const std::vector<double> adjusted_p = stats::holm(raw_p);
    for (size_t i = 0; i < res.pairs.size(); ++i) {
        res.pairs[i].p_holm = adjusted_p[i];
        res.pairs[i].statistically_significant = adjusted_p[i] <= 0.05;
    }
    // findings: reading of the grid. NO hardcoded
    // narrative, each statement is computed from the data.
    {
        char b[256];
        // Primary contrast compares the endpoints of the supplied grid.
        const PairComparison& prim = res.pairs.back();
        std::snprintf(b, sizeof b,
                      "primary contrast (%s -> %s): mean paired MM PnL diff %+.1f ticks*qty "
                      "(per-lot %s), Holm p=%.4f, %s",
                      prim.cell_a.c_str(), prim.cell_b.c_str(), prim.mean_diff,
                      number_or_null(prim.pnl_diff_per_lot, prim.per_lot_available, "%+.2f").c_str(), prim.p_holm,
                      prim.statistically_significant ? "significant" : "not significant");
        res.findings.push_back(b);
        std::snprintf(b, sizeof b,
                      "economic materiality (primary): %s (threshold %.2f ticks*qty/lot)",
                      prim.per_lot_available ? (prim.economically_material ? "MATERIAL" : "not material")
                                             : "unavailable",
                      fc.economic_threshold_ticks_qty_per_lot);
        res.findings.push_back(b);
        // fill-count movement across the slice (execution mechanics channel)
        const double f_lo = res.cells.front().mean_mm_filled_qty;
        const double f_hi = res.cells.back().mean_mm_filled_qty;
        std::snprintf(b, sizeof b,
                      "MM filled lots across the slice: %.1f (md share %.3g) vs %.1f "
                      "(md share %.3g): %s", f_hi, fc.md_shares.back(), f_lo, fc.md_shares.front(),
                      std::fabs(f_hi - f_lo) > 1e-9 ? "sample means differ"
                                                    : "sample means match");
        res.findings.push_back(b);
        // taker-side economics movement
        const double m_lo = res.cells.front().mean_mom_pnl;
        const double m_hi = res.cells.back().mean_mom_pnl;
        std::snprintf(b, sizeof b,
                      "taker (momentum) mean PnL across the slice: %.1f -> %.1f",
                      m_lo, m_hi);
        res.findings.push_back(b);
        // taker-side inference at the primary contrast (endpoint reported for
        // archetype heterogeneity; NOT part of the Holm family, the MM PnL
        // contrast is the predefined primary, the taker line is descriptive
        // with its own paired mean/se/p)
        {
            const PairComparison& prim2 = res.pairs.back();
            const stats::MeanCI mci = stats::mean_ci(prim2.mom_pnl_diff, fc.hac_lag);
            const double p_taker = stats::paired_sign_flip_pvalue(prim2.mom_pnl_diff,
                                                       res.definition_hash,
                                                       fc.permutation_reps);
            std::snprintf(b, sizeof b,
                          "exploratory taker endpoint: mean paired PnL diff %+.1f (raw sign-flip p=%.4f)",
                          mci.mean, p_taker);
            res.findings.push_back(b);
        }
        // verdict against H0 for the primary contrast
        res.findings.emplace_back(prim.verdict());
    }
    return res;
}

// ---------------- artifact + report + reproduce ----------------

namespace {

// Scanners for the canonical artifact format emitted below.
bool scan_key(const std::string& text, const std::string& key, std::string& out) {
    const std::string pat = "\"" + key + "\": \"";
    size_t pos = text.find(pat);
    if (pos == std::string::npos) return false;
    pos += pat.size();
    out.clear();
    while (pos < text.size()) {
        char ch = text[pos++];
        if (ch == '"') return true;
        if (static_cast<unsigned char>(ch) < 0x20) return false;
        if (ch == '\\') {
            if (pos == text.size()) return false;
            ch = text[pos++];
            switch (ch) {
                case '"': case '\\': case '/': break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'u': {
                    if (pos + 4 > text.size() || text.substr(pos, 2) != "00") return false;
                    const auto digit = [](char c) -> int {
                        if (c >= '0' && c <= '9') return c - '0';
                        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                        return -1;
                    };
                    const int hi = digit(text[pos + 2]), lo = digit(text[pos + 3]);
                    if (hi < 0 || lo < 0) return false;
                    ch = static_cast<char>(hi * 16 + lo);
                    pos += 4;
                    break;
                }
                default: return false;
            }
        }
        out += ch;
    }
    return false;
}

bool scan_num(const std::string& text, const std::string& key, double& out) {
    const std::string pat = "\"" + key + "\": ";
    size_t pos = text.find(pat);
    if (pos == std::string::npos) return false;
    pos += pat.size();
    const char* start = text.c_str() + pos;
    char* end = nullptr;
    out = std::strtod(start, &end);
    if (end == start || !std::isfinite(out)) return false;
    while(std::isspace(static_cast<unsigned char>(*end))) ++end;
    return *end==',' || *end==']' || *end=='}';
}

template <class Integer>
bool scan_int(const std::string& text, const std::string& key, Integer& out) {
    const std::string pat = "\"" + key + "\": ";
    size_t pos = text.find(pat);
    if (pos == std::string::npos) return false;
    pos += pat.size();
    const char* start = text.data() + pos;
    const auto parsed = std::from_chars(start, text.data() + text.size(), out);
    if (parsed.ec != std::errc{}) return false;
    const char* end = parsed.ptr;
    while(std::isspace(static_cast<unsigned char>(*end))) ++end;
    return *end==',' || *end==']' || *end=='}';
}

bool read_file(const std::string& path, std::string& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
        out.append(buf, n);
        if (n < sizeof buf || std::ferror(f) || std::feof(f)) break;
    }
    const bool read_error = std::ferror(f) != 0;
    std::fclose(f);
    return !read_error && !out.empty();
}

bool write_file(const std::string& path, const std::string& content) {
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) return false;
    const bool complete = std::fwrite(content.data(), 1, content.size(), file) == content.size();
    return std::fclose(file) == 0 && complete;
}

}  // namespace

std::string flagship_to_json(const FlagshipResult& r) {
    char buf[1024];
    std::string out = "{\n  \"schema_version\": 2,\n";
    out += "  \"experiment\": " + json_quote(r.cfg.name) + ",\n";
    std::snprintf(buf, sizeof buf, "  \"definition_hash\": %llu,\n  \"total_latency_ns\": %lld,\n",
                  (unsigned long long)r.definition_hash, (long long)r.cfg.total_latency_ns);
    out += buf;
    out += "  \"flow_model\": " + json_quote(r.flow_model_name) + ",\n";
    std::snprintf(buf, sizeof buf,
                  "  \"taker_self_excitation\": %.17g,\n  \"bots_observe_shadow\": %s,\n",
                  r.cfg.taker_self_excitation,
                  r.cfg.bots_observe_shadow ? "true" : "false");
    out += buf;
    std::snprintf(buf, sizeof buf,
                  "  \"horizon_ns\": %lld,\n  \"warmup_ns\": %lld,\n"
                  "  \"economic_threshold\": %.17g,\n",
                  (long long)r.cfg.horizon_ns, (long long)r.cfg.warmup_ns,
                  r.cfg.economic_threshold_ticks_qty_per_lot);
    out += buf;
    std::snprintf(buf, sizeof buf, "  \"permutation_reps\": %llu,\n  \"hac_lag\": %llu,\n",
                  (unsigned long long)r.cfg.permutation_reps,
                  (unsigned long long)r.cfg.hac_lag);
    out += buf;
    out += "  \"seeds\": [";
    for (size_t i = 0; i < r.cfg.seeds.size(); ++i) {
        std::snprintf(buf, sizeof buf, "%s%llu", i ? ", " : "", (unsigned long long)r.cfg.seeds[i]);
        out += buf;
    }
    out += "],\n";
    out += "  \"compiler\": " + json_quote(r.compiler) + ",\n  \"platform\": " + json_quote(r.platform) + ",\n";
    out += "  \"created_utc\": " + json_quote(r.created_utc) + ",\n";
    out += "  \"core_version\": " + json_quote(r.core_version) + ",\n";
    out += "  \"core_revision\": " + json_quote(r.core_revision) + ",\n";
    out += "  \"core_source_state\": " + json_quote(r.core_source_state) + ",\n";
    out += "  \"cells\": [\n";
    for (size_t c = 0; c < r.cells.size(); ++c) {
        const CellResult& cell = r.cells[c];
        std::snprintf(buf, sizeof buf,
                      "    {\"name\": \"%s\", \"alloc\": \"%s\", \"md_share\": %.17g,\n"
                      "     \"mean_mm_pnl\": %.17g, \"mean_mom_pnl\": %.17g,\n"
                      "     \"mean_mm_filled_qty\": %.17g, \"mean_mom_filled_qty\": %.17g,\n"
                      "     \"mean_mm_markout\": %s, \"mean_mom_markout\": %s,\n"
                      "     \"mm_markout_seeds\": %llu, \"mom_markout_seeds\": %llu,\n"
                      "     \"seeds\": [\n",
                      cell.name.c_str(), cell.alloc.to_string().c_str(),
                      r.cfg.md_shares[c], cell.mean_mm_pnl, cell.mean_mom_pnl,
                      cell.mean_mm_filled_qty, cell.mean_mom_filled_qty,
                      number_or_null(cell.mean_mm_markout, cell.mm_markout_seeds).c_str(),
                      number_or_null(cell.mean_mom_markout, cell.mom_markout_seeds).c_str(),
                      (unsigned long long)cell.mm_markout_seeds, (unsigned long long)cell.mom_markout_seeds);
        out += buf;
        for (size_t k = 0; k < cell.per_seed.size(); ++k) {
            const CellSeedOutcome& o = cell.per_seed[k];
            std::snprintf(buf, sizeof buf,
                          "       {\"seed\": %llu, \"mm_pnl\": %lld, \"mom_pnl\": %lld,\n"
                          "        \"mm_fills\": %llu, \"mm_filled_qty\": %llu, \"mom_fills\": %llu, \"mom_filled_qty\": %llu,\n"
                          "        \"mm_markout\": %s, \"mom_markout\": %s,\n"
                          "        \"mm_markout_samples\": %llu, \"mom_markout_samples\": %llu,\n"
                          "        \"mm_drawdown\": %lld, \"final_hash\": %llu,\n"
                          "        \"md_transit_samples\": %llu, \"action_trace_samples\": %llu,\n"
                          "        \"measured_md_transit_ns\": %s, \"measured_decision_ns\": %s, "
                          "\"measured_order_ns\": %s}%s\n",
                          (unsigned long long)r.cfg.seeds[k], (long long)o.mm_marked_pnl,
                          (long long)o.mom_marked_pnl, (unsigned long long)o.mm_fills,
                          (unsigned long long)o.mm_filled_qty, (unsigned long long)o.mom_fills,
                          (unsigned long long)o.mom_filled_qty,
                          number_or_null(o.mm_markout_100us, o.mm_markout_samples).c_str(),
                          number_or_null(o.mom_markout_100us, o.mom_markout_samples).c_str(),
                          (unsigned long long)o.mm_markout_samples, (unsigned long long)o.mom_markout_samples,
                          (long long)o.mm_max_drawdown, (unsigned long long)o.final_state_hash,
                          (unsigned long long)o.md_transit_samples, (unsigned long long)o.action_trace_samples,
                          number_or_null(o.mean_md_transit_ns, o.md_transit_samples).c_str(),
                          number_or_null(o.mean_decision_ns, o.action_trace_samples).c_str(),
                          number_or_null(o.mean_order_transit_ns, o.action_trace_samples).c_str(),
                          k + 1 < cell.per_seed.size() ? "," : "");
            out += buf;
        }
        std::snprintf(buf, sizeof buf, "     ]}%s\n", c + 1 < r.cells.size() ? "," : "");
        out += buf;
    }
    out += "  ],\n  \"pairs\": [\n";
    for (size_t i = 0; i < r.pairs.size(); ++i) {
        const PairComparison& p = r.pairs[i];
        char interval[64] = "null";
        if (r.cfg.seeds.size() > 1)
            std::snprintf(interval, sizeof interval, "[%.17g, %.17g]", p.ci95_lo, p.ci95_hi);
        std::snprintf(buf, sizeof buf,
                      "    {\"a\": \"%s\", \"b\": \"%s\",\n"
                      "     \"mean_mm_pnl_diff\": %.17g, \"ci95\": %s,\n"
                      "     \"p_perm\": %.17g, \"p_holm\": %.17g,\n"
                      "     \"pnl_diff_per_lot\": %s,\n"
                      "     \"significant\": %s, \"material\": %s}%s\n",
                      p.cell_a.c_str(), p.cell_b.c_str(), p.mean_diff, interval,
                      p.p_permutation, p.p_holm,
                      number_or_null(p.pnl_diff_per_lot, p.per_lot_available).c_str(),
                      p.statistically_significant ? "true" : "false",
                      p.per_lot_available ? (p.economically_material ? "true" : "false") : "null",
                      i + 1 < r.pairs.size() ? "," : "");
        out += buf;
    }
    out += "  ],\n  \"findings\": [\n";
    for (size_t i = 0; i < r.findings.size(); ++i) {
        out += "    " + json_quote(r.findings[i]);
        if (i + 1 < r.findings.size()) out += ",";
        out += "\n";
    }
    out += "  ]\n}\n";
    return out;
}

std::string flagship_report_md(const FlagshipResult& r) {
    char buf[512];
    std::string out;
    out += "# Flagship: latency-stage non-fungibility (fixed total budget)\n\n";
    out += "This paired simulation study estimates effects within the model.\n\n";
    out += "Source revision `" + r.core_revision + "` (" + r.core_source_state +
           "), built with " + r.compiler + " on " + r.platform + ".\n\n";
    out += "## Research question\n\nDoes shifting a fixed " +
           std::to_string(r.cfg.total_latency_ns) + " ns budget across observation, decision and "
           "order transit change mean market-maker marked PnL?\n\n";
    out += "## Intervention\n\nThe allocation grid varies the observation share and divides the "
           "remainder between decision and order transit. Every configured allocation sums to the fixed "
           "budget. JSON reports measured publication-to-feed-arrival means for accepted MD packets "
           "excluding retransmissions. Decision-to-departure and departure-to-gateway means use "
           "delivered strategy fill legs with stage traces. Sample counts identify the two populations, "
           "and absent measurements are null. These separate means do not trace a single path.\n\n";
    out += "## Market process\n\nflow_model=" + r.flow_model_name;
    if (r.flow_model_name == "clustered") {
        std::snprintf(buf, sizeof buf, " (post-trade intensity boost and trade-sign persistence, k=%.2f)",
                      r.cfg.taker_self_excitation);
        out += buf;
    } else {
        out += " (memoryless taker process within each regime, k=0)";
    }
    out += "\n\nThe regime comparison jointly changes temporal dependence, realized intensity and "
           "trade-sign persistence. The Poisson control alone cannot identify the mechanism.\n\n";
    out += "## Replication and inference\n\nThe " + std::to_string(r.cfg.seeds.size()) +
           " master seeds are the independent replication units. All cells use the same seeds and "
           "named streams for paired comparisons. The two-sided sign-flip test assumes the paired "
           "differences are invariant to independent sign changes under the null. Holm correction "
           "covers the " + std::to_string(r.pairs.size()) +
           " market-maker contrasts in this grid at family-wise alpha 0.05. The endpoints "
           "form the primary contrast. Intervals are descriptive, unadjusted normal approximations "
           "across seeds. The taker endpoint is exploratory.\n\n";
    if (r.cfg.seeds.size() <= stats::kExactSignFlipMaxPairs) {
        const auto assignments = u64{1} << r.cfg.seeds.size();
        const double floor = std::min(1.0, 2.0 * r.pairs.size() / assignments);
        std::snprintf(buf, sizeof buf, "The exact test uses %llu sign assignments. The smallest "
                      "possible Holm-adjusted p-value is %.10g, before any additional ties.%s\n\n",
                      (unsigned long long)assignments, floor,
                      floor > 0.05 ? " This pilot cannot reject at family-wise alpha 0.05." : "");
        out += buf;
    } else {
        out += "The sign-flip test uses " + std::to_string(r.cfg.permutation_reps) +
               " seeded Monte Carlo draws and counts the observed assignment once.\n\n";
    }
    out += "## Interference mode\n\n";
    out += r.cfg.bots_observe_shadow
               ? "frozen observation (bots see a shadow touch excluding strategy orders; "
                 "matching and trade feedback remain)\n\n"
               : "reactive (bots observe the live touch and react to strategy quotes; the "
                 "measured effect includes market feedback — interference sensitivity is "
                 "probed by re-running frozen)\n\n";

    // grid table
    out += "PnL is marked from strategy-local cash and inventory at the horizon. Filled lots "
           "count delivered fill legs. Markouts average seed means with observed probes.\n\n";
    out += "| cell | configured allocation (ns) | MM PnL (ticks*qty) | MM filled lots | MM markout | "
           "Taker PnL | Taker filled lots |\n";
    out += "|---|---|---|---|---|---|---|\n";
    for (size_t c = 0; c < r.cells.size(); ++c) {
        const CellResult& cell = r.cells[c];
        std::snprintf(buf, sizeof buf, "| %s | %s | %.1f | %.1f | %s | %.1f | %.1f |\n",
                      cell.name.c_str(), cell.alloc.to_string().c_str(), cell.mean_mm_pnl,
                      cell.mean_mm_filled_qty,
                      number_or_null(cell.mean_mm_markout, cell.mm_markout_seeds, "%+.2f").c_str(),
                      cell.mean_mom_pnl,
                      cell.mean_mom_filled_qty);
        out += buf;
    }
    out += "\n## Paired contrasts (MM PnL, b − a, per seed)\n\n";
    out += "| contrast | mean diff | 95% CI (iid) | p (perm) | p (Holm) | per-lot | "
           "significant | material |\n|---|---|---|---|---|---|---|---|\n";
    for (const PairComparison& p : r.pairs) {
        char interval[64] = "n/a";
        if (r.cfg.seeds.size() > 1)
            std::snprintf(interval, sizeof interval, "[%+.1f, %+.1f]", p.ci95_lo, p.ci95_hi);
        std::snprintf(buf, sizeof buf,
                      "| %s → %s | %+.1f | %s | %.4f | %.4f | %s | %s | %s |\n",
                      p.cell_a.c_str(), p.cell_b.c_str(), p.mean_diff, interval,
                      p.p_permutation, p.p_holm,
                      number_or_null(p.pnl_diff_per_lot, p.per_lot_available, "%+.2f").c_str(),
                      p.statistically_significant ? "yes" : "no",
                      p.per_lot_available ? (p.economically_material ? "yes" : "no") : "n/a");
        out += buf;
    }
    out += "\n";
    for (const std::string& f : r.findings) out += "- " + f + "\n";
    if (!r.findings.empty()) out += "\n";
    out += "## Interpretation\n\nCompare the Poisson and frozen-observation controls as "
           "sensitivity checks. Compare the per-lot point estimate with the planned economic "
           "threshold and its paired PnL result with the adjusted test. A non-rejection does "
           "not establish equivalence.\n\n";
    out += "## Reproducibility\n\n`arbiter flagship --reproduce results.json --report report.md` "
           "recomputes the outputs and compares their bytes. The recorded provenance is carried "
           "into the comparison. Verify it against the original clean build separately.\n";
    return out;
}

Status write_flagship_artifacts(const std::string& dir, const FlagshipResult& r) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return fail(Errc::IoError, 0);
    const std::string json_path = dir + "/results.json";
    const std::string report_path = dir + "/report.md";
    const std::string json = flagship_to_json(r);
    if (!write_file(json_path, json)) return fail(Errc::IoError, 1);
    const std::string md = flagship_report_md(r);
    if (!write_file(report_path, md)) return fail(Errc::IoError, 2);
    return good();
}

FlagshipReproReport flagship_reproduce(const std::string& artifact_path,
                                       const std::string& report_path) {
    FlagshipReproReport rep;
    rep.detail = "ok";
    std::string text;
    if (!read_file(artifact_path, text)) {
        rep.detail = "cannot read artifact: " + artifact_path;
        return rep;
    }
    u64 schema = 0;
    if (!scan_int(text, "schema_version", schema) || schema != 2) {
        rep.detail = "unsupported artifact schema; use the recorded generating revision";
        return rep;
    }
    std::string name, flow;
    if (!scan_key(text, "experiment", name)) {
        rep.detail = "missing experiment name";
        return rep;
    }
    if (!scan_key(text, "flow_model", flow)) {
        rep.detail = "missing flow_model";
        return rep;
    }
    if(flow!="clustered" && flow!="poisson") { rep.detail="invalid flow_model"; return rep; }
    u64 def_hash = 0;
    if (!scan_int(text, "definition_hash", def_hash)) {
        rep.detail = "missing definition_hash";
        return rep;
    }
    i64 total_ns = 0;
    {
        if (!scan_int(text, "total_latency_ns", total_ns) || total_ns < 0) {
            rep.detail = "missing total_latency_ns";
            return rep;
        }
    }
    double self_exc = 0;
    if (!scan_num(text, "taker_self_excitation", self_exc)) {
        rep.detail = "missing taker_self_excitation";
        return rep;
    }
    bool shadow = false;
    {
        const std::string key = "\"bots_observe_shadow\": ";
        const auto at = text.find(key);
        if (at == std::string::npos) { rep.detail = "missing bots_observe_shadow"; return rep; }
        size_t pos = at + key.size();
        if (text.compare(pos, 4, "true") == 0) { shadow = true; pos += 4; }
        else if (text.compare(pos, 5, "false") == 0) pos += 5;
        else { rep.detail = "invalid bots_observe_shadow"; return rep; }
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
        if (pos == text.size() || text[pos] != ',') { rep.detail = "invalid bots_observe_shadow"; return rep; }
    }
    // seeds array
    std::vector<u64> seeds;
    {
        const size_t pos = text.find("\"seeds\": [");
        if (pos == std::string::npos) {
            rep.detail = "missing seeds";
            return rep;
        }
        size_t p = pos + 10;
        while (true) {
            while (p < text.size() && std::isspace(static_cast<unsigned char>(text[p]))) ++p;
            if (p == text.size()) { rep.detail = "incomplete seeds"; return rep; }
            if (text[p] == ']' && seeds.empty()) break;
            u64 seed = 0;
            const auto parsed = std::from_chars(text.data() + p, text.data() + text.size(), seed);
            if (parsed.ec != std::errc{}) { rep.detail = "invalid seed"; return rep; }
            p = static_cast<size_t>(parsed.ptr - text.data());
            seeds.push_back(seed);
            while (p < text.size() && std::isspace(static_cast<unsigned char>(text[p]))) ++p;
            if (p == text.size()) { rep.detail = "incomplete seeds"; return rep; }
            if (text[p] == ']') break;
            if (text[p++] != ',') { rep.detail = "invalid seeds separator"; return rep; }
        }
    }
    // cells: (name, md_share, per-seed final_hash, mm_pnl, filled_qty, mom_pnl)
    struct RepCell {
        std::string name;
        double md_share = 0;
        std::vector<u64> hash;
        std::vector<i64> mm_pnl;
        std::vector<i64> mom_pnl;
    };
    std::vector<RepCell> cells;
    {
        size_t pos = text.find("\"cells\": [");
        if (pos == std::string::npos) {
            rep.detail = "missing cells";
            return rep;
        }
        size_t p = pos;
        while (true) {
            p = text.find("{\"name\": \"", p);
            if (p == std::string::npos || p > text.find("\"pairs\"", pos)) break;
            RepCell cell;
            const size_t nend = text.find('"', p + 10);
            if (nend == std::string::npos) { rep.detail = "invalid cell name"; return rep; }
            cell.name = text.substr(p + 10, nend - (p + 10));
            if(!scan_num(text.substr(p, text.find("]}", p) - p), "md_share", cell.md_share) ||
               cell.md_share<0 || cell.md_share>1) { rep.detail="invalid md_share"; return rep; }
            // per-seed records inside this cell
            const size_t cell_end = text.find("]}", p);
            if (cell_end == std::string::npos) { rep.detail = "incomplete cell"; return rep; }
            std::string rec = text.substr(p, cell_end == std::string::npos
                                                   ? std::string::npos
                                                   : cell_end - p);
            size_t q = 0;
            while ((q = rec.find("{\"seed\":", q)) != std::string::npos) {
                const size_t end=rec.find('}',q);
                if(end==std::string::npos) { rep.detail="incomplete seed record"; return rep; }
                const std::string record=rec.substr(q,end-q+1);
                u64 seed=0, hash=0;
                const size_t index=cell.hash.size();
                if(index>=seeds.size() || !scan_int(record,"seed",seed) || seed!=seeds[index] ||
                   !scan_int(record,"final_hash",hash)) {
                    rep.detail="cell seed order/count/hash mismatch"; return rep;
                }
                i64 mm = 0, mom = 0;
                if(!scan_int(record,"mm_pnl",mm) || !scan_int(record,"mom_pnl",mom)) {
                    rep.detail="invalid seed economics"; return rep;
                }
                cell.hash.push_back(hash);
                cell.mm_pnl.push_back(mm);
                cell.mom_pnl.push_back(mom);
                q += 8;
            }
            if(cell.hash.size()!=seeds.size()) { rep.detail="cell seed count mismatch"; return rep; }
            cells.push_back(std::move(cell));
            p = cell_end + 2;
        }
    }
    if (cells.empty() || seeds.empty()) {
        rep.detail = "no cells/seeds in artifact";
        return rep;
    }

    // rebuild the exact design from the artifact, every design field is
    // recorded in the artifact; nothing is guessed. A definition-hash
    // mismatch means software/version drift and is reported loudly.
    FlagshipConfig fc;
    fc.name = name;
    fc.total_latency_ns = total_ns;
    fc.flow_model = flow == "clustered" ? FlowModel::Clustered : FlowModel::Poisson;
    fc.taker_self_excitation = self_exc;
    fc.bots_observe_shadow = shadow;
    fc.seeds = seeds;
    fc.md_shares.clear();
    for (const RepCell& c : cells) fc.md_shares.push_back(c.md_share);
    {
        for (const char* key : {"horizon_ns", "warmup_ns"}) {
            i64 value = 0;
            if (!scan_int(text, key, value)) {
                rep.detail = std::string("invalid ") + key;
                return rep;
            }
            if (std::string(key) == "horizon_ns") fc.horizon_ns = value;
            else fc.warmup_ns = value;
        }
        double v = 0;
        if (!scan_num(text, "economic_threshold", v)) { rep.detail = "invalid economic_threshold"; return rep; }
        fc.economic_threshold_ticks_qty_per_lot = v;
        u64 u = 0;
        if (!scan_int(text, "permutation_reps", u) || u > std::numeric_limits<size_t>::max()) {
            rep.detail = "invalid permutation_reps"; return rep;
        }
        fc.permutation_reps = static_cast<size_t>(u);
        if (!scan_int(text, "hac_lag", u) || u > std::numeric_limits<size_t>::max()) {
            rep.detail = "invalid hac_lag"; return rep;
        }
        fc.hac_lag = static_cast<size_t>(u);
    }

    const u64 rebuilt_hash = definition_hash(fc);
    if (rebuilt_hash != def_hash) {
        rep.detail = "definition_hash mismatch: rebuilt " + std::to_string(rebuilt_hash) +
                     " vs artifact " + std::to_string(def_hash);
        return rep;
    }
    FlagshipResult res;
    try { res = run_flagship(fc); }
    catch (const std::exception& error) { rep.detail = error.what(); return rep; }
    rep.cells_checked = cells.size();
    rep.seeds_checked = seeds.size();
    for (size_t c = 0; c < cells.size(); ++c) {
        for (size_t k = 0; k < seeds.size(); ++k) {
            const CellSeedOutcome& o = res.cells[c].per_seed[k];
            if (o.final_state_hash != cells[c].hash[k] ||
                o.mm_marked_pnl != cells[c].mm_pnl[k] || o.mom_marked_pnl != cells[c].mom_pnl[k])
                ++rep.mismatches;
        }
    }
    rep.ok = rep.mismatches == 0;
    if (rep.ok) {
        // Verify every serialized outcome and contrast, including fields
        // that do not participate in the causal state hash. Provenance is
        // informational, so preserve the recorded values during comparison.
        for (const auto& [key, value] : {
                 std::pair{"compiler", &res.compiler}, std::pair{"platform", &res.platform},
                 std::pair{"created_utc", &res.created_utc}, std::pair{"core_version", &res.core_version},
                 std::pair{"core_revision", &res.core_revision}, std::pair{"core_source_state", &res.core_source_state}}) {
            if (!scan_key(text, key, *value)) {
                rep.ok = false;
                ++rep.mismatches;
                break;
            }
        }
        if (rep.ok && text != flagship_to_json(res)) {
            rep.ok = false;
            ++rep.mismatches;
        }
    }
    if (rep.ok && !report_path.empty()) {
        std::string report;
        if (!read_file(report_path, report) || report != flagship_report_md(res)) {
            rep.ok = false;
            ++rep.mismatches;
            rep.detail = "report bytes differ or report is unreadable";
        }
    }
    if (!rep.ok && rep.detail == "ok")
        rep.detail = "mismatches: " + std::to_string(rep.mismatches);
    return rep;
}

}  // namespace arbiter
