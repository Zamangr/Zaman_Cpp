#pragma once

#include "IScheduler.h"

namespace rrs {

// Генетический алгоритм для оптимизации DRR-расписания.
//
// Кодирование (геном): валидное расписание (Schedule) — каждая особь
// представляет полное корректное двухкруговое расписание. Это упрощает
// поддержание валидности и устраняет необходимость в repair-функциях.
//
// Начальная популяция:
//   - 1 особь от алгоритма Бергера
//   - K особей от рандомизированного Greedy с разными сидами
//   - Остаток — мутации первых
//
// Операторы:
//   1. Турнирная селекция (размер турнира = 3).
//   2. Мутация типа A "swap_tours": меняем местами два тура целиком.
//      Корректность сохраняется (это перестановка туров).
//   3. Мутация типа B "flip_mirror_pair": для пары команд (a,b) находим
//      два тура, в которых она играет (a→b и b→a), и инвертируем хозяев.
//      Корректность DRR сохраняется (каждое направление по-прежнему 1 раз).
//   4. Crossover "tour-set selection": для двух родителей формируем потомка
//      из туров, выбранных как: для каждого тура берём от того родителя,
//      у которого этот тур даёт меньше breaks при склейке с уже выбранными.
//      После сборки — repair (если необходимо).
//
// Fitness: F(S) = total_breaks (минимизируется).
//
// Сложность: O(G · P · n²), где G — поколения, P — размер популяции.

class GeneticScheduler : public IScheduler {
public:
    struct Params {
        int population_size = 50;
        int generations = 200;
        double mutation_rate = 0.3;
        int tournament_size = 3;
        int elite_count = 5;
        std::uint32_t seed = 42;
    };

    GeneticScheduler() = default;
    explicit GeneticScheduler(Params p) : params_(p) {}

    [[nodiscard]] Schedule solve(int n_teams) override;
    [[nodiscard]] std::string name() const override { return "Genetic"; }

private:
    Params params_;
};

} // namespace rrs
