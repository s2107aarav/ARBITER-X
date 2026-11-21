// Robust statistics for dependent HF observations.
#include "arbiter/stats.hpp"
#include "arbiter/rng.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace arbiter {
namespace stats {

namespace {
double mean_of(const std::vector<double>& x) {
    if (x.empty()) return 0.0;
    double s = 0.0;
    for (double v : x) s += v;
    return s / static_cast<double>(x.size());
}
void validate_pvalues(const std::vector<double>& p) {
    for (double v : p)
        if (!(v >= 0.0 && v <= 1.0)) throw std::invalid_argument("p-value outside [0,1]");
}
}  // namespace

double hac_variance(const std::vector<double>& x, size_t lag) {
    const size_t n = x.size();
    if (n < 2) return 0.0;
    const double m = mean_of(x);
    std::vector<double> d(n);
    for (size_t i = 0; i < n; ++i) d[i] = x[i] - m;
    double gamma0 = 0.0;
    for (double v : d) gamma0 += v * v;
    gamma0 /= static_cast<double>(n);
    double v = gamma0;
    const size_t L = std::min(lag, n - 1);
    for (size_t j = 1; j <= L; ++j) {
        double gj = 0.0;
        for (size_t i = j; i < n; ++i) gj += d[i] * d[i - j];
        gj /= static_cast<double>(n);
        const double w = 1.0 - static_cast<double>(j) / static_cast<double>(L + 1);  // Bartlett
        v += 2.0 * w * gj;
    }
    return v / static_cast<double>(n);  // variance of the mean
}

MeanCI mean_ci(const std::vector<double>& x, size_t hac_lag) {
    MeanCI r;
    r.n = x.size();
    if (x.empty()) return r;
    r.mean = mean_of(x);
    double s2 = 0.0;
    for (double v : x) s2 += (v - r.mean) * (v - r.mean);
    const double var = r.n > 1 ? s2 / static_cast<double>(r.n - 1) : 0.0;
    r.se_iid = std::sqrt(var / static_cast<double>(r.n));
    const double hv = hac_variance(x, hac_lag);
    r.se_hac = std::sqrt(hv < 0.0 ? 0.0 : hv);
    r.ci95_lo = r.mean - 1.96 * r.se_iid;
    r.ci95_hi = r.mean + 1.96 * r.se_iid;
    r.ci95_hac_lo = r.mean - 1.96 * r.se_hac;
    r.ci95_hac_hi = r.mean + 1.96 * r.se_hac;
    return r;
}

CorrResult pearson(const std::vector<double>& x, const std::vector<double>& y) {
    CorrResult r;
    const size_t n = std::min(x.size(), y.size());
    r.n = n;
    if (n < 2) return r;
    double scale_x = 0, scale_y = 0;
    for (size_t i = 0; i < n; ++i) {
        scale_x = std::max(scale_x, std::abs(x[i]));
        scale_y = std::max(scale_y, std::abs(y[i]));
    }
    if (scale_x == 0 || scale_y == 0) return r;
    double mx = 0, my = 0;
    for (size_t i = 0; i < n; ++i) {
        mx += x[i] / scale_x;
        my += y[i] / scale_y;
    }
    mx /= n;
    my /= n;
    double sxy = 0, sxx = 0, syy = 0;
    for (size_t i = 0; i < n; ++i) {
        const double dx = x[i] / scale_x - mx, dy = y[i] / scale_y - my;
        sxy += dx * dy;
        sxx += dx * dx;
        syy += dy * dy;
    }
    if (sxx && syy) r.r = std::clamp(sxy / std::sqrt(sxx) / std::sqrt(syy), -1.0, 1.0);
    return r;
}


double paired_sign_flip_pvalue(const std::vector<double>& d, u64 seed, u64 reps) {
    const size_t n = d.size();
    if (n == 0) return 1.0;
    if (reps == 0) throw std::invalid_argument("sign-flip repetitions must be positive");
    double obs = 0.0;
    for (double v : d) obs += v;
    if (!std::isfinite(obs)) throw std::invalid_argument("invalid sign-flip differences");
    // enumerate all sign flips (n <= 20: <= 1M flips)
    if (n <= kExactSignFlipMaxPairs) {
        const size_t flips = 1u << n;
        size_t ge = 0;
        for (size_t f = 0; f < flips; ++f) {
            double s = 0.0;
            for (size_t i = 0; i < n; ++i) s += (f >> i) & 1u ? -d[i] : d[i];
            if (std::fabs(s) >= std::fabs(obs)) ++ge;
        }
        return static_cast<double>(ge) / static_cast<double>(flips);
    }
    // seeded random flips
    Rng rng(Rng::stream(seed, 0x5045524Dull).state);
    u64 ge = 0;
    for (u64 r = 0; r < reps; ++r) {
        double s = 0.0;
        for (size_t i = 0; i < n; ++i) s += rng.chance(0.5) ? -d[i] : d[i];
        if (std::fabs(s) >= std::fabs(obs)) ++ge;
    }
    return (static_cast<double>(ge) + 1.0) / (static_cast<double>(reps) + 1.0);
}

std::vector<double> bonferroni(const std::vector<double>& p) {
    validate_pvalues(p);
    std::vector<double> q(p.size());
    const double m = static_cast<double>(p.size());
    for (size_t i = 0; i < p.size(); ++i) q[i] = std::min(1.0, p[i] * m);
    return q;
}

std::vector<double> holm(const std::vector<double>& p) {
    validate_pvalues(p);
    const size_t m = p.size();
    std::vector<size_t> idx(m);
    for (size_t i = 0; i < m; ++i) idx[i] = i;
    std::stable_sort(idx.begin(), idx.end(), [&](size_t a, size_t b) { return p[a] < p[b]; });
    std::vector<double> adjusted(m, 1.0);
    double running_max = 0.0;
    for (size_t k = 0; k < m; ++k) {
        const double raw = p[idx[k]];
        const double candidate = static_cast<double>(m - k) * raw;
        running_max = std::max(running_max, std::min(1.0, candidate));
        adjusted[idx[k]] = running_max;
    }
    return adjusted;
}

std::vector<double> fdr_bh(const std::vector<double>& p) {
    validate_pvalues(p);
    const size_t m = p.size();
    std::vector<size_t> idx(m);
    for (size_t i = 0; i < m; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) { return p[a] < p[b]; });
    std::vector<double> q(m, 1.0);
    double running = 1.0;
    for (size_t k = m; k-- > 0;) {
        const double v = p[idx[k]] * static_cast<double>(m) / static_cast<double>(k + 1);
        running = std::min(running, v);
        q[idx[k]] = std::min(1.0, running);
    }
    return q;
}

