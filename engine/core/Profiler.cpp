#include "engine/core/Profiler.h"

#include <iomanip>

void Profiler::startProfile(const char* name) {
    auto& data = profileMap_[name];
    data.startTime = Clock::now();
    data.running = true;
}

void Profiler::stopProfile(const char* name) {
    auto it = profileMap_.find(name);
    if (it == profileMap_.end() || !it->second.running) {
        return;
    }

    const auto end = Clock::now();
    const auto duration = std::chrono::duration<double, std::milli>(end - it->second.startTime).count();

    it->second.totalMs += duration;
    ++it->second.callCount;
    it->second.running = false;
}

void Profiler::printAndReset(std::ostream& out) {
    out << std::fixed << std::setprecision(3);

    for (auto& [name, data] : profileMap_) {
        if (data.callCount == 0) {
            continue;
        }

        const double avgMs = data.totalMs / static_cast<double>(data.callCount);
        out << "[Profiler] " << name << " count=" << data.callCount << " total_ms=" << data.totalMs
            << " avg_ms=" << avgMs << '\n';

        data.totalMs = 0.0;
        data.callCount = 0;
        data.running = false;
    }
}
