#pragma once

#include "IScheduler.h"

namespace rrs {

// Жадный алгоритм составления двухкругового расписания.
//
// Принцип:
//   Для каждого тура r последовательно выбираем матчи из множества U
//   ещё не сыгранных, применяя многокритериальную эвристику:
//
//   приоритет(i,j) = w1 * unplayed_pressure
//                  + w2 * home_balance(i,j)
//                  + w3 * recent_break_penalty
//
//   где:
//     unplayed_pressure — сколько у команд осталось ещё несыгранных матчей
//                          (команды с большим количеством матчей выбираем раньше)
//     home_balance — насколько (i дома, j в гостях) выравнивает баланс
//     recent_break_penalty — штраф, если кто-то из i,j играл такую же
//                            дома/гости 2 тура подряд
//
// Сложность: O(R · n² · log n) — для каждого тура (R = 2(n-1))
//            перебираем O(n²) пар и сортируем по приоритету.

class GreedyScheduler : public IScheduler {
public:
    [[nodiscard]] Schedule solve(int n_teams) override;
    [[nodiscard]] std::string name() const override { return "Greedy"; }
};

} // namespace rrs
