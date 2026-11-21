#include "testfw.hpp"
#include "arbiter/stats.hpp"
#include "arbiter/rng.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
using namespace arbiter;
namespace { bool approx(double a, double b, double tol=1e-9) { return std::fabs(a-b)<=tol; } }
AX_TEST(stats_mean_ci_and_corrections) {
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    stats::MeanCI ci = stats::mean_ci(x, 1);
    CHECK(approx(ci.mean, 3.0));
    CHECK(ci.se_iid > 0.0 && ci.se_hac > 0.0);
    CHECK(ci.ci95_lo < 3.0 && ci.ci95_hi > 3.0);
    CHECK(ci.ci95_hac_lo < 3.0 && ci.ci95_hac_hi > 3.0);
    // HAC with strong autocorrelation must not understate vs IID dramatically
    std::vector<double> ar;
    double v = 0.0;
    Rng rng(Rng::stream(3, 3).state);
    for (int i = 0; i < 500; ++i) {
        v = 0.95 * v + (rng.unit() - 0.5);
        ar.push_back(v);
    }
    stats::MeanCI ci2 = stats::mean_ci(ar, 20);
    CHECK(ci2.se_hac >= ci2.se_iid);  // dependence inflates the robust SE
    const double large = std::numeric_limits<double>::max();
    CHECK(approx(stats::pearson({-large, large}, {large, -large}).r, -1));
    auto bh = stats::fdr_bh({0.001, 0.01, 0.5});
    CHECK(bh[0] <= bh[1] && bh[1] <= bh[2]);
    auto bf = stats::bonferroni({0.01, 0.5});
    CHECK(approx(bf[0], 0.02) && approx(bf[1], 1.0));
    const std::vector<double> p = {0.012, 0.03, 0.04, 0.2, 0.5, 0.6, 0.7, 0.8, 0.9};
    const std::vector<double> expected_holm = {0.108, 0.24, 0.28, 1, 1, 1, 1, 1, 1};
    const std::vector<double> expected_bh = {0.108, 0.12, 0.12, 0.45, 0.9, 0.9, 0.9, 0.9, 0.9};
    const auto holm = stats::holm(p);
    const auto known_bh = stats::fdr_bh(p);
    for (size_t i = 0; i < p.size(); ++i) {
        CHECK(approx(holm[i], expected_holm[i]));
        CHECK(approx(known_bh[i], expected_bh[i]));
    }
    const std::vector<double> shuffled = {0.5, 0.012, 0.9, 0.04, 0.7, 0.2, 0.03, 0.8, 0.6};
    const auto shuffled_holm = stats::holm(shuffled);
    for (size_t i = 0; i < shuffled.size(); ++i) {
        const auto original = std::find(p.begin(), p.end(), shuffled[i]);
        CHECK(original != p.end());
        CHECK(approx(shuffled_holm[i], holm[static_cast<size_t>(original - p.begin())]));
    }
    for (size_t i = 1; i < holm.size(); ++i) CHECK(holm[i - 1] <= holm[i]);
    auto folds = stats::walk_forward(100, 4, 0.5);
    CHECK_EQ(folds.size(), 4u);
    CHECK_EQ(folds.front().train_end, 50u);
    CHECK_EQ(folds.back().test_end, 100u);
    for (size_t i = 1; i < folds.size(); ++i) CHECK(folds[i].test_begin >= folds[i - 1].test_end);
    // small datasets: only non-empty test ranges are returned (a clearly
    // documented number of valid folds, never an empty one)
    auto tiny = stats::walk_forward(5, 4, 0.5);
    CHECK_EQ(tiny.size(), 2u);
    for (const auto& f : tiny) CHECK(f.test_end > f.test_begin);
    auto none = stats::walk_forward(2, 4, 0.5);
    CHECK_EQ(none.size(), 1u);
    for (size_t i = 1; i < tiny.size(); ++i) CHECK(tiny[i].test_begin >= tiny[i - 1].test_end);
}


AX_TEST(walk_forward_handles_full_size_domain) {
    const size_t n = std::numeric_limits<size_t>::max();
    CHECK(stats::walk_forward(n, 2, 1.0).empty());
    const auto folds = stats::walk_forward(n, 3, 0.5);
    CHECK_EQ(folds.size(), 3u);
    if (folds.size() != 3) return;
    CHECK_EQ(folds.back().test_end, n);
    for (size_t i = 0; i < folds.size(); ++i) {
        CHECK(folds[i].test_begin < folds[i].test_end);
        if (i) CHECK_EQ(folds[i - 1].test_end, folds[i].test_begin);
    }
}

AX_TEST(paired_sign_flip_resolution_and_monte_carlo) {
    const double p8 = stats::paired_sign_flip_pvalue(std::vector<double>(8, 1), 1, 5000);
    const double p9 = stats::paired_sign_flip_pvalue(std::vector<double>(9, 1), 1, 5000);
    CHECK_EQ(p8, 2.0 / 256);
    CHECK_EQ(p9, 2.0 / 512);
    CHECK_EQ(stats::holm(std::vector<double>(9, p8)).front(), 0.0703125);
    CHECK_EQ(stats::holm(std::vector<double>(9, p9)).front(), 0.03515625);
    CHECK_EQ(stats::paired_sign_flip_pvalue({1e-12, 1e-12, 0}, 1, 5000), 0.5);
    const std::vector<double> large_grid(21, 1);
    const double mc = stats::paired_sign_flip_pvalue(large_grid, 2026, 5000);
    CHECK_EQ(mc, 1.0 / 5001);
    CHECK_EQ(mc, stats::paired_sign_flip_pvalue(large_grid, 2026, 5000));
    CHECK_EQ(stats::paired_sign_flip_pvalue(std::vector<double>(21, 0), 1, 5000), 1.0);
}

AX_TEST_MAIN
