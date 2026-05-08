#include "Validator.h"
#include <sstream>
#include <vector>
#include <set>

namespace rrs {

std::string ValidationResult::report() const {
    std::ostringstream oss;
    if (ok) {
        oss << "[OK] Schedule is valid.";
    } else {
        oss << "[FAIL] " << errors.size() << " validation error(s):\n";
        for (size_t i = 0; i < errors.size(); ++i) {
            oss << "  " << (i + 1) << ". " << errors[i] << "\n";
        }
    }
    return oss.str();
}

ValidationResult Validator::validate(const Schedule& s, const ValidationOptions& opts) {
    ValidationResult res;
    const int n = s.n_teams();

    if (n <= 0 || n % 2 != 0) {
        res.add_error("Number of teams must be positive and even (got " +
                      std::to_string(n) + ")");
        return res;
    }

    const int expected_tours = opts.double_round_robin ? 2 * (n - 1) : (n - 1);
    if (s.n_tours() != expected_tours) {
        res.add_error("Expected " + std::to_string(expected_tours) +
                      " tours, got " + std::to_string(s.n_tours()));
    }

    // (1) В каждом туре ровно n/2 матчей и каждая команда играет ровно один раз
    for (int r = 0; r < s.n_tours(); ++r) {
        const Tour& tour = s.tour(r);

        if (static_cast<int>(tour.size()) != n / 2) {
            res.add_error("Tour " + std::to_string(r + 1) +
                          " has " + std::to_string(tour.size()) +
                          " matches, expected " + std::to_string(n / 2));
            continue;
        }

        std::vector<int> appearance(n, 0);
        for (const auto& m : tour) {
            if (m.home < 0 || m.home >= n || m.away < 0 || m.away >= n) {
                res.add_error("Tour " + std::to_string(r + 1) +
                              ": invalid team index in match (" +
                              std::to_string(m.home) + " vs " +
                              std::to_string(m.away) + ")");
                continue;
            }
            if (m.home == m.away) {
                res.add_error("Tour " + std::to_string(r + 1) +
                              ": team " + std::to_string(m.home) +
                              " plays itself");
                continue;
            }
            appearance[m.home]++;
            appearance[m.away]++;
        }
        for (int i = 0; i < n; ++i) {
            if (appearance[i] != 1) {
                res.add_error("Tour " + std::to_string(r + 1) +
                              ": team " + std::to_string(i) +
                              " appears " + std::to_string(appearance[i]) +
                              " times (expected 1)");
            }
        }
    }

    // (2) Каждая упорядоченная пара (для DRR) или неупорядоченная (для SRR) встречается ровно 1 раз
    if (opts.double_round_robin) {
        // ordered_count[i][j] = сколько раз i принимал j дома
        std::vector<std::vector<int>> ordered_count(n, std::vector<int>(n, 0));
        for (int r = 0; r < s.n_tours(); ++r) {
            for (const auto& m : s.tour(r)) {
                if (m.home >= 0 && m.home < n && m.away >= 0 && m.away < n) {
                    ordered_count[m.home][m.away]++;
                }
            }
        }
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (i == j) continue;
                if (ordered_count[i][j] != 1) {
                    res.add_error("Pair (" + std::to_string(i) + " home vs " +
                                  std::to_string(j) + ") appears " +
                                  std::to_string(ordered_count[i][j]) +
                                  " times (expected 1)");
                }
            }
        }
    } else {
        std::vector<std::vector<int>> unord_count(n, std::vector<int>(n, 0));
        for (int r = 0; r < s.n_tours(); ++r) {
            for (const auto& m : s.tour(r)) {
                auto p = m.unordered_pair();
                unord_count[p.first][p.second]++;
            }
        }
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                if (unord_count[i][j] != 1) {
                    res.add_error("Pair {" + std::to_string(i) + ", " +
                                  std::to_string(j) + "} appears " +
                                  std::to_string(unord_count[i][j]) +
                                  " times (expected 1)");
                }
            }
        }
    }

    // (3) Расширенные ограничения: серии домашних/выездных
    if (opts.max_consecutive_home > 0 || opts.max_consecutive_away > 0) {
        for (int t = 0; t < n; ++t) {
            auto pattern = s.home_pattern(t);
            int run_home = 0, run_away = 0;
            for (int r = 0; r < static_cast<int>(pattern.size()); ++r) {
                if (pattern[r] == 1) {
                    run_home++;
                    run_away = 0;
                    if (opts.max_consecutive_home > 0 &&
                        run_home > opts.max_consecutive_home) {
                        res.add_error("Team " + std::to_string(t) +
                                      ": consecutive home streak of " +
                                      std::to_string(run_home) +
                                      " exceeds limit " +
                                      std::to_string(opts.max_consecutive_home));
                    }
                } else if (pattern[r] == 0) {
                    run_away++;
                    run_home = 0;
                    if (opts.max_consecutive_away > 0 &&
                        run_away > opts.max_consecutive_away) {
                        res.add_error("Team " + std::to_string(t) +
                                      ": consecutive away streak of " +
                                      std::to_string(run_away) +
                                      " exceeds limit " +
                                      std::to_string(opts.max_consecutive_away));
                    }
                }
            }
        }
    }

    return res;
}

} // namespace rrs
