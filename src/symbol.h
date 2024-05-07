#pragma once
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
    VAR
};

// 符号对应的值
struct Value
{
    TYPE type;
    int val;
    Value() = default;
    Value(TYPE type_, int val_) : type(type_), val(val_) {}
};

// 符号表
// 支持嵌套 --STL容器
// 跨作用域查询 --随机访问
// 综合判断 --使用vector
class SymbolList
{
public:
    vector<unordered_map<string, Value>> symbol_list_array;
    int index;
    int cur_index;
    SymbolList()
    {
        index = 0;
        cur_index = 0;
    }
    void newMap();
    void deleteMap();
    void addSymbol(string name, Value value);
    Value getSymbol(string name);
};