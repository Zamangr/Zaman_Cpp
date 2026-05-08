#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <utility>

namespace rrs {

// Базовая единица расписания: один матч между двумя командами.
// Поле home — индекс команды-хозяина, away — гостей.
// Индексация команд: 0..n-1.
struct Match {
    int home;
    int away;

    Match() : home(-1), away(-1) {}
    Match(int h, int a) : home(h), away(a) {}

    // Сравнение пар команд без учёта хозяина поля
    // (нужно при проверке "каждая пара играет ровно дважды").
    [[nodiscard]] std::pair<int, int> unordered_pair() const {
        return (home < away) ? std::make_pair(home, away)
                             : std::make_pair(away, home);
    }

    bool operator==(const Match& o) const {
        return home == o.home && away == o.away;
    }
};

// Тур — множество матчей, проводимых одновременно.
// В сбалансированном круговом турнире |Tour| = n/2.
using Tour = std::vector<Match>;

// Полное расписание турнира.
// schedule[r] — матчи r-го тура (r = 0..R-1, R = 2(n-1) для двухкругового).
class Schedule {
public:
    Schedule() : n_teams_(0) {}
    explicit Schedule(int n_teams) : n_teams_(n_teams) {}

    // Число команд
    [[nodiscard]] int n_teams() const { return n_teams_; }

    // Число туров
    [[nodiscard]] int n_tours() const { return static_cast<int>(tours_.size()); }

    // Общее число матчей в расписании
    [[nodiscard]] int n_matches() const;

    // Доступ к турам
    [[nodiscard]] const Tour& tour(int r) const { return tours_[r]; }
    Tour& tour(int r) { return tours_[r]; }

    // Все туры (для итерации)
    [[nodiscard]] const std::vector<Tour>& tours() const { return tours_; }

    // Добавить тур
    void add_tour(Tour t) { tours_.push_back(std::move(t)); }

    // Очистить расписание
    void clear() { tours_.clear(); }

    // Зарезервировать память
    void reserve(int n_tours) { tours_.reserve(n_tours); }

    // Установить число команд
    void set_n_teams(int n) { n_teams_ = n; }

    // Получить вектор статусов "дома/в гостях" для команды i по турам.
    // 1 = играет дома в туре r, 0 = в гостях, -1 = не играет (если расписание неполное).
    [[nodiscard]] std::vector<int> home_pattern(int team) const;

    // Текстовое представление расписания (для отладки)
    [[nodiscard]] std::string to_string() const;

private:
    int n_teams_;
    std::vector<Tour> tours_;
};

} // namespace rrs
