#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace LocalTether::Utils {
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };
    
    class Logger {
    public:
        static Logger& GetInstance();
        
        // Log messages at different levels
        void Log(const std::string& message, LogLevel level = LogLevel::Info);
        void Debug(const std::string& message);
        void Info(const std::string& message);
        void Warning(const std::string& message);
        void Error(const std::string& message);
        void Critical(const std::string& message);
        
        // Get all log messages
        const std::vector<std::string>& GetLogs() const;
        
        // Clear logs
        void Clear();
        
    private:
        Logger();
        ~Logger() = default;
        
        // Prevent copying
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        
        std::vector<std::string> logs;
        std::mutex mutex;
        
        // Format a log message with timestamp and level
        std::string FormatMessage(const std::string& message, LogLevel level);
        
        // Convert log level to string
        std::string LogLevelToString(LogLevel level);
    };
}