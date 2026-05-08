#include "Schedule.h"
#include <sstream>
#include <iomanip>

namespace rrs {

int Schedule::n_matches() const {
    int total = 0;
    for (const auto& t : tours_) total += static_cast<int>(t.size());
    return total;
}

std::vector<int> Schedule::home_pattern(int team) const {
    std::vector<int> pattern(tours_.size(), -1);
    for (int r = 0; r < static_cast<int>(tours_.size()); ++r) {
        for (const auto& m : tours_[r]) {
            if (m.home == team) {
                pattern[r] = 1;
                break;
            }
            if (m.away == team) {
                pattern[r] = 0;
                break;
            }
        }
    }
    return pattern;
}

std::string Schedule::to_string() const {
    std::ostringstream oss;
    oss << "Schedule[n=" << n_teams_
        << ", tours=" << tours_.size() << "]\n";
    for (int r = 0; r < static_cast<int>(tours_.size()); ++r) {
        oss << "Tour " << std::setw(2) << (r + 1) << ": ";
        for (const auto& m : tours_[r]) {
            oss << "(" << m.home << " vs " << m.away << ") ";
        }
        oss << "\n";
    }
    return oss.str();
}

} // namespace rrs
