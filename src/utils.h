#pragma once

#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <unordered_set>

using namespace std;

enum TYPE
{
    CONSTANT,
    VAR,
    FUNC,
    ARRAY,
    POINTER
};

// 符号对应的值
// 对CONST类型，val : 常量的值
// 对VAR类型，val无实际意义
// 对FUNC类型，void->val = 0 ; int->val = 1
// 对ARRAY类型，val : 数组长度
// 对POINTER类型，val : []的数量
struct Value
{
    // @type: 符号的类型
    TYPE type;
    int val;
    // @name_index: 符号的命名序号，这里用的是符号所在的基本块号
    int name_index;
    Value() = default;
    Value(TYPE type_, int val_, int name_index_) : type(type_), val(val_), name_index(name_index_) {}
};

/**
 * 符号表
 * 需要实现功能：
 * 1.支持嵌套 --STL容器
 * 2.跨作用域查询 --随机访问
 * 综合考虑使用vector实现
 */
class SymbolList
{
public:
    // @symbol_list_array: 符号表构成的vector 前后符号表有着嵌套关系
    vector<unordered_map<string, Value>> symbol_list_array;

    SymbolList() = default;

    /**
     * @brief Create a new symbol list
     */
    void newMap();

    /**
     * @brief Delete a disabled symbol list
     */
    void deleteMap();

    /**
     * @brief Add a symbol to the current symbol list
     * @param name The original name of the symbol
     * @param value Related information of the symbol
     */
    void addSymbol(string name, Value value);

    /**
     * @brief Finds the corresponding symbol by name
     * @param name The original name of the symbol
     * @return The value of the symbol
     */
    Value getSymbol(string name);
};

struct Block_Unit
{
    // @index: 块编号，从0开始计数
    int index;
    // @parent_index: 母块编号，体现块之间的嵌套关系
    int parent_index;
    // @end: 记录块是否终止
    bool end;
    Block_Unit()
    {
        index = 0;
        parent_index = 0;
        end = false;
    }
    Block_Unit(int index_, int parent_index_) : index(index_), parent_index(parent_index_), end(false) {}
};

/**
 * 基本块处理器
 * 对每个基本块进行编号记录并管理
 * 建立基本块之间的父子关系
 * 维护基本块的完结情况
 */
class BlockHandler
{
public:
    // @block_cnt: 记录基本块总数，从0开始，用于编号
    int block_cnt;
    // @block_list: 保存当前所有基本块
    vector<Block_Unit> block_list;
    // @block_now: 当前所在基本块
    Block_Unit block_now;

    BlockHandler()
    {
        block_cnt = 0;
        block_now = Block_Unit();
        block_list.push_back(block_now);
    }

    bool is_end()
    {
        return block_now.end;
    }

    void set_end()
    {
        block_now.end = true;
    }

    void set_not_end()
    {
        block_now.end = false;
    }

    void addBlock()
    {
        block_cnt++;
        Block_Unit new_block = Block_Unit(block_cnt, block_now.index);
        block_list.push_back(new_block);
        block_now = new_block;
    }

    void leaveBlock()
    {
        bool end_info = block_now.end;
        block_now = block_list[block_now.parent_index];
        block_now.end = end_info;
    }
};