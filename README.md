# Курсовая работа

## Сборка и запуск

### Вариант 1: CMake (рекомендуется)
```bash
mkdir build && cd build
cmake ..
make
./benchmark
```

### Вариант 2: g++ напрямую
```bash
g++ -std=c++20 -O2 -Wall -Iinclude \
    src/Schedule.cpp src/Validator.cpp src/Metrics.cpp \
    src/BergerScheduler.cpp src/GreedyScheduler.cpp \
    src/BacktrackScheduler.cpp src/GeneticScheduler.cpp \
    src/main.cpp -o benchmark
./benchmark
```

## Структура проекта
```
├── include/
│   ├── Schedule.h          — структуры Match, Tour, Schedule
│   ├── Validator.h         — проверка корректности расписания
│   ├── Metrics.h           — подсчёт нарушений и метрики Q
│   ├── IScheduler.h        — интерфейс алгоритмов
│   ├── BergerScheduler.h   — алгоритм Бергера (круговой метод)
│   ├── GreedyScheduler.h   — рандомизированный жадный алгоритм
│   ├── BacktrackScheduler.h— backtracking с отсечениями
│   └── GeneticScheduler.h  — генетический алгоритм
├── src/                    — реализации (.cpp)
├── results/                — CSV и графики
├── make_plots.py           — построение графиков (Python/matplotlib)
└── CMakeLists.txt
```

## Построение графиков
```bash
pip install matplotlib pandas numpy seaborn
python make_plots.py
```
