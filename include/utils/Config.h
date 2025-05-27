#pragma once
#include <string>
#include <unordered_map>
#include <any>
#include <cstdint>

//not used yet

namespace LocalTether::Utils {
    class Config {
    public:
        static Config& GetInstance();
        

        bool LoadFromFile();
        bool SaveToFile();
        
        template<typename T>
        void Set(const std::string& key, const T& value);
        

        template<typename T>
        T Get(const std::string& key, const T& defaultValue);

        bool HasKey(const std::string& key);

        static const std::string PAUSE_COMBO_KEY;
        static const std::string default_config_filepath_;
        
    private:
        Config();
        ~Config() = default;
        
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;
        
        std::unordered_map<std::string, std::any> values;
    };
}