#include "BacktrackScheduler.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <functional>
#include <limits>
#include <chrono>

namespace rrs {

// Структура состояния поиска
namespace {
struct BTState {
    int n;
    int R;
    std::vector<std::vector<bool>> played_home;
    std::vector<int> home_count;
    std::vector<int> away_count;
    std::vector<int> last_status;
    std::vector<int> current_streak;  // длина текущей серии одинакового статуса
    int total_breaks = 0;

    // Текущее частичное расписание
    std::vector<Tour> tours;

    // Лучшее найденное расписание
    int best_breaks = std::numeric_limits<int>::max();
    Schedule best_schedule;

    // Тайминг
    std::chrono::steady_clock::time_point t_start;
    int time_limit_ms;
    bool timed_out = false;

    explicit BTState(int n_teams, int t_limit_ms)
        : n(n_teams), R(2 * (n_teams - 1)),
          played_home(n_teams, std::vector<bool>(n_teams, false)),
          home_count(n_teams, 0), away_count(n_teams, 0),
          last_status(n_teams, -1), current_streak(n_teams, 0),
          best_schedule(n_teams),
          time_limit_ms(t_limit_ms) {
        t_start = std::chrono::steady_clock::now();
    }

    bool out_of_time() {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t_start).count();
        if (ms > time_limit_ms) { timed_out = true; return true; }
        return false;
    }

    bool pair_available(int i, int j) const {
        return !played_home[i][j] || !played_home[j][i];
    }
};

// Генерируем все совершенные паросочетания на множестве свободных команд.
// Используем рекурсивный enumerate с обрезкой, чтобы не строить все сразу:
// процессируем матчинг "по дороге".
//
// callback вызывается для каждого допустимого матчинга. Если callback вернёт
// false — прерываем перебор (используется для тайм-аута / нахождения решения).
template <class F>
bool enumerate_matchings(BTState& st, std::vector<bool>& busy,
                         Tour& current, F&& callback) {
    if (st.out_of_time()) return false;

    // Найти первую свободную команду — это даёт уникальное представление матчинга.
    int target = -1;
    for (int i = 0; i < st.n; ++i) {
        if (!busy[i]) { target = i; break; }
    }
    if (target == -1) {
        // Матчинг полный — вызываем callback.
        return callback();
    }

    // Перебираем партнёров для target (в любом порядке индексов).
    for (int p = target + 1; p < st.n; ++p) {
        if (busy[p]) continue;
        if (!st.pair_available(target, p)) continue;

        busy[target] = busy[p] = true;
        current.emplace_back(target, p);  // временный матч (хозяин решится позже)

        if (!enumerate_matchings(st, busy, current, callback)) {
            // Откат и выход (timeout / found best)
            current.pop_back();
            busy[target] = busy[p] = false;
            return false;
        }

        current.pop_back();
        busy[target] = busy[p] = false;
    }
    return true;
}

// Перебираем назначения хозяев в матчинге.
// matching — список пар (a, b) без определённого хозяина.
// Для каждого варианта вызываем callback с готовым тур-вектором, временно
// применяя изменения к состоянию. callback возвращает false для прерывания.
template <class F>
bool enumerate_host_assignments(BTState& st, const std::vector<Match>& matching,
                                size_t idx, Tour& tour, F&& callback) {
    if (st.out_of_time()) return false;
    if (idx == matching.size()) {
        return callback();
    }
    int a = matching[idx].home;
    int b = matching[idx].away;

    // Два варианта: (a дома, b в гостях) и (b дома, a в гостях)
    for (int variant = 0; variant < 2; ++variant) {
        int host = (variant == 0) ? a : b;
        int guest = (variant == 0) ? b : a;
        if (st.played_home[host][guest]) continue;  // уже сыграно

        // Применить
        tour.emplace_back(host, guest);
        st.played_home[host][guest] = true;
        int prev_h_status = st.last_status[host];
        int prev_g_status = st.last_status[guest];
        int prev_h_streak = st.current_streak[host];
        int prev_g_streak = st.current_streak[guest];
        int br_added = 0;
        if (st.last_status[host] == 1) { st.current_streak[host]++; br_added++; }
        else st.current_streak[host] = 1;
        if (st.last_status[guest] == 0) { st.current_streak[guest]++; br_added++; }
        else st.current_streak[guest] = 1;
        st.total_breaks += br_added;
        st.last_status[host] = 1;
        st.last_status[guest] = 0;
        st.home_count[host]++;
        st.away_count[guest]++;

        // Branch-and-bound: если уже хуже лучшего — обрезаем
        if (st.total_breaks < st.best_breaks) {
            if (!enumerate_host_assignments(st, matching, idx + 1, tour, callback)) {
                // Откат и выход
                st.home_count[host]--;
                st.away_count[guest]--;
                st.last_status[host] = prev_h_status;
                st.last_status[guest] = prev_g_status;
                st.current_streak[host] = prev_h_streak;
                st.current_streak[guest] = prev_g_streak;
                st.total_breaks -= br_added;
                st.played_home[host][guest] = false;
                tour.pop_back();
                return false;
            }
        }

        // Откат
        st.home_count[host]--;
        st.away_count[guest]--;
        st.last_status[host] = prev_h_status;
        st.last_status[guest] = prev_g_status;
        st.current_streak[host] = prev_h_streak;
        st.current_streak[guest] = prev_g_streak;
        st.total_breaks -= br_added;
        st.played_home[host][guest] = false;
        tour.pop_back();
    }
    return true;
}

bool bt_search(BTState& st, int r);

bool bt_search(BTState& st, int r) {
    if (st.out_of_time()) return false;

    if (r == st.R) {
        // Полное расписание. Сравниваем с лучшим.
        if (st.total_breaks < st.best_breaks) {
            st.best_breaks = st.total_breaks;
            st.best_schedule = Schedule(st.n);
            for (auto& t : st.tours) st.best_schedule.add_tour(t);
        }
        return true;  // продолжаем поиск (хотим найти оптимум)
    }

    // Нижняя граница breaks: даже если оставшиеся туры идеальны, текущее total_breaks
    // не уменьшится. Если оно уже >= best_breaks, обрезаем.
    if (st.total_breaks >= st.best_breaks) return true;  // bound

    // Перебираем матчинги для тура r
    std::vector<bool> busy(st.n, false);
    Tour scratch_matching;

    auto on_matching = [&]() -> bool {
        // scratch_matching содержит пары без хозяев. Перебираем хозяев.
        std::vector<Match> matching = scratch_matching;
        Tour tour_with_hosts;
        auto on_full_tour = [&]() -> bool {
            // Тур построен, идём в следующий
            st.tours.push_back(tour_with_hosts);
            bool cont = bt_search(st, r + 1);
            st.tours.pop_back();
            return cont;
        };
        return enumerate_host_assignments(st, matching, 0, tour_with_hosts, on_full_tour);
    };

    return enumerate_matchings(st, busy, scratch_matching, on_matching);
}
} // anonymous namespace

Schedule BacktrackScheduler::solve(int n_teams) {
    if (n_teams < 2 || n_teams % 2 != 0) {
        throw std::invalid_argument("Backtracking requires even n >= 2");
    }
    BTState st(n_teams, time_limit_ms_);
    bt_search(st, 0);

    // Если за лимит ничего не нашли — возвращаем то что есть (даже пустое).
    if (st.best_schedule.n_tours() == 0) {
        return Schedule(n_teams);
    }
    return st.best_schedule;
}

} // namespace rrs
