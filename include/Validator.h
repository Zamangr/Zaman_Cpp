#pragma once

#include "Schedule.h"
#include <string>
#include <vector>

namespace rrs {

// Результат валидации: успех + список диагностических сообщений
struct ValidationResult {
    bool ok = true;
    std::vector<std::string> errors;

    // Конструктор успешного результата
    static ValidationResult success() { return {}; }

    // Регистрация ошибки
    void add_error(std::string msg) {
        ok = false;
        errors.push_back(std::move(msg));
    }

    [[nodiscard]] std::string report() const;
};

// Опции валидации.
// Базовые инварианты двухкругового турнира проверяются всегда.
// Расширенные ограничения (max_consecutive_home) — опционально.
struct ValidationOptions {
    bool double_round_robin = true;        // двухкруговой (каждая пара 2 раза, 1 дома + 1 в гостях)
    int max_consecutive_home = 0;          // 0 = не проверять; >0 = ограничение на серию
    int max_consecutive_away = 0;
};

// Проверка расписания.
// Базовые инварианты:
//   1. Число туров = 2(n-1) (для двухкругового) или n-1 (для однокругового).
//   2. В каждом туре ровно n/2 матчей (n чётное).
//   3. В каждом туре каждая команда играет ровно 1 раз.
//   4. Каждая упорядоченная пара (i,j), i≠j, встречается ровно 1 раз
//      (для двухкругового: (i,j) и (j,i) — разные матчи).
class Validator {
public:
    [[nodiscard]] static ValidationResult validate(
        const Schedule& s,
        const ValidationOptions& opts = {});
};

} // namespace rrs
