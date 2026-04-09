#pragma once

#include <chrono>
#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>

class Profiler {
public:
    using Clock = std::chrono::steady_clock;

    void startProfile(const char* name);
    void stopProfile(const char* name);

    void printAndReset(std::ostream& out);

private:
    struct ProfileData {
        Clock::time_point startTime{};
        double totalMs = 0.0;
        std::uint64_t callCount = 0;
        bool running = false;
    };

    std::unordered_map<std::string, ProfileData> profileMap_;
};
