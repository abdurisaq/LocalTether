#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <fstream>

namespace LocalTether::Utils {
    enum class LogLevel {
        Trace,
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
        void Trace(const std::string& message);
       
        const std::vector<std::string>& GetLogs() const;

        static std::string getKeyName(uint8_t vkCode);

        void Clear();
        
    private:
        Logger();
        ~Logger();
        
        
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        
        std::vector<std::string> logs;
        std::mutex mutex;
        std::ofstream logFile;
        
        std::string FormatMessage(const std::string& message, LogLevel level);
        
   
        std::string LogLevelToString(LogLevel level);
    };
}