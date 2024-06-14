#pragma once

#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <unordered_set>

using namespace std;

// unordered_map<string, int> var_val;
// unordered_map<string, TYPE> var_type;

enum TYPE
{
    CONSTANT,
    VAR,
    FUNC,
    ARRAY,
    POINTER
};

// 符号对应的值
// 对FUNC类型，void : val = 0 ; int : val = 1
struct Value
{
    TYPE type;
    int val;
    int name_index;
    Value() = default;
    Value(TYPE type_, int val_, int name_index_) : type(type_), val(val_), name_index(name_index_) {}
};

// 符号表
// 支持嵌套 --STL容器
// 跨作用域查询 --随机访问
// 综合判断 --使用vector
class SymbolList
{
public:
    vector<unordered_map<string, Value>> symbol_list_array;
    // @index: 符号表数量
    int index;
    // @cur_index:
    SymbolList()
    {
        index = 0;
    }
    void newMap();
    void deleteMap();
    void addSymbol(string name, Value value);
    Value getSymbol(string name);
};