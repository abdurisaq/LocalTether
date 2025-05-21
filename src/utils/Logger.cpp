#include "utils/Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace LocalTether::Utils {
    
    Logger& Logger::GetInstance() {
        static Logger instance;
        return instance;
    }
    
    Logger::Logger() {
        // Initialize logger
    }
    
    void Logger::Log(const std::string& message, LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex);
        std::string formatted = FormatMessage(message, level);
        logs.push_back(formatted);
        
        // Also output to console for debugging
        std::cout << formatted << std::endl;
    }
    
    void Logger::Debug(const std::string& message) {
        Log(message, LogLevel::Debug);
    }
    
    void Logger::Info(const std::string& message) {
        Log(message, LogLevel::Info);
    }
    
    void Logger::Warning(const std::string& message) {
        Log(message, LogLevel::Warning);
    }
    
    void Logger::Error(const std::string& message) {
        Log(message, LogLevel::Error);
    }
    
    void Logger::Critical(const std::string& message) {
        Log(message, LogLevel::Critical);
    }
    
    const std::vector<std::string>& Logger::GetLogs() const {
        return logs;
    }
    
    void Logger::Clear() {
        std::lock_guard<std::mutex> lock(mutex);
        logs.clear();
    }
    
    std::string Logger::FormatMessage(const std::string& message, LogLevel level) {
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        auto now_tm = std::localtime(&now_time_t);
        
        std::stringstream ss;
        ss << "[" << std::put_time(now_tm, "%H:%M:%S") << "] ";
        ss << "[" << LogLevelToString(level) << "] ";
        ss << message;
        
        return ss.str();
    }
    
    std::string Logger::LogLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::Debug:
                return "DEBUG";
            case LogLevel::Info:
                return "INFO";
            case LogLevel::Warning:
                return "WARNING";
            case LogLevel::Error:
                return "ERROR";
            case LogLevel::Critical:
                return "CRITICAL";
            default:
                return "UNKNOWN";
        }
    }
}