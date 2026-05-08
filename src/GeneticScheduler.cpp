#include "GeneticScheduler.h"
#include "BergerScheduler.h"
#include "GreedyScheduler.h"
#include "Metrics.h"

#include <vector>
#include <algorithm>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace rrs {

namespace {

// Подсчёт breaks для расписания
int count_breaks(const Schedule& s) {
    int br = 0;
    const int n = s.n_teams();
    for (int t = 0; t < n; ++t) {
        auto pat = s.home_pattern(t);
        int prev = -2;
        for (int x : pat) {
            if (x >= 0 && x == prev) br++;
            prev = x;
        }
    }
    return br;
}

// Мутация A: меняем местами два случайных тура.
// Сохраняет корректность DRR (просто перестановка).
void mutate_swap_tours(Schedule& s, std::mt19937& rng) {
    int R = s.n_tours();
    if (R < 2) return;
    std::uniform_int_distribution<int> dist(0, R - 1);
    int r1 = dist(rng);
    int r2 = dist(rng);
    while (r2 == r1) r2 = dist(rng);
    std::swap(s.tour(r1), s.tour(r2));
}

// Мутация B: для случайной пары команд (a,b) находим два тура, в которых
// она играет, и инвертируем хозяев.
// Это сохраняет инвариант DRR: каждое направление (a→b) и (b→a) играется ровно раз.
void mutate_flip_mirror_pair(Schedule& s, std::mt19937& rng) {
    const int n = s.n_teams();
    std::uniform_int_distribution<int> team_d(0, n - 1);
    int a = team_d(rng), b = team_d(rng);
    while (b == a) b = team_d(rng);

    int idx_ab = -1, idx_ba = -1;  // индексы туров, в которых (a@home, b@away) и наоборот
    int pos_ab = -1, pos_ba = -1;  // индексы матча внутри тура

    for (int r = 0; r < s.n_tours(); ++r) {
        for (int k = 0; k < static_cast<int>(s.tour(r).size()); ++k) {
            const Match& m = s.tour(r)[k];
            if (m.home == a && m.away == b) { idx_ab = r; pos_ab = k; }
            if (m.home == b && m.away == a) { idx_ba = r; pos_ba = k; }
        }
    }
    if (idx_ab < 0 || idx_ba < 0) return;

    // Меняем хозяев в обоих турах. Это инвертирует обе игры — теперь:
    //   тур idx_ab: (b@home, a@away)
    //   тур idx_ba: (a@home, b@away)
    // Сумма по-прежнему: a играет 1 раз дома, b играет 1 раз дома. ОК.
    std::swap(s.tour(idx_ab)[pos_ab].home, s.tour(idx_ab)[pos_ab].away);
    std::swap(s.tour(idx_ba)[pos_ba].home, s.tour(idx_ba)[pos_ba].away);
}

// Crossover: формируем потомка как комбинацию туров двух родителей.
// Стратегия: для каждого индекса тура r выбираем тур от случайного родителя.
// Это может нарушить DRR (некоторые пары попадут 2 раза, другие 0 раз).
// Поэтому делаем repair.
//
// Repair: после сборки находим "лишние" пары и "недостающие". Меняем матчи в
// конфликтующих турах так, чтобы восстановить инвариант. Если не удаётся —
// возвращаем копию первого родителя (fallback).
//
// Для простоты и надёжности: используем "uniform crossover" по парам
// (a,b)-туров: для каждой пары команд (a,b) случайно выбираем тур, в котором
// они играют, от одного из родителей. Это сохраняет инвариант DRR.
//
// Здесь работаем над парами в формате (упорядоченная пара a→b):
//   Каждая упорядоченная пара играется ровно в одном туре.
//   Если для пары (a,b) от родителя P выбран тур r, ставим этот матч в тур r у потомка.
//
// Это требует, чтобы для каждой пары мы знали номер тура у обоих родителей.
Schedule crossover_uniform(const Schedule& p1, const Schedule& p2, std::mt19937& rng) {
    const int n = p1.n_teams();
    const int R = p1.n_tours();

    // Для каждой упорядоченной пары — её тур у каждого родителя
    std::vector<std::vector<int>> tour_p1(n, std::vector<int>(n, -1));
    std::vector<std::vector<int>> tour_p2(n, std::vector<int>(n, -1));
    for (int r = 0; r < R; ++r) {
        for (const auto& m : p1.tour(r)) tour_p1[m.home][m.away] = r;
        for (const auto& m : p2.tour(r)) tour_p2[m.home][m.away] = r;
    }

    // Строим расписание потомка тур за туром, выбирая для каждой пары родителя.
    // Чтобы не нарушать "в туре n/2 матчей и каждая команда играет 1 раз",
    // делаем так: проходим случайным порядком по парам, и пытаемся положить
    // в тур того родителя, у которого свободны обе команды в этом туре.
    // Если оба родителя дают тур, в котором конфликт — выбираем тур с
    // меньшим конфликтом, и потом локально чиним.
    //
    // Для простоты используем стратегию: 50/50 от родителя, без явной починки.
    // Если не удаётся — fallback на копию p1.

    // Список всех упорядоченных пар
    std::vector<std::pair<int,int>> all_pairs;
    all_pairs.reserve(n * (n - 1));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            if (i != j) all_pairs.emplace_back(i, j);

    std::shuffle(all_pairs.begin(), all_pairs.end(), rng);

    // Заполняем туры
    std::vector<Tour> child_tours(R);
    std::vector<std::vector<bool>> busy(R, std::vector<bool>(n, false));
    bool ok = true;
    std::uniform_int_distribution<int> coin(0, 1);

    for (const auto& pr : all_pairs) {
        int a = pr.first, b = pr.second;
        int r1 = tour_p1[a][b];
        int r2 = tour_p2[a][b];

        // Какой родитель?
        int chosen_r = (coin(rng) == 0) ? r1 : r2;
        int alt_r = (chosen_r == r1) ? r2 : r1;

        if (!busy[chosen_r][a] && !busy[chosen_r][b]) {
            child_tours[chosen_r].emplace_back(a, b);
            busy[chosen_r][a] = busy[chosen_r][b] = true;
        } else if (!busy[alt_r][a] && !busy[alt_r][b]) {
            child_tours[alt_r].emplace_back(a, b);
            busy[alt_r][a] = busy[alt_r][b] = true;
        } else {
            // Не помещается ни туда, ни туда — пробуем найти любой тур, где обе свободны
            bool placed = false;
            for (int r = 0; r < R; ++r) {
                if (!busy[r][a] && !busy[r][b]) {
                    child_tours[r].emplace_back(a, b);
                    busy[r][a] = busy[r][b] = true;
                    placed = true;
                    break;
                }
            }
            if (!placed) { ok = false; break; }
        }
    }

    if (!ok) return p1;  // fallback

    Schedule child(n);
    child.reserve(R);
    for (auto& t : child_tours) {
        if (t.size() != static_cast<size_t>(n / 2)) return p1;  // fallback при неполных турах
        child.add_tour(std::move(t));
    }
    return child;
}

} // anonymous namespace

