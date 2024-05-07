#include <unordered_map>
#include "symbol.h"

void SymbolList::newMap()
{
    symbol_list_array.push_back(unordered_map<string, Value>());
    index++;
}

void SymbolList::deleteMap()
{
    symbol_list_array.pop_back();
    index--;
}

void SymbolList::addSymbol(string name, Value value)
{
    symbol_list_array.back()[name] = value;
}

Value SymbolList::getSymbol(string name)
{
    cur_index = index;
    for (auto i = symbol_list_array.rbegin(); i != symbol_list_array.rend(); i++)
    {
        if (i->find(name) != i->end())
        {
            return i->at(name);
        }
        cur_index--;
    }
    return Value();
}