#include "utils/Config.h"
#include "utils/Logger.h" 
#include <fstream>
#include <iostream> 
#include <sstream>
#include <vector>
#include <algorithm>

namespace LocalTether::Utils {

     const std::string& Config::GetPauseComboKey() { 
        static const std::string key = "input.pause_combo_vk";
        return key;
    }

    const std::string& Config::GetDefaultConfigFilePath() { 
        static const std::string path = "localtether_config.cfg";
        return path;
    }

    Config& Config::GetInstance() {
        static Config instance;
        return instance;
    }

    Config::Config() {
      Logger::GetInstance().Info("Config: Constructor called. Attempting to load from file.");
      if (!LoadFromFile()) {
          Logger::GetInstance().Error("Config: LoadFromFile failed in constructor.");
      } else {
          Logger::GetInstance().Info("Config: LoadFromFile succeeded in constructor.");
      }
    }

    bool Config::LoadFromFile() {
        const std::string& configFilePath = GetDefaultConfigFilePath(); 
        Logger::GetInstance().Info("Config::LoadFromFile: Attempting to open: " + configFilePath);
        std::ifstream configFile(configFilePath);
        if (!configFile.is_open()) {
            Logger::GetInstance().Error("Config::LoadFromFile: FAILED to open config file: " + configFilePath);
            return false;
        }
        Logger::GetInstance().Info("Config::LoadFromFile: Successfully opened config file: " + configFilePath);

        values.clear();

        std::string line;
        int line_num = 0;
        const std::string& pauseKey = GetPauseComboKey(); 

        while (std::getline(configFile, line)) {
            line_num++;
            Logger::GetInstance().Debug("Config::LoadFromFile: Processing line " + std::to_string(line_num) + ": " + line);
            std::stringstream ss_line(line);
            std::string key_str; 
            std::string value_str;

            if (std::getline(ss_line, key_str, '=') && std::getline(ss_line, value_str)) {
                key_str.erase(0, key_str.find_first_not_of(" \t\n\r"));
                key_str.erase(key_str.find_last_not_of(" \t\n\r") + 1);
                value_str.erase(0, value_str.find_first_not_of(" \t\n\r"));
                value_str.erase(value_str.find_last_not_of(" \t\n\r") + 1);
                Logger::GetInstance().Debug("Config::LoadFromFile: Parsed key='" + key_str + "', value_str='" + value_str + "'");

                if (key_str == pauseKey) { 
                    std::vector<uint8_t> combo;
                    std::stringstream ss_value(value_str);
                    int vk_code_int;
                    std::string parsed_combo_log = "Parsed combo for " + key_str + ": ";
                    while (ss_value >> vk_code_int) {
                        parsed_combo_log += std::to_string(vk_code_int) + " ";
                        if (vk_code_int > 0 && vk_code_int < 256) {
                            combo.push_back(static_cast<uint8_t>(vk_code_int));
                        } else {
                            Logger::GetInstance().Warning("Config::LoadFromFile: Invalid VK code " + std::to_string(vk_code_int) + " for " + pauseKey);
                        }
                    }
                    if (ss_value.fail() && !ss_value.eof()) {
                        Logger::GetInstance().Warning("Config::LoadFromFile: Error parsing value for " + pauseKey + ". Remainder: '" + ss_value.str().substr(ss_value.tellg()) + "'");
                    }
                    Logger::GetInstance().Debug("Config::LoadFromFile: " + parsed_combo_log + "Resulting combo size: " + std::to_string(combo.size()));
                    Set(key_str, combo);
                } else {
                    if (value_str == "true" || value_str == "false") {
                        Logger::GetInstance().Debug("Config::LoadFromFile: Setting bool for key " + key_str);
                        Set(key_str, value_str == "true");
                    } else {
                        bool set_as_specific_type = false;
                        try {
                            size_t processed_chars_int = 0;
                            int int_val = std::stoi(value_str, &processed_chars_int);
                            if (processed_chars_int == value_str.length()) {
                                Logger::GetInstance().Debug("Config::LoadFromFile: Setting int for key " + key_str);
                                Set(key_str, int_val);
                                set_as_specific_type = true;
                            }
                        } catch (const std::invalid_argument&) {
                        } catch (const std::out_of_range&) {}

                        if (!set_as_specific_type) {
                            try {
                                size_t processed_chars_float = 0;
                                float float_val = std::stof(value_str, &processed_chars_float);
                                 if (processed_chars_float == value_str.length()) {
                                    Logger::GetInstance().Debug("Config::LoadFromFile: Setting float for key " + key_str);
                                    Set(key_str, float_val);
                                    set_as_specific_type = true;
                                }
                            } catch (const std::invalid_argument&) {
                            } catch (const std::out_of_range&) {}
                        }

                        if (!set_as_specific_type) {
                            Logger::GetInstance().Debug("Config::LoadFromFile: Setting string for key " + key_str);
                            Set(key_str, value_str);
                        }
                    }
                }
            } else {
                 Logger::GetInstance().Warning("Config::LoadFromFile: Skipped malformed line " + std::to_string(line_num) + ": " + line);
            }
        }
        configFile.close();
        Logger::GetInstance().Info("Config::LoadFromFile: Finished loading. Total keys in map: " + std::to_string(values.size()));
        return true;
    }


