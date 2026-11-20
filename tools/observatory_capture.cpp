#include "arbiter/build_info.hpp"
#include "arbiter/world.hpp"

#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
using namespace arbiter;
namespace fs = std::filesystem;

constexpr size_t kCaptureLimit = 6 * 1024 * 1024;

class Capture final : public WorldObserver {
public:
    explicit Capture(size_t capacity = kCaptureLimit) : capacity_(capacity) {}

    void scheduler(ObserverContext ctx, ScheduleBoundary boundary, const WorldEvent& ev) noexcept override {
        const char* kind = boundary == ScheduleBoundary::Scheduled ? "schedule" :
                           boundary == ScheduleBoundary::Begin ? "begin" : "complete";
        write(ctx, kind, [&](auto& out) {
            out << ",\"event\":\"" << ev.tiebreak << "\",\"due\":\"" << ev.t
                << "\",\"rank\":" << +ev.rank << ",\"type\":" << static_cast<u16>(ev.type);
        });
    }

    void frame(ObserverContext ctx, const ObservedFrame& f) noexcept override {
        write(ctx, "frame", [&](auto& out) {
            out << ",\"channel\":" << +f.channel << ",\"seq\":\"" << f.sequence
                << "\",\"order\":\"" << f.order_id << "\",\"action\":" << +f.action
                << ",\"duplicate\":" << (f.duplicate ? "true" : "false")
                << ",\"retransmit\":" << (f.retransmit ? "true" : "false");
            if (f.maker_id) out << ",\"maker\":\"" << f.maker_id << '"';
            if (f.dropped) out << ",\"arrival\":null,\"departure\":null";
            else out << ",\"arrival\":\"" << f.arrival_event << "\",\"departure\":\"" << f.departure << '"';
            if (f.intent)
                out << ",\"intent\":{\"side\":\"" << (is_buy(f.intent->side) ? "buy" : "sell")
                    << "\",\"price\":\"" << f.intent->price << "\",\"qty\":\"" << f.intent->qty
                    << "\",\"account\":" << f.intent->account << '}';
            out << ",\"bytes\":\"";
            constexpr std::string_view digits = "0123456789abcdef";
            for (unsigned char b : f.bytes) out << digits[b >> 4] << digits[b & 15];
            out << '"';
        });
    }

    void receiver(ObserverContext ctx, u8 channel, u64 seq, u64 event,
                  ReceiveOutcome outcome) noexcept override {
        write(ctx, "receive", [&](auto& out) {
            const char* value = outcome == ReceiveOutcome::Accepted ? "delivered" :
                                outcome == ReceiveOutcome::DecodeRejected ? "corrupt" :
                                outcome == ReceiveOutcome::Held ? "held" :
                                outcome == ReceiveOutcome::Retired ? "retired" : "duplicate";
            out << ",\"channel\":" << +channel << ",\"seq\":\"" << seq
                << "\",\"event\":\"" << event << "\",\"outcome\":\"" << value << '"';
        });
    }

    void risk(ObserverContext ctx, u64 order, bool accepted, u64 reason) noexcept override {
        write(ctx, "risk", [&](auto& out) {
            out << ",\"order\":\"" << order << "\",\"accepted\":" << (accepted ? "true" : "false")
                << ",\"reason\":\"" << reason << '"';
        });
    }

    void book(ObserverContext ctx, u64 order, Side side, std::span<const Fill> fills,
              const OptBook& book, bool cancel) noexcept override {
        write(ctx, "book", [&](auto& out) {
            const auto ladder = book.to_ladder();
            out << ",\"order\":\"" << order << "\",\"side\":\"" << (is_buy(side) ? "buy" : "sell")
                << "\",\"cancel\":" << (cancel ? "true" : "false") << ",\"fills\":[";
            bool first = true;
            for (const auto& f : fills) {
                if (!first) out << ',';
                first = false;
                out << "{\"taker\":\"" << f.taker_order_id << "\",\"maker\":\"" << f.maker_order_id
                    << "\",\"price\":\"" << f.price << "\",\"qty\":\"" << f.qty << "\"}";
            }
            out << "],\"asks\":";
            levels(out, ladder.asks);
            out << ",\"bids\":";
            levels(out, ladder.bids);
        });
    }

    void accounting(ObserverContext ctx, i64 mm_auth, i64 mm_local, i64 mom_auth, i64 mom_local,
                    const RiskEngine& risk) noexcept override {
        write(ctx, "accounting", [&](auto& out) {
            out << ",\"mmAuth\":\"" << mm_auth << "\",\"mmLocal\":\"" << mm_local
                << "\",\"momAuth\":\"" << mom_auth << "\",\"momLocal\":\"" << mom_local << '"'
                << ",\"reservations\":[";
            bool first = true;
            for (const auto& [id, item] : risk.reservations()) {
                if (!first) out << ',';
                first = false;
                out << "{\"order\":\"" << id << "\",\"account\":" << item.account
                    << ",\"qty\":\"" << item.signed_qty << "\"}";
            }
            out << ']';
        });
    }