std::vector<Fold> walk_forward(size_t n, size_t n_folds, double min_train_frac) {
    std::vector<Fold> folds;
    if (n_folds == 0 || n < 2 || !std::isfinite(min_train_frac)) return folds;
    min_train_frac = std::clamp(min_train_frac, 0.0, 1.0);
    const long double train_size = std::ceil(static_cast<long double>(n) * min_train_frac);
    if (train_size >= static_cast<long double>(n)) return folds;
    const size_t min_train = std::max<size_t>(1, static_cast<size_t>(train_size));
    if (min_train >= n) return folds;
    n_folds = std::min(n_folds, n - min_train);
    const size_t test_region = n - min_train;
    size_t test_begin = min_train;
    size_t remainder = 0;
    const size_t extra = test_region % n_folds;
    for (size_t f = 0; f < n_folds; ++f) {
        size_t width = test_region / n_folds;
        if (remainder >= n_folds - extra) {
            ++width;
            remainder -= n_folds - extra;
        } else {
            remainder += extra;
        }
        const size_t test_end = test_begin + width;
        if (test_end == test_begin) continue;  // never emit an empty test range
        folds.push_back({0, test_begin, test_begin, test_end});
        test_begin = test_end;
    }
    return folds;
}

std::string format_ci(double lo, double hi) {
    char buf[128];
    std::snprintf(buf, sizeof buf, "[%.6g, %.6g]", lo, hi);
    return std::string(buf);
}

}  // namespace stats
}  // namespace arbiter
