#include "Schedule.h"
#include "Validator.h"
#include "Metrics.h"
#include "BergerScheduler.h"
#include "GreedyScheduler.h"
#include "BacktrackScheduler.h"
#include "GeneticScheduler.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <memory>
#include <vector>
#include <string>
#include <climits>

namespace rrs {

struct BenchResult {
    std::string algo;
    int n;
    double time_ms;
    int breaks;
    int max_streak;
    double quality;
    bool valid;
};

BenchResult bench(IScheduler& alg, int n, int runs) {
    BenchResult r{alg.name(), n, 0, 0, 0, 0, false};
    Schedule first;
    double total_ms = 0;

    for (int k = 0; k < runs; ++k) {
        auto t0 = std::chrono::high_resolution_clock::now();
        Schedule s = alg.solve(n);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        total_ms += ms;
        if (k == 0) first = std::move(s);
    }

    r.time_ms = total_ms / runs;
    auto v = Validator::validate(first);
    auto m = Metrics::compute(first);
    r.breaks = m.total_breaks;
    r.max_streak = m.max_streak;
    r.quality = m.quality;
    r.valid = v.ok;
    return r;
}

void print_header() {
    std::cout << std::left
              << std::setw(15) << "Algorithm"
              << std::setw(6)  << "n"
              << std::setw(15) << "Time (ms)"
              << std::setw(10) << "Breaks"
              << std::setw(10) << "MaxStrk"
              << std::setw(10) << "Q"
              << std::setw(8)  << "Valid"
              << "\n";
    std::cout << std::string(74, '-') << "\n";
}

void print_row(const BenchResult& r) {
    std::cout << std::left
              << std::setw(15) << r.algo
              << std::setw(6)  << r.n
              << std::setw(15) << std::fixed << std::setprecision(4) << r.time_ms
              << std::setw(10) << r.breaks
              << std::setw(10) << r.max_streak
              << std::setw(10) << std::fixed << std::setprecision(4) << r.quality
              << std::setw(8)  << (r.valid ? "YES" : "NO")
              << "\n";
}

} // namespace rrs

int main() {
    using namespace rrs;

    std::cout << "==================================================================\n";
    std::cout << "  Round-Robin Scheduling: Comparative Benchmark\n";
    std::cout << "==================================================================\n\n";

    std::vector<int> ns = {6, 8, 10, 12};
    std::vector<BenchResult> results;
    print_header();

    for (int n : ns) {
        {
            BergerScheduler alg;
            BenchResult r = bench(alg, n, 100);
            print_row(r); results.push_back(r);
        }
        {
            GreedyScheduler alg;
            BenchResult r = bench(alg, n, 30);
            print_row(r); results.push_back(r);
        }
        {
            int t_limit = (n <= 8) ? 2000 : (n == 10 ? 5000 : 10000);
            BacktrackScheduler alg(t_limit);
            BenchResult r = bench(alg, n, 1);
            print_row(r); results.push_back(r);
        }
        {
            GeneticScheduler::Params p;
            p.population_size = 50;
            p.generations = 100;
            GeneticScheduler alg(p);
            BenchResult r = bench(alg, n, 5);
            print_row(r); results.push_back(r);
        }
        std::cout << "\n";
    }

    // CSV экспорт
    std::ofstream csv("results/benchmark.csv");
    csv << "algorithm,n,time_ms,breaks,max_streak,quality,valid\n";
    for (const auto& r : results) {
        csv << r.algo << "," << r.n << "," << r.time_ms << ","
            << r.breaks << "," << r.max_streak << "," << r.quality << ","
            << (r.valid ? 1 : 0) << "\n";
    }
    csv.close();
    std::cout << "Results saved to results/benchmark.csv\n";

    // GA stability test
    std::cout << "\n=== GA stability test (n=8, 30 runs) ===\n";
    std::ofstream csv2("results/ga_stability.csv");
    csv2 << "run,breaks,quality,time_ms\n";
    int min_b = INT_MAX, max_b = 0;
    double sum_b = 0;
    for (int run = 0; run < 30; ++run) {
        GeneticScheduler::Params p;
        p.population_size = 50;
        p.generations = 100;
        p.seed = static_cast<std::uint32_t>(run + 1);
        GeneticScheduler alg(p);

        auto t0 = std::chrono::high_resolution_clock::now();
        Schedule s = alg.solve(8);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        auto m = Metrics::compute(s);
        csv2 << run << "," << m.total_breaks << "," << m.quality << "," << ms << "\n";
        if (m.total_breaks < min_b) min_b = m.total_breaks;
        if (m.total_breaks > max_b) max_b = m.total_breaks;
        sum_b += m.total_breaks;
    }
    csv2.close();
    std::cout << "  Min breaks: " << min_b << "\n";
    std::cout << "  Max breaks: " << max_b << "\n";
    std::cout << "  Mean breaks: " << (sum_b / 30) << "\n";
    std::cout << "  Saved to results/ga_stability.csv\n";

    return 0;
}
