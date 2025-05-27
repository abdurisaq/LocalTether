#include "utils/Config.h"
#include <fstream>
#include <iostream>
#include <sstream> // Required for std::stringstream
#include <vector>  // Required for std::vector
#include <algorithm> // Required for std::transform

namespace LocalTether::Utils {

    const std::string Config::PAUSE_COMBO_KEY = "input.pause_combo_vk";
    const std::string Config::default_config_filepath_ = "localtether_config.cfg";

    Config& Config::GetInstance() {
        static Config instance; 
        return instance;
    }
    
    Config::Config() {
      LoadFromFile(); 
    }
    
     bool Config::LoadFromFile() {
        std::ifstream configFile(default_config_filepath_); // Use member variable
        if (!configFile.is_open()) {
            // std::cout << "Config: No config file found or failed to open: " << default_config_filepath_ << ". Using defaults." << std::endl;
            return false;
        }

        values.clear(); 

        std::string line;
        while (std::getline(configFile, line)) {
            std::stringstream ss_line(line);
            std::string key;
            std::string value_str;

            if (std::getline(ss_line, key, '=') && std::getline(ss_line, value_str)) {
                key.erase(0, key.find_first_not_of(" \t\n\r"));
                key.erase(key.find_last_not_of(" \t\n\r") + 1);
                value_str.erase(0, value_str.find_first_not_of(" \t\n\r"));
                value_str.erase(value_str.find_last_not_of(" \t\n\r") + 1);

                if (key == PAUSE_COMBO_KEY) {
                    std::vector<uint8_t> combo;
                    std::stringstream ss_value(value_str);
                    int vk_code_int;
                    while (ss_value >> vk_code_int) {
                        if (vk_code_int > 0 && vk_code_int < 256) {
                            combo.push_back(static_cast<uint8_t>(vk_code_int));
                        }
                    }
                    Set(key, combo);
                } else {
                    if (value_str == "true" || value_str == "false") {
                        Set(key, value_str == "true");
                    } else {
                        try {
                            size_t processed_chars_int = 0;
                            int int_val = std::stoi(value_str, &processed_chars_int);
                            if (processed_chars_int == value_str.length()) {
                                Set(key, int_val);
                                continue;
                            }
                        } catch (const std::invalid_argument&) {
                        } catch (const std::out_of_range&) {}

                        try {
                            size_t processed_chars_float = 0;
                            float float_val = std::stof(value_str, &processed_chars_float);
                             if (processed_chars_float == value_str.length()) {
                                Set(key, float_val);
                                continue;
                            }
                        } catch (const std::invalid_argument&) {
                        } catch (const std::out_of_range&) {}
                        
                        Set(key, value_str);
                    }
                }
            }
        }
        configFile.close();
        // std::cout << "Config: Loaded from " << default_config_filepath_ << std::endl;
        return true;
    }
    
    // Modified: Removed filepath parameter, uses default_config_filepath_
    bool Config::SaveToFile() {
        std::ofstream configFile(default_config_filepath_); // Use member variable
        if (!configFile.is_open()) {
            std::cerr << "Config error: Failed to open config file for writing: " << default_config_filepath_ << std::endl;
            return false;
        }

        for (const auto& pair : values) {
            configFile << pair.first << "=";
            if (pair.first == PAUSE_COMBO_KEY) {
                try {
                    const auto& combo = std::any_cast<const std::vector<uint8_t>&>(pair.second);
                    for (size_t i = 0; i < combo.size(); ++i) {
                        configFile << static_cast<int>(combo[i]) << (i == combo.size() - 1 ? "" : " ");
                    }
                } catch (const std::bad_any_cast&) {
                    configFile << "ErrorSerializingPauseCombo"; 
                }
            } else if (pair.second.type() == typeid(std::string)) {
                configFile << std::any_cast<const std::string&>(pair.second);
            } else if (pair.second.type() == typeid(int)) {
                configFile << std::any_cast<int>(pair.second);
            } else if (pair.second.type() == typeid(float)) {
                configFile << std::any_cast<float>(pair.second);
            } else if (pair.second.type() == typeid(bool)) {
                configFile << (std::any_cast<bool>(pair.second) ? "true" : "false");
            } else {
                configFile << "UnsupportedType"; 
            }
            configFile << std::endl;
        }
        configFile.close();
        // std::cout << "Config: Saved to " << default_config_filepath_ << std::endl;
        return true;
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
                // Attempt conversion if stored as string and T is not string
                if constexpr (!std::is_same_v<T, std::string> && std::is_same_v<decltype(std::any_cast<std::string>(values[key])), std::string>) {
                    try {
                        const std::string& str_val = std::any_cast<const std::string&>(values[key]);
                        if constexpr (std::is_same_v<T, int>) {
                            return static_cast<T>(std::stoi(str_val));
                        } else if constexpr (std::is_same_v<T, float>) {
                            return static_cast<T>(std::stof(str_val));
                        } else if constexpr (std::is_same_v<T, bool>) {
                            std::string temp_str = str_val;
                            std::transform(temp_str.begin(), temp_str.end(), temp_str.begin(), ::tolower);
                            return static_cast<T>(temp_str == "true" || temp_str == "1");
                        }
                    } catch (const std::exception&) {
                        // Conversion failed
                         std::cerr << "Config error: Type mismatch and conversion failed for key " << key << std::endl;
                    }
                } else {
                    std::cerr << "Config error: Type mismatch for key " << key << std::endl;
                }
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
    template void Config::Set<std::vector<uint8_t>>(const std::string&, const std::vector<uint8_t>&); 
    
    template int Config::Get<int>(const std::string&, const int&);
    template float Config::Get<float>(const std::string&, const float&);
    template bool Config::Get<bool>(const std::string&, const bool&);
    template std::string Config::Get<std::string>(const std::string&, const std::string&);
    template std::vector<uint8_t> Config::Get<std::vector<uint8_t>>(const std::string&, const std::vector<uint8_t>&); 
}