
#ifndef CFGLOADER_HPP
#define CFGLOADER_HPP

#include <string>
#include <map>

class CfgLoader {
    std::map<std::string, std::string> config;

public:
    bool load(const std::string& path);
    std::string get(const std::string& key) const;
};

#endif