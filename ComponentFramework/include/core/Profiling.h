#ifndef PROFILING_H
#define PROFILING_H

#include <iostream>
#include <chrono>
#include <string>
#include <unordered_map>

struct timingTag {
    std::string startLocation;
    std::string endLocation;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTimeMonitor;
    std::chrono::time_point<std::chrono::high_resolution_clock> endTimeMonitor;
    int startLine = 0;
    int endLine = 0;
    int64_t realStartTime = 0;
    int64_t realEndTime = 0;
};


inline std::unordered_map<std::string, timingTag> timingMap;

inline void start(const char* name, const char* file, int line) {  
    timingTag tag;
    tag.startLine = line;
    tag.startLocation = file;
    tag.startTimeMonitor = std::chrono::high_resolution_clock::now();       
    timingMap[name] = tag;
}

inline void end(const char* name, const char* file, int line) {
    auto& tag = timingMap[name];
    tag.endTimeMonitor = std::chrono::high_resolution_clock::now();
    tag.endLine = line;
    tag.endLocation = file;
    tag.realStartTime = std::chrono::time_point_cast<std::chrono::microseconds>(tag.startTimeMonitor).time_since_epoch().count();
    tag.realEndTime = std::chrono::time_point_cast<std::chrono::microseconds>(tag.endTimeMonitor).time_since_epoch().count();
}

inline void printTimingAt(const char* name) {
    auto& tag = timingMap.at(name);
    
    std::cout << "\nProfiling: " << name << "\n"
              << " Start File: " << tag.startLocation << "\n"
              << " Start Line: " << tag.startLine << "\n"
              << " Start Time: " << tag.realStartTime << "\n"
              << " End File:   " << tag.endLocation << "\n"
              << " End Line:   " << tag.endLine << "\n"
              << " End Time:   " << tag.realEndTime << "\n"
              << " Duration:   " << (tag.realEndTime - tag.realStartTime) << " microseconds\n\n";
}

inline void printAllTiming() {
    for (const auto& it : timingMap) {
        printTimingAt(it.first.c_str());
    }
}

inline void clearTiming()
{
    timingMap.clear();
}
#define MonitorTimingStart(name) start(name, __FILE__, __LINE__)
#define MonitorTimingEnd(name) end(name, __FILE__, __LINE__)

#endif