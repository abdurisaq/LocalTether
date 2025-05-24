#include "utils/Config.h"
#include <fstream>
#include <iostream>

//unimplemented configs
namespace LocalTether::Utils {
    
    Config& Config::GetInstance() {
        static Config instance;
        return instance;
    }
    
    Config::Config() {
      
    }
    
    bool Config::LoadFromFile(const std::string& filepath) {
       
        return false;
    }
    
    bool Config::SaveToFile(const std::string& filepath) {
   
        return false;
    }
    
    template<typename T>
    void Config::Set(const std::string& key, const T& value) {
        values[key] = value;
    }
    
    template<typename T>
    T Config::Get(const std::string& key, const T& defaultValue) {
        if (HasKey(key)) {
            try {
                return std::any_cast<T>(values[key]);
            } catch (const std::bad_any_cast&) {
                std::cerr << "Config error: Type mismatch for key " << key << std::endl;
            }
        }
        return defaultValue;
    }
    
    bool Config::HasKey(const std::string& key) {
        return values.find(key) != values.end();
    }
    
    // Explicit template instantiations
    template void Config::Set<int>(const std::string&, const int&);
    template void Config::Set<float>(const std::string&, const float&);
    template void Config::Set<bool>(const std::string&, const bool&);
    template void Config::Set<std::string>(const std::string&, const std::string&);
    
    template int Config::Get<int>(const std::string&, const int&);
    template float Config::Get<float>(const std::string&, const float&);
    template bool Config::Get<bool>(const std::string&, const bool&);
    template std::string Config::Get<std::string>(const std::string&, const std::string&);
}
