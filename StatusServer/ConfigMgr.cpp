//
// Created by mpt on 2026/6/30.
//

#include "ConfigMgr.h"

#include <boost/filesystem.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <map>
#include <string>
#include <utility>

ConfigMgr::ConfigMgr() {
    const boost::filesystem::path current_path =
            boost::filesystem::current_path();
    const boost::filesystem::path config_path = current_path / "config.ini";

    boost::property_tree::ptree pt;
    boost::property_tree::read_ini(config_path.string(), pt);

    for (const auto& section_pair : pt) {
        const std::string& section_name = section_pair.first;
        const boost::property_tree::ptree& section_tree = section_pair.second;

        std::map<std::string, std::string> section_config;
        for (const auto& key_value_pair : section_tree) {
            const std::string& key = key_value_pair.first;
            const std::string& value =
                    key_value_pair.second.get_value<std::string>();
            section_config[key] = value;
        }

        SectionInfo section_info;
        section_info._section_datas = std::move(section_config);
        _config_map[section_name] = std::move(section_info);
    }
}