Schedule GeneticScheduler::solve(int n_teams) {
    if (n_teams < 2 || n_teams % 2 != 0) {
        throw std::invalid_argument("Genetic requires even n >= 2");
    }
    const int n = n_teams;
    std::mt19937 rng(params_.seed);

    // Инициализация популяции
    std::vector<Schedule> population;
    population.reserve(params_.population_size);

    // 1 особь — Бергер
    {
        BergerScheduler b;
        population.push_back(b.solve(n));
    }
    // Несколько особей — Greedy с разными сидами
    int n_greedy = std::min(params_.population_size / 4, 8);
    for (int i = 0; i < n_greedy; ++i) {
        GreedyScheduler g;
        Schedule s = g.solve(n);
        if (s.n_tours() == 2 * (n - 1)) population.push_back(std::move(s));
    }
    // Остальное: мутации Бергера
    while (static_cast<int>(population.size()) < params_.population_size) {
        Schedule s = population[0];  // копия Бергера
        int n_mutations = std::uniform_int_distribution<int>(2, 8)(rng);
        for (int k = 0; k < n_mutations; ++k) {
            if (std::uniform_real_distribution<double>(0, 1)(rng) < 0.5)
                mutate_swap_tours(s, rng);
            else
                mutate_flip_mirror_pair(s, rng);
        }
        population.push_back(std::move(s));
    }

    // Оценка фитнеса
    std::vector<int> fitness(population.size());
    auto evaluate_all = [&]() {
        for (size_t i = 0; i < population.size(); ++i) {
            fitness[i] = count_breaks(population[i]);
        }
    };
    evaluate_all();

    // Турнирная селекция
    auto tournament = [&]() -> int {
        std::uniform_int_distribution<int> dist(0, population.size() - 1);
        int best = dist(rng);
        for (int t = 1; t < params_.tournament_size; ++t) {
            int candidate = dist(rng);
            if (fitness[candidate] < fitness[best]) best = candidate;
        }
        return best;
    };

    // Эволюционный цикл
    for (int gen = 0; gen < params_.generations; ++gen) {
        // Сортируем по фитнесу для elite
        std::vector<int> idx(population.size());
        for (size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<int>(i);
        std::sort(idx.begin(), idx.end(),
                  [&](int a, int b){ return fitness[a] < fitness[b]; });

        std::vector<Schedule> new_pop;
        new_pop.reserve(params_.population_size);

        // Elitism
        for (int i = 0; i < params_.elite_count && i < static_cast<int>(idx.size()); ++i) {
            new_pop.push_back(population[idx[i]]);
        }

        // Заполняем остаток через crossover + mutation
        while (static_cast<int>(new_pop.size()) < params_.population_size) {
            int p1_idx = tournament();
            int p2_idx = tournament();
            Schedule child = crossover_uniform(population[p1_idx], population[p2_idx], rng);

            if (std::uniform_real_distribution<double>(0, 1)(rng) < params_.mutation_rate) {
                if (std::uniform_real_distribution<double>(0, 1)(rng) < 0.5)
                    mutate_swap_tours(child, rng);
                else
                    mutate_flip_mirror_pair(child, rng);
            }
            new_pop.push_back(std::move(child));
        }

        population = std::move(new_pop);
        evaluate_all();
    }

    // Возвращаем лучшее
    int best_idx = 0;
    for (size_t i = 1; i < population.size(); ++i) {
        if (fitness[i] < fitness[best_idx]) best_idx = static_cast<int>(i);
    }
    return std::move(population[best_idx]);
}

} // namespace rrs
