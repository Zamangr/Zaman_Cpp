#pragma once

#include "Schedule.h"
#include <string>

namespace rrs {

// Общий интерфейс для всех алгоритмов составления расписания.
// Позволяет main.cpp единообразно прогонять и сравнивать алгоритмы.
class IScheduler {
public:
    virtual ~IScheduler() = default;

    // Решить задачу: построить двухкруговое расписание для n команд.
    [[nodiscard]] virtual Schedule solve(int n_teams) = 0;

    // Имя алгоритма (для логов и таблиц)
    [[nodiscard]] virtual std::string name() const = 0;
};

} // namespace rrs
