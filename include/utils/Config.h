#pragma once
#include <string>
#include <unordered_map>
#include <any>

namespace LocalTether::Utils {
    class Config {
    public:
        static Config& GetInstance();
        
        // Load configuration from file
        bool LoadFromFile(const std::string& filepath);
        
        // Save configuration to file
        bool SaveToFile(const std::string& filepath);
        
        // Set configuration values
        template<typename T>
        void Set(const std::string& key, const T& value);
        
        // Get configuration values
        template<typename T>
        T Get(const std::string& key, const T& defaultValue);
        
        // Check if a key exists
        bool HasKey(const std::string& key);
        
    private:
        Config();
        ~Config() = default;
        
        // Prevent copying
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;
        
        std::unordered_map<std::string, std::any> values;
    };
}