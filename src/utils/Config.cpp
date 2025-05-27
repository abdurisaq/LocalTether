#include "utils/Config.h"
#include <fstream>
#include <iostream>
#include <sstream>  
#include <vector>   
#include <algorithm>  

namespace LocalTether::Utils {

    const std::string Config::PAUSE_COMBO_KEY = "input.pause_combo_vk";
    const std::string Config::default_config_filepath_ = "localtether_config.cfg";

    Config& Config::GetInstance() {
        static Config instance; 
        return instance;
    }
    
    Config::Config() {
      std::cerr << "Config: Constructor called. Attempting to load from file." << std::endl;
      if (!LoadFromFile()) { 
          std::cerr << "Config: LoadFromFile failed in constructor." << std::endl;
      } else {
          std::cerr << "Config: LoadFromFile succeeded in constructor." << std::endl;
      }
    }
    
    bool Config::LoadFromFile() {
        std::cerr << "Config::LoadFromFile: Attempting to open: " << default_config_filepath_ << std::endl;  
        std::ifstream configFile(default_config_filepath_); 
        if (!configFile.is_open()) {
            std::cerr << "Config::LoadFromFile: FAILED to open config file: " << default_config_filepath_ << std::endl;  
            return false;
        }
        std::cerr << "Config::LoadFromFile: Successfully opened config file: " << default_config_filepath_ << std::endl;  

        values.clear(); 

        std::string line;
        int line_num = 0;
        while (std::getline(configFile, line)) {
            line_num++;
            std::cerr << "Config::LoadFromFile: Processing line " << line_num << ": " << line << std::endl;  
            std::stringstream ss_line(line);
            std::string key;
            std::string value_str;

            if (std::getline(ss_line, key, '=') && std::getline(ss_line, value_str)) {
                key.erase(0, key.find_first_not_of(" \t\n\r"));
                key.erase(key.find_last_not_of(" \t\n\r") + 1);
                value_str.erase(0, value_str.find_first_not_of(" \t\n\r"));
                value_str.erase(value_str.find_last_not_of(" \t\n\r") + 1);
                std::cerr << "Config::LoadFromFile: Parsed key='" << key << "', value_str='" << value_str << "'" << std::endl;  

                if (key == PAUSE_COMBO_KEY) {
                    std::vector<uint8_t> combo;
                    std::stringstream ss_value(value_str);
                    int vk_code_int;
                    std::string parsed_combo_log = "Parsed combo for " + key + ": ";
                    while (ss_value >> vk_code_int) {
                        parsed_combo_log += std::to_string(vk_code_int) + " ";
                        if (vk_code_int > 0 && vk_code_int < 256) {
                            combo.push_back(static_cast<uint8_t>(vk_code_int));
                        }
                    }
                    std::cerr << "Config::LoadFromFile: " << parsed_combo_log << std::endl;  
                    Set(key, combo);
                } else {
                    
                    if (value_str == "true" || value_str == "false") {
                        std::cerr << "Config::LoadFromFile: Setting bool for key " << key << std::endl;  
                        Set(key, value_str == "true");
                    } else {
                        bool set_as_specific_type = false;
                        try {
                            size_t processed_chars_int = 0;
                            int int_val = std::stoi(value_str, &processed_chars_int);
                            if (processed_chars_int == value_str.length()) {
                                std::cerr << "Config::LoadFromFile: Setting int for key " << key << std::endl;  
                                Set(key, int_val);
                                set_as_specific_type = true;
                            }
                        } catch (const std::invalid_argument&) {
                        } catch (const std::out_of_range&) {}

                        if (!set_as_specific_type) {
                            try {
                                size_t processed_chars_float = 0;
                                float float_val = std::stof(value_str, &processed_chars_float);
                                 if (processed_chars_float == value_str.length()) {
                                    std::cerr << "Config::LoadFromFile: Setting float for key " << key << std::endl;  
                                    Set(key, float_val);
                                    set_as_specific_type = true;
                                }
                            } catch (const std::invalid_argument&) {
                            } catch (const std::out_of_range&) {}
                        }
                        
                        if (!set_as_specific_type) {
                            std::cerr << "Config::LoadFromFile: Setting string for key " << key << std::endl;  
                            Set(key, value_str);
                        }
                    }
                }
            } else {
                 std::cerr << "Config::LoadFromFile: Skipped malformed line " << line_num << ": " << line << std::endl;  
            }
        }
        configFile.close();
        std::cerr << "Config::LoadFromFile: Finished loading. Total keys in map: " << values.size() << std::endl;  
        return true;
    }
    
    
    bool Config::SaveToFile() {
        std::cerr << "Config::SaveToFile: Attempting to save to " << default_config_filepath_ << std::endl;  
        std::cerr << "Config::SaveToFile: Number of keys to save: " << values.size() << std::endl;  
        for (const auto& pair : values) {
            std::cerr << "Config::SaveToFile: Saving key='" << pair.first << "'" << std::endl; 
        }

        std::ofstream configFile(default_config_filepath_); 
        if (!configFile.is_open()) {
            std::cerr << "Config::SaveToFile: FAILED to open config file for writing: " << default_config_filepath_ << std::endl;  
            return false;
        }
        std::cerr << "Config::SaveToFile: Successfully opened for writing." << std::endl; 

        for (const auto& pair : values) {
            configFile << pair.first << "=";
            if (pair.first == PAUSE_COMBO_KEY) {
                try {
                    const auto& combo = std::any_cast<const std::vector<uint8_t>&>(pair.second);
                    for (size_t i = 0; i < combo.size(); ++i) {
                        configFile << static_cast<int>(combo[i]) << (i == combo.size() - 1 ? "" : " ");
                    }
                } catch (const std::bad_any_cast& e) {
                    configFile << "ErrorSerializingPauseCombo"; 
                    std::cerr << "Config::SaveToFile: Error serializing pause combo: " << e.what() << std::endl;  
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
        std::cerr << "Config::SaveToFile: Finished saving." << std::endl;  
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