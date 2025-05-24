#pragma once
#include <string>
#include <unordered_map>
#include <any>

//not used yet

namespace LocalTether::Utils {
    class Config {
    public:
        static Config& GetInstance();
        

        bool LoadFromFile(const std::string& filepath);
        bool SaveToFile(const std::string& filepath);
        
        template<typename T>
        void Set(const std::string& key, const T& value);
        

        template<typename T>
        T Get(const std::string& key, const T& defaultValue);

        bool HasKey(const std::string& key);
        
    private:
        Config();
        ~Config() = default;
        
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;
        
        std::unordered_map<std::string, std::any> values;
    };
}