    void feed(ObserverContext ctx, FeedBoundary kind, u64 sequence, u64 next) noexcept override {
        write(ctx, "feed", [&](auto& out) {
            out << ",\"action\":\"" << (kind == FeedBoundary::GapScheduled ? "gap" :
                                        kind == FeedBoundary::SnapshotFallback ? "snapshot" : "apply")
                << "\",\"seq\":\"" << sequence << "\",\"next\":\"" << next << '"';
        });
    }

    void journal(ObserverContext ctx, u64 id, const Event& event) noexcept override {
        write(ctx, "journal", [&](auto& out) {
            out << ",\"record\":\"" << id << "\",\"type\":" << static_cast<u16>(event.type)
                << ",\"a\":\"" << event.a << "\",\"b\":\"" << event.b << "\",\"c\":\"" << event.c << '"';
        });
    }

    bool truncated() const noexcept { return truncated_; }
    size_t count() const noexcept { return count_; }
    const std::string& bytes() const noexcept { return records_; }

private:
    size_t capacity_;
    bool truncated_ = false;
    size_t count_ = 0;
    std::string records_;

    template <typename Append>
    void write(ObserverContext ctx, const char* kind, Append append) noexcept {
        if (truncated_) return;
        try {
            std::ostringstream record;
            record.exceptions(std::ios::badbit | std::ios::failbit);
            record << "{\"kind\":\"" << kind << "\",\"ns\":\"" << ctx.now
                   << "\",\"ordinal\":\"" << ctx.processed_ordinal << '"';
            append(record);
            record << "}\n";
            const auto line = record.view();
            if (line.size() > capacity_ - records_.size()) {
                truncated_ = true;
                return;
            }
            records_.append(line);
            ++count_;
        } catch (...) {
            truncated_ = true;
        }
    }

    static void levels(std::ostream& out, const std::vector<LadderLevel>& levels) {
        out << '[';
        bool first_level = true;
        for (const auto& level : levels) {
            if (!first_level) out << ',';
            first_level = false;
            out << "{\"price\":\"" << level.price << "\",\"orders\":[";
            bool first_order = true;
            for (const auto& order : level.orders) {
                if (!first_order) out << ',';
                first_order = false;
                out << "{\"id\":\"" << order.id << "\",\"qty\":\"" << order.qty
                    << "\",\"leaves\":\"" << order.leaves << "\"}";
            }
            out << "]}";
        }
        out << ']';
    }
};

WorldConfig profile_config(std::string_view profile) {
    WorldConfig config;
    config.journal_events = false;
    config.horizon_ns = 100'000'000;
    config.hash_every_events = 13;
    config.checkpoint_every_events = 31;
    if (profile == "fault") {
        config.order_channel.loss_rate = 0.22;
        config.order_channel.dup_rate = 0.7;
        config.order_channel.corrupt_rate = 0.12;
        config.exec_channel.loss_rate = 0.4;
        config.exec_channel.dup_rate = 0.5;
        config.md_channel.loss_rate = 0.15;
        config.md_channel.reorder_window = 5;
    }
    return config;
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::string read_bytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "Cannot open comparison artifact");
    return {std::istreambuf_iterator<char>(file), {}};
}

struct TempDirectory {
    fs::path path;
    TempDirectory() {
        const auto prefix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        for (int i = 0; i < 32; ++i) {
            const auto candidate = fs::temp_directory_path() / ("arx-observer-" + prefix + "-" + std::to_string(i));
            if (fs::create_directory(candidate)) { path = candidate; return; }
        }
        throw std::runtime_error("Cannot create observer test workspace");
    }
    ~TempDirectory() { std::error_code ignored; fs::remove_all(path, ignored); }
};

