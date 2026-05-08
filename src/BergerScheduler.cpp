#include "BergerScheduler.h"
#include <stdexcept>
#include <vector>

namespace rrs {

// Реализация классического "circle method" (метода круга),
// математически эквивалентного таблицам Бергера.
//
// Метод:
//   1. Команда 0 фиксирована. Команды 1..n-1 размещаются по кругу.
//   2. На каждом туре пары формируются как "соединения по кругу":
//      позиция 0 ↔ позиция n-1, 1 ↔ n-2, 2 ↔ n-3, ..., n/2-1 ↔ n/2.
//   3. После каждого тура команды на позициях 1..n-1 циклически сдвигаются.
//   4. Это гарантирует, что каждая пара (i,j) встретится РОВНО один раз
//      за n-1 туров (теорема о 1-факторизации полного графа K_n).
//
// Назначение хозяина поля по правилу Бергера:
//   - Для тура r и пары на позициях (i, n-1-i):
//       если (r + i) чётно — хозяин из верхней позиции,
//       иначе — из нижней.
//
// Второй круг получается зеркальным отражением первого.

Schedule BergerScheduler::solve(int n_teams) {
    if (n_teams < 2 || n_teams % 2 != 0) {
        throw std::invalid_argument(
            "Berger algorithm requires even n >= 2");
    }
    const int n = n_teams;

    Schedule schedule(n);
    schedule.reserve(2 * (n - 1));

    // arr[k] — команда, стоящая в позиции k.
    // arr[0] фиксирована; arr[1..n-1] вращаются.
    std::vector<int> arr(n);
    for (int i = 0; i < n; ++i) arr[i] = i;

    // Первый круг: n-1 туров
    for (int r = 0; r < n - 1; ++r) {
        Tour tour;
        tour.reserve(n / 2);

        // Формируем пары: позиция i и позиция n-1-i, для i=0..n/2-1
        for (int i = 0; i < n / 2; ++i) {
            int top = arr[i];
            int bottom = arr[n - 1 - i];

            // Назначение хозяина: чередуем по чётности (r + i)
            if ((r + i) % 2 == 0) {
                tour.emplace_back(top, bottom);
            } else {
                tour.emplace_back(bottom, top);
            }
        }

        schedule.add_tour(std::move(tour));

        // Ротация: arr[0] остаётся, arr[1..n-1] сдвигаются циклически вправо.
        // новый arr[1] = старый arr[n-1],
        // новый arr[2] = старый arr[1], и т.д.
        int last = arr[n - 1];
        for (int i = n - 1; i > 1; --i) {
            arr[i] = arr[i - 1];
        }
        arr[1] = last;
    }

    // Второй круг: зеркальный (поменять home и away)
    const int first_round = n - 1;
    for (int r = 0; r < first_round; ++r) {
        Tour mirror;
        mirror.reserve(n / 2);
        for (const auto& m : schedule.tour(r)) {
            mirror.emplace_back(m.away, m.home);
        }
        schedule.add_tour(std::move(mirror));
    }

    return schedule;
}

} // namespace rrs
