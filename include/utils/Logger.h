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

        void Log(const std::string& message, LogLevel level = LogLevel::Info);
        void Debug(const std::string& message);
        void Info(const std::string& message);
        void Warning(const std::string& message);
        void Error(const std::string& message);
        void Critical(const std::string& message);
        
       
        const std::vector<std::string>& GetLogs() const;

        void Clear();
        
    private:
        Logger();
        ~Logger() = default;
        
        
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        
        std::vector<std::string> logs;
        std::mutex mutex;
        
        
        std::string FormatMessage(const std::string& message, LogLevel level);
        
   
        std::string LogLevelToString(LogLevel level);
    };
}