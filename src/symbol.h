#pragma once
#include <unordered_map>

extern std::unordered_map<std::string, int> var_val;

enum TYPE
{
    CONSTANT,
    VAR
};
extern std::unordered_map<std::string, TYPE> var_type;