void verify(std::string_view profile) {
    TempDirectory temp;
    auto config = profile_config(profile);
    config.journal_events = true;
    const auto plain_path = temp.path / "ordinary.journal";
    const auto observed_path = temp.path / "observed.journal";
    config.journal_path = plain_path.string();
    const u64 seed = profile == "fault" ? 2 : 909;
    World plain(config, seed);
    config.journal_path = observed_path.string();
    Capture capture;
    World observed(config, seed, &capture);
    const auto check_pair = [&](std::string_view stage) {
        const auto a = plain.stats();
        const auto b = observed.stats();
        require(a.events_processed == b.events_processed && a.trades == b.trades &&
                a.risk_accepted == b.risk_accepted && a.risk_rejected == b.risk_rejected &&
                a.mm_auth_inventory == b.mm_auth_inventory && a.mm_inventory == b.mm_inventory &&
                a.mom_auth_inventory == b.mom_auth_inventory && a.mom_inventory == b.mom_inventory &&
                a.hash_timeline == b.hash_timeline && plain.state_hash() == observed.state_hash(),
                "Observed World differs from ordinary run");
        auto baseline = plain.capture_snapshot();
        auto traced = observed.capture_snapshot();
        baseline.config.journal_path.clear();
        traced.config.journal_path.clear();
        const auto p = (temp.path / "ordinary.snapshot").string();
        const auto q = (temp.path / "observed.snapshot").string();
        require(bool(write_world_snapshot(p, baseline)) && bool(write_world_snapshot(q, traced)),
                "Cannot write snapshots for equivalence test");
        require(read_bytes(p) == read_bytes(q), "Snapshot contents differ");
        require(read_bytes(plain_path) == read_bytes(observed_path), "Journal bytes differ");
        std::cout << profile << ' ' << stage << ": equivalent across " << b.events_processed << " events\n";
    };
    plain.run(110);
    observed.run(110);
    check_pair("prefix");
    auto restore_config = config;
    restore_config.journal_events = false;
    restore_config.journal_path.clear();
    const auto snapshot = observed.capture_snapshot();
    World continuation(restore_config, seed);
    require(bool(continuation.restore_snapshot(snapshot, restore_config, true)), "Restore failed");
    const u64 remaining = profile == "fault" ? 390 : 260;
    const auto restored_stats = continuation.run(remaining);
    plain.run(remaining);
    const auto observed_stats = observed.run(remaining);
    check_pair("continuation");
    require(continuation.state_hash() == observed.state_hash() &&
            restored_stats.events_processed == observed_stats.events_processed, "Restored continuation differs");
    require(!capture.truncated() && capture.count() > 370 && observed_stats.trades > 0,
            "Capture must include meaningful complete execution");
    if (profile == "fault")
        require(observed_stats.order_frames_dropped > 0 && observed_stats.order_frames_duplicated > 0 &&
                observed_stats.wire_frames_corrupted > 0 && observed_stats.md_gap_events > 0 &&
                observed_stats.md_recovered > 0, "Fault profile missed required outcomes");
    Capture tiny(32);
    World limited(restore_config, seed, &tiny);
    limited.run(1);
    require(tiny.truncated() && tiny.count() == 0, "Overflow was not detected");
}

u64 number(std::string_view text) {
    u64 result = 0;
    const auto [end, status] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (status != std::errc{} || end != text.data() + text.size()) throw std::runtime_error("Expected an unsigned integer");
    return result;
}

bool export_run(const fs::path& path, std::string_view profile, u64 seed, u64 budget) {
    const auto config = profile_config(profile);
    Capture capture;
    World world(config, seed, &capture);
    const auto result = world.run(budget);
    std::ofstream out(path, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << "{\"schema\":\"ARX-WORLD-OBS/1\",\"profile\":\"" << profile << "\",\"commit\":\"" << build::revision()
        << "\",\"sourceState\":\"" << build::source_state() << "\",\"engine\":\"" << build::version()
        << "\",\"seed\":\"" << seed << "\",\"config\":\"" << world.config_hash()
        << "\",\"eventBudget\":\"" << budget << "\",\"mmAccount\":" << config.mm.account
        << ",\"momentumAccount\":" << config.momentum.account << "}\n";
    out << capture.bytes();
    out << "{\"kind\":\"end\",\"records\":" << capture.count()
        << ",\"truncated\":" << (capture.truncated() ? "true" : "false")
        << ",\"processed\":\"" << result.events_processed << "\",\"ns\":\"" << result.logical_end
        << "\",\"worldHash\":\"" << world.state_hash() << "\"}\n";
    out.close();
    std::cout << "Captured " << capture.count() << " records across " << result.events_processed << " World events"
              << (capture.truncated() ? " (truncated)" : "") << '\n';
    return !capture.truncated();
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--verify") {
            verify("normal");
            verify("fault");
            return 0;
        }
        if (argc < 3 || argc > 5 ||
            (std::string_view(argv[2]) != "normal" && std::string_view(argv[2]) != "fault")) {
            std::cerr << "Usage: arbiter_observe PATH normal|fault [SEED=909] [EVENTS=370]\n"
                         "       arbiter_observe --verify\n";
            return 2;
        }
        const auto budget = argc > 4 ? number(argv[4]) : u64{370};
        if (!budget || budget > 2000) throw std::runtime_error("Event budget must be 1 to 2000");
        return export_run(argv[1], argv[2], argc > 3 ? number(argv[3]) : 909, budget) ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "arbiter_observe: " << e.what() << '\n';
        return 1;
    }
}