    bool Config::SaveToFile() {
        const std::string& configFilePath = GetDefaultConfigFilePath(); 
        Logger::GetInstance().Info("Config::SaveToFile: Attempting to save to " + configFilePath);
        Logger::GetInstance().Debug("Config::SaveToFile: Number of keys to save: " + std::to_string(values.size()));
        for (const auto& pair : values) {
            Logger::GetInstance().Debug("Config::SaveToFile: Preparing to save key='" + pair.first + "' with typeid: " + pair.second.type().name());
        }

        std::ofstream configFile(configFilePath);
        if (!configFile.is_open()) {
            Logger::GetInstance().Error("Config::SaveToFile: FAILED to open config file for writing: " + configFilePath);
            return false;
        }
        Logger::GetInstance().Info("Config::SaveToFile: Successfully opened for writing: " + configFilePath);
        const std::string& pauseKey = GetPauseComboKey(); 

        for (const auto& pair : values) {
            configFile << pair.first << "=";
            if (pair.first == pauseKey) { 
                try {
                    const auto& combo = std::any_cast<const std::vector<uint8_t>&>(pair.second);
                    std::string combo_str_log = "Saving combo for " + pair.first + ": ";
                    for (size_t i = 0; i < combo.size(); ++i) {
                        configFile << static_cast<int>(combo[i]) << (i == combo.size() - 1 ? "" : " ");
                        combo_str_log += std::to_string(static_cast<int>(combo[i])) + " ";
                    }
                    Logger::GetInstance().Debug("Config::SaveToFile: " + combo_str_log);
                } catch (const std::bad_any_cast& e) {
                    configFile << ""; 
                    Logger::GetInstance().Error("Config::SaveToFile: Error serializing pause combo for key '" + pair.first + "': " + e.what() + ". Saved as empty.");
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
                Logger::GetInstance().Warning("Config::SaveToFile: UnsupportedType for key '" + pair.first + "'. Type: " + pair.second.type().name());
                configFile << "UnsupportedType";
            }
            configFile << std::endl;
        }
        configFile.close();
        Logger::GetInstance().Info("Config::SaveToFile: Finished saving.");
        return true;
    }

    template<typename T>
    void Config::Set(const std::string& key, const T& value) {
        values[key] = value;
        Logger::GetInstance().Debug("Config::Set: Key '" + key + "' set with type " + typeid(T).name() + ". Value count in map: " + std::to_string(values.size()));
    }

    template<typename T>
    T Config::Get(const std::string& key, const T& defaultValue) {
        auto it = values.find(key);
        if (it != values.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast& e) {
                Logger::GetInstance().Warning("Config::Get: bad_any_cast for key '" + key +
                          "'. Requested type: " + typeid(T).name() +
                          ", Actual stored type: " + it->second.type().name() +
                          ". Exception: " + e.what());

                if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                    if (it->second.type() == typeid(std::string)) {
                        Logger::GetInstance().Info("Config::Get: Key '" + key + "' (requested as vector<uint8_t>) " +
                                  "is stored as std::string. Attempting to parse.");
                        const auto& value_str = std::any_cast<const std::string&>(it->second);
                        std::vector<uint8_t> parsed_combo;
                        std::stringstream ss_value(value_str);
                        int vk_code_int;
                        while (ss_value >> vk_code_int) {
                            if (vk_code_int > 0 && vk_code_int < 255) { // VK codes are 1-254
                                parsed_combo.push_back(static_cast<uint8_t>(vk_code_int));
                            } else {
                                Logger::GetInstance().Warning("Config::Get: Invalid value " + std::to_string(vk_code_int) + " while parsing string for key '" + key + "'");
                            }
                        }
                        if (!ss_value.fail() || ss_value.eof()) {
                             Logger::GetInstance().Debug("Config::Get: Successfully parsed string to vector<uint8_t> for key '" + key + "'. Size: " + std::to_string(parsed_combo.size()));
                            return parsed_combo;
                        } else {
                            Logger::GetInstance().Warning("Config::Get: Failed to fully parse string '" + value_str + "' to vector<uint8_t> for key '" + key + "'.");
                        }
                    }
                }
                else if constexpr (std::is_same_v<T, bool>) {
                    if (it->second.type() == typeid(std::string)) {
                        const auto& temp_str = std::any_cast<const std::string&>(it->second);
                        Logger::GetInstance().Debug("Config::Get: Converting string to bool for key " + key + ": " + temp_str);
                        return static_cast<T>(temp_str == "true" || temp_str == "1");
                    }
                }
                Logger::GetInstance().Warning("Config::Get: Unhandled type mismatch or conversion failed for key '" + key + "'. Returning default value.");
            }
        } else {
            
            if (key.empty()) {
                 Logger::GetInstance().Error("Config::Get: Called with an EMPTY key. This is likely an issue with how Config::GetPauseComboKey() or similar is being used or initialized. Returning default value.");
            } else {
                 Logger::GetInstance().Debug("Config::Get: Key '" + key + "' not found in config. Returning default value.");
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