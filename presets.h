#pragma once
#include <string>
#include <vector>

struct PresetScript {
    std::string name;
    std::string description;
    const char* code;
};

const std::vector<PresetScript>& GetBuiltInScripts();