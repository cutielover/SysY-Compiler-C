
#include "symbol.h"

void SymbolList::newMap()
{
    symbol_list_array.push_back(unordered_map<string, Value>());
}

void SymbolList::deleteMap()
{
    symbol_list_array.pop_back();
}

void SymbolList::addSymbol(string name, Value value)
{
    symbol_list_array.back()[name] = value;
}

Value SymbolList::getSymbol(string name)
{
    for (auto i = symbol_list_array.rbegin(); i != symbol_list_array.rend(); i++)
    {
        if (i->find(name) != i->end())
        {
            return i->at(name);
        }
    }
    return Value();
}
