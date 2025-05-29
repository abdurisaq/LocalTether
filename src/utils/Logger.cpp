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
        
         logFile.open("application.log", std::ios::out | std::ios::app); // Open in append mode
        if (!logFile.is_open()) {
            std::cerr << "CRITICAL: Failed to open log file: application.log" << std::endl;
        }
        Log("Logger initialized. Logging to application.log", LogLevel::Info);
    }
    
    Logger::~Logger() {
        if (logFile.is_open()) {
            logFile.close();
        }
    }
    void Logger::Log(const std::string& message, LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex);
        std::string formatted = FormatMessage(message, level);
        logs.push_back(formatted);
        
        // Also output to console for debugging
        std::cout << formatted << std::endl;
        if (logFile.is_open()) {
            logFile << formatted << std::endl;
        }
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
    void Logger::Trace(const std::string& message) {
        Log(message, LogLevel::Trace);
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
            case LogLevel::Trace:
                return "TRACE";
            default:
                return "UNKNOWN";
        }
    }



    std::string Logger::getKeyName(uint8_t vkCode){
        switch(vkCode) {
            case 0x08: return "BACKSPACE";
            case 0x09: return "TAB";
            case 0x0D: return "ENTER";
            case 0x10: return "SHIFT";
            case 0x11: return "CTRL";
            case 0x12: return "ALT";
            case 0x13: return "PAUSE";
            case 0x14: return "CAPS_LOCK";
            case 0x1B: return "ESC";
            case 0x20: return "SPACE";
            case 0x21: return "PAGE_UP";
            case 0x22: return "PAGE_DOWN";
            case 0x23: return "END";
            case 0x24: return "HOME";
            case 0x25: return "LEFT";
            case 0x26: return "UP";
            case 0x27: return "RIGHT";
            case 0x28: return "DOWN";
            case 0x2C: return "PRINT_SCREEN";
            case 0x2D: return "INSERT";
            case 0x2E: return "DELETE";
            case 0x5B: return "WIN_LEFT";
            case 0x5C: return "WIN_RIGHT";
            case 0x5D: return "CONTEXT_MENU";
            case 0x70: return "F1";
            case 0x71: return "F2";
            case 0x72: return "F3";
            case 0x73: return "F4";
            case 0x74: return "F5";
            case 0x75: return "F6";
            case 0x76: return "F7";
            case 0x77: return "F8";
            case 0x78: return "F9";
            case 0x79: return "F10";
            case 0x7A: return "F11";
            case 0x7B: return "F12";
            default:
                if ((vkCode >= 0x30 && vkCode <= 0x39) || (vkCode >= 0x41 && vkCode <= 0x5A)) {
                    return std::string(1, static_cast<char>(vkCode));
                }
                return "VK_" + std::to_string(vkCode);
        }
    }
}