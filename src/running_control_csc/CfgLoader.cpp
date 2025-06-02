
#include "running_control_csc/CfgLoader.hpp"
#include <fstream>

bool CfgLoader::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (getline(file, line)) {
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            config[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }
    return true;
}

std::string CfgLoader::get(const std::string& key) const {
    auto it = config.find(key);
    return (it != config.end()) ? it->second : "";
}