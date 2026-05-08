#pragma once

#include "Schedule.h"
#include <string>

namespace rrs {

// Метрики качества расписания.
//
// Основная содержательная метрика — число "breaks" (нарушений чередования):
// два подряд идущих тура, в которых команда играет в одном статусе (оба дома или оба в гостях).
//
// Для двухкругового турнира с n командами:
//   - всего смен статуса возможно n*(R-1), где R = 2(n-1)
//   - идеальное расписание Бергера обычно достигает минимума breaks ~= n
//     (полностью без breaks невозможно для DRR при n > 2)
//
// Q = 1 - (breaks / n*(R-1))   ∈ [0, 1], чем ближе к 1, тем лучше.
struct ScheduleMetrics {
    int total_breaks = 0;          // суммарное число breaks по всем командам
    int max_streak = 0;            // максимальная длина серии одного статуса
    double quality = 0.0;          // нормированное качество Q ∈ [0,1]
    double objective = 0.0;        // целевая функция F(S) = breaks + λ·penalty

    [[nodiscard]] std::string to_string() const;
};

// Параметры расчёта целевой функции
struct MetricOptions {
    int hard_streak_limit = 0;     // 0 = без штрафа; >0 = штраф за серию ≥ предела
    double penalty_weight = 100.0; // вес штрафа в целевой функции
};

class Metrics {
public:
    [[nodiscard]] static ScheduleMetrics compute(
        const Schedule& s,
        const MetricOptions& opts = {});
};

} // namespace rrs
