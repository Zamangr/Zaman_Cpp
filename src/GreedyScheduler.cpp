#include "GreedyScheduler.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <functional>
#include <limits>
#include <random>
#include <numeric>

namespace rrs {

// Рандомизированный жадный алгоритм (Randomized Greedy).
//
// Подход:
//   1. Случайно перемешиваем порядок рассмотрения команд.
//   2. На каждом туре жадно строим матчинг с эвристикой
//      "most-constrained-first" + локальным откатом внутри тура.
//   3. Если глобально не удалось завершить расписание (тупик между турами) —
//      перезапускаем алгоритм с новым случайным сидом (до MAX_RESTARTS попыток).
//   4. Из всех успешных запусков берём расписание с минимальным числом breaks.
//
// Сложность одной попытки: ~O(R · n²) при удачной эвристике.
// Общая сложность: O(K · R · n²), где K — число попыток (обычно ≤ 5).

static constexpr int MAX_RESTARTS = 8;
static constexpr int CANDIDATE_RUNS = 3;  // сколько успешных запусков сохранять для выбора лучшего

namespace {

struct GreedyAttempt {
    bool success = false;
    Schedule schedule;
    int breaks = std::numeric_limits<int>::max();
};

GreedyAttempt try_greedy(int n, std::mt19937& rng) {
    GreedyAttempt result;
    const int R = 2 * (n - 1);

    std::vector<std::vector<bool>> played_home(n, std::vector<bool>(n, false));
    std::vector<int> home_count(n, 0);
    std::vector<int> away_count(n, 0);
    std::vector<int> last_status(n, -1);

    auto pair_available = [&](int i, int j) {
        return !played_home[i][j] || !played_home[j][i];
    };

    auto choose_host = [&](int a, int b) -> std::pair<int,int> {
        bool a_home = !played_home[a][b];
        bool b_home = !played_home[b][a];
        if (a_home && !b_home) return {a, b};
        if (!a_home && b_home) return {b, a};
        int diff_a = home_count[a] - away_count[a];
        int diff_b = home_count[b] - away_count[b];
        if (diff_a < diff_b) return {a, b};
        if (diff_b < diff_a) return {b, a};
        if (last_status[a] == 1 && last_status[b] != 1) return {b, a};
        if (last_status[b] == 1 && last_status[a] != 1) return {a, b};
        return {a, b};
    };

    Schedule schedule(n);
    schedule.reserve(R);

    // Случайный порядок рассмотрения команд при равных приоритетах
    std::vector<int> perm(n);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), rng);

    for (int r = 0; r < R; ++r) {
        Tour tour;
        std::vector<bool> busy(n, false);

        std::function<bool()> rec = [&]() -> bool {
            int target = -1;
            int min_p = std::numeric_limits<int>::max();
            for (int i_idx = 0; i_idx < n; ++i_idx) {
                int i = perm[i_idx];
                if (busy[i]) continue;
                int cnt = 0;
                for (int j = 0; j < n; ++j) {
                    if (j == i || busy[j]) continue;
                    if (pair_available(i, j)) cnt++;
                }
                if (cnt < min_p) { min_p = cnt; target = i; }
            }
            if (target == -1) return true;
            if (min_p == 0) return false;

            struct Cand { int p; double sc; };
            std::vector<Cand> cands;
            for (int j = 0; j < n; ++j) {
                if (j == target || busy[j] || !pair_available(target, j)) continue;
                int j_partners = 0;
                for (int k = 0; k < n; ++k) {
                    if (k == j || k == target || busy[k]) continue;
                    if (pair_available(j, k)) j_partners++;
                }
                double sc = -static_cast<double>(j_partners);
                if (last_status[target] == 1 && last_status[j] == 0) sc += 1.0;
                if (last_status[target] == 0 && last_status[j] == 1) sc += 1.0;
                // Небольшой случайный шум для разнообразия
                std::uniform_real_distribution<double> noise(0.0, 0.5);
                sc += noise(rng);
                cands.push_back({j, sc});
            }
            std::sort(cands.begin(), cands.end(),
                      [](const Cand& a, const Cand& b){ return a.sc > b.sc; });

            for (const auto& c : cands) {
                int p = c.p;
                auto [host, guest] = choose_host(target, p);

                int prev_h = last_status[host];
                int prev_g = last_status[guest];
                busy[target] = busy[p] = true;
                played_home[host][guest] = true;
                last_status[host] = 1; last_status[guest] = 0;
                home_count[host]++; away_count[guest]++;
                tour.emplace_back(host, guest);

                if (rec()) return true;

                tour.pop_back();
                home_count[host]--; away_count[guest]--;
                last_status[host] = prev_h;
                last_status[guest] = prev_g;
                played_home[host][guest] = false;
                busy[target] = busy[p] = false;
            }
            return false;
        };

        if (!rec()) {
            return result;  // success остаётся false
        }
        schedule.add_tour(std::move(tour));
    }

    result.success = true;
    result.schedule = std::move(schedule);
    return result;
}

} // anonymous namespace

Schedule GreedyScheduler::solve(int n_teams) {
    if (n_teams < 2 || n_teams % 2 != 0) {
        throw std::invalid_argument("Greedy requires even n >= 2");
    }
    const int n = n_teams;

    // Несколько попыток с разными сидами; берём лучшее по breaks.
    GreedyAttempt best;
    int successes = 0;
    std::mt19937 rng(12345);
    for (int attempt = 0; attempt < MAX_RESTARTS && successes < CANDIDATE_RUNS; ++attempt) {
        std::mt19937 attempt_rng(static_cast<std::uint32_t>(rng() + attempt * 7919));
        GreedyAttempt cur = try_greedy(n, attempt_rng);
        if (cur.success) {
            // Считаем breaks
            int br = 0;
            for (int t = 0; t < n; ++t) {
                auto pat = cur.schedule.home_pattern(t);
                int prev = -2;
                for (int x : pat) {
                    if (x >= 0 && x == prev) br++;
                    prev = x;
                }
            }
            cur.breaks = br;
            if (cur.breaks < best.breaks) {
                best = std::move(cur);
            }
            successes++;
        }
    }

    if (best.success) return std::move(best.schedule);
    // Если совсем ничего не получилось — возвращаем пустое (валидатор скажет об этом)
    return Schedule(n);
}

} // namespace rrs
