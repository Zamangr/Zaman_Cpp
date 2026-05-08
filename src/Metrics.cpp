#include "Metrics.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace rrs {

std::string ScheduleMetrics::to_string() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << "breaks=" << total_breaks
        << ", max_streak=" << max_streak
        << ", Q=" << quality
        << ", F=" << objective;
    return oss.str();
}

ScheduleMetrics Metrics::compute(const Schedule& s, const MetricOptions& opts) {
    ScheduleMetrics m;
    const int n = s.n_teams();
    const int R = s.n_tours();
    if (n <= 0 || R <= 0) return m;

    int penalty_count = 0;

    for (int t = 0; t < n; ++t) {
        auto pattern = s.home_pattern(t);
        int streak = 1;
        int prev = -2;
        for (int r = 0; r < R; ++r) {
            if (pattern[r] < 0) { prev = -2; streak = 1; continue; }
            if (pattern[r] == prev) {
                streak++;
                m.total_breaks++;     // каждый "повтор статуса" = один break
                m.max_streak = std::max(m.max_streak, streak);
                if (opts.hard_streak_limit > 0 && streak >= opts.hard_streak_limit) {
                    penalty_count++;
                }
            } else {
                streak = 1;
                m.max_streak = std::max(m.max_streak, streak);
            }
            prev = pattern[r];
        }
    }

    const double max_possible = static_cast<double>(n) * std::max(1, R - 1);
    m.quality = 1.0 - (static_cast<double>(m.total_breaks) / max_possible);
    if (m.quality < 0.0) m.quality = 0.0;
    if (m.quality > 1.0) m.quality = 1.0;

    m.objective = static_cast<double>(m.total_breaks) +
                  opts.penalty_weight * static_cast<double>(penalty_count);

    return m;
}

} // namespace rrs
