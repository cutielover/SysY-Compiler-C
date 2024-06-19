#pragma once

#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <deque>
#include <unordered_map>
#include "assert.h"
#include "symbol.h"
using namespace std;

extern string koopa_str;
extern int reg_cnt;
extern int if_cnt;

// 警惕使用全局变量
static int logical;

// lv7 while
static int loop_cnt;
static int loop_dep;
static unordered_map<int, int> find_loop;

static bool if_end = true;

// lv8 function
static bool param_block = false;
static vector<pair<string, int>> function_params; // 记录函数的形参，用于在函数体内部延迟加入符号表
                                                  // lv9 update: 由于新加入了数组参数，加入一个标识，0=int;size=ptr
// lv8 global var
static bool is_global = true;

extern SymbolList symbol_list;
extern BlockHandler block_handler;

static void initArray(std::string name_, std::string *ptr, const std::vector<int> &len)
{
    int n = len[0];
    if (len.size() == 1)
    {
        for (int i = 0; i < n; ++i)
        {
            string tmp = "%" + to_string(reg_cnt);
            reg_cnt++;
            koopa_str += "  " + tmp + " = getelemptr " + name_ + ", " + to_string(i) + "\n";
            koopa_str += "  store " + ptr[i] + ", " + tmp + "\n";
        }
    }
    else
    {
        vector<int> sublen(len.begin() + 1, len.end());
        int width = 1;
        for (auto l : sublen)
            width *= l;
        for (int i = 0; i < n; ++i)
        {
            string tmp = "%" + to_string(reg_cnt);
            reg_cnt++;
            koopa_str += "  " + tmp + " = getelemptr " + name_ + ", " + to_string(i) + "\n";
            initArray(tmp, ptr + i * width, sublen);
        }
    }
}

static string getInitList(std::string *ptr, const std::vector<int> &len)
{
    std::string ret = "{";
    if (len.size() == 1)
    {
        int n = len[0];
        ret += ptr[0];
        for (int i = 1; i < n; ++i)
        {
            ret += ", " + ptr[i];
        }
    }
    else
    {
        int n = len[0], width = 1;
        std::vector<int> sublen(len.begin() + 1, len.end());
        for (auto iter = len.end() - 1; iter != len.begin(); --iter)
            width *= *iter;
        ret += getInitList(ptr, sublen);
        for (int i = 1; i < n; ++i)
        {
            ret += ", " + getInitList(ptr + width * i, sublen);
        }
    }
    ret += "}";
    return ret;
}

// lv4+
// 所有 AST 的基类
class BaseAST
{
public:
    virtual ~BaseAST() = default;

    // 用于优化，对于能直接计算出值的表达式，直接返回值，减少寄存器浪费
    // bool: true-能计算 false-不能
    // int: 能计算出的值；对于不能计算出的值，返回-1
    virtual pair<bool, int> Koopa() const { return make_pair(false, -1); }
    virtual string name() const { return "oops"; }
    // 用于计算初始化的值
    // 必定能计算出来，否则是程序的问题
    virtual int calc() const { return 0; }
    virtual bool is_list() const { return false; }
};

// lv4+
// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST
{
public:
    // 用智能指针管理对象
    unique_ptr<deque<unique_ptr<BaseAST>>> DefList;
    int func_num;

    pair<bool, int> Koopa() const override
    {
        // 全局符号表
        symbol_list.newMap();

        // block_end.push_back(false);
        // 库函数声明
        koopa_str += "decl @getint(): i32\ndecl @getch() : i32\ndecl @getarray(*i32) : i32\n";
        koopa_str += "decl @putint(i32)\ndecl @putch(i32)\ndecl @putarray(i32, *i32)\n";
        koopa_str += "decl @starttime()\ndecl @stoptime()\n\n\n";

        // 库函数加入符号表
        symbol_list.addSymbol("getint", Value(FUNC, 1, 0));
        symbol_list.addSymbol("getch", Value(FUNC, 1, 0));
        symbol_list.addSymbol("getarray", Value(FUNC, 1, 0));
        symbol_list.addSymbol("putint", Value(FUNC, 0, 0));
        symbol_list.addSymbol("putch", Value(FUNC, 0, 0));
        symbol_list.addSymbol("putarray", Value(FUNC, 0, 0));
        symbol_list.addSymbol("starttime", Value(FUNC, 0, 0));
        symbol_list.addSymbol("stoptime", Value(FUNC, 0, 0));

        int n = (*DefList).size();

        // 全局变量
        for (int i = 0; i < n - func_num; i++)
        {
            if (n == func_num)
                break;
            ((*DefList)[i])->Koopa();
        }

        koopa_str += "\n";

        // 函数: in inverted order
        for (int i = n - 1; i >= n - func_num; i--)
        {
            ((*DefList)[i])->Koopa();
        }

        symbol_list.deleteMap();

        return make_pair(false, -1);
    }
};

// lv4+
// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST
{
public:
    string type;
    string ident;
    unique_ptr<BaseAST> block;

    pair<bool, int> Koopa() const override
    {
        is_global = false;
        int val_func = 0;
        if (type == "int")
        {
            val_func = 1;
        }
        Value global_func = Value(FUNC, val_func, 0);
        symbol_list.addSymbol(ident, global_func);

        koopa_str += "fun ";
        koopa_str += "@";
        koopa_str += ident;
        koopa_str += "()";

        if (type == "int")
        {
            koopa_str += ": i32 ";
        }
        else if (type == "void")
        {
            koopa_str += " ";
        }

        // func_type->Koopa();

        reg_cnt = 0;

        koopa_str += "{\n";
        koopa_str += "\%entry:\n";

        block->Koopa();

        if (!block_handler.is_end())
        {
            if (type == "int")
                koopa_str += "  ret 0\n";
            else
            {
                koopa_str += "  ret\n";
            }
        }

        koopa_str += "}\n\n";
        return make_pair(false, -1);
    }
};

// lv8
// FuncDef with Params
// FuncDef ::= FuncType IDENT '(' FuncFParams ')' Block
// @
class FuncDefWithParamsAST : public BaseAST
{
public:
    string type;
    string ident;
    unique_ptr<BaseAST> funcfparams;
    unique_ptr<BaseAST> block;

    pair<bool, int> Koopa() const override
    {
        is_global = false;
        int val_func = 0;
        if (type == "int")
        {
            val_func = 1;
        }
        Value global_func = Value(FUNC, val_func, 0);
        symbol_list.addSymbol(ident, global_func);

        koopa_str += "fun ";
        koopa_str += "@";
        koopa_str += ident;

        koopa_str += "(";

        // 不在函数体内，声明参数
        param_block = false;
        funcfparams->Koopa();

        koopa_str += ")";

        if (type == "int")
        {
            koopa_str += ": i32 ";
        }
        else if (type == "void")
        {
            koopa_str += " ";
        }

        reg_cnt = 0;
        koopa_str += "{\n";
        koopa_str += "\%entry:\n";

        // 在函数体内，把参数加载出来
        param_block = true;
        funcfparams->Koopa();

        block->Koopa();

        if (!block_handler.is_end())
        {
            if (type == "int")
                koopa_str += "  ret 0\n";
            else
            {
                koopa_str += "  ret\n";
            }
        }

        koopa_str += "}\n\n";
        return make_pair(false, -1);
    }
};

// lv8
// FuncFParams
class FuncFParamsAST : public BaseAST
{
public:
    vector<unique_ptr<BaseAST>> ParamList;

    pair<bool, int> Koopa() const override
    {
        bool params = false;
        for (auto &i : ParamList)
        {
            if (params && !param_block)
            {
                koopa_str += ", ";
            }
            i->Koopa();
            params = true;
        }
        return make_pair(false, -1);
    }
};

// lv9 update
// FuncFParam
// INT IDENT
class FuncFParamAST : public BaseAST
{
public:
    string type;
    string name_;
    vector<unique_ptr<BaseAST>> array_size_list;

    pair<bool, int> Koopa() const override
    {
        // 声明参数
        if (!param_block)
        {
            if (type == "int")
            {
                string param_tag = "@" + name_ + "_" + to_string(block_handler.block_cnt + 1) + "_param";
                koopa_str += param_tag + ": i32";
            }
            else if (type == "array")
            {
                string param_tag = "@" + name_ + "_" + to_string(block_handler.block_cnt + 1) + "_param";
                koopa_str += param_tag + ": ";

                string type_tag = "i32";
                int len = array_size_list.size();
                for (int i = len - 1; i >= 0; i--)
                {
                    type_tag = "[" + type_tag + ", " + to_string(array_size_list[i]->calc()) + "]";
                }
                koopa_str += "*" + type_tag;
            }
        }
        // 函数体内 为参数分配内存空间
        else
        {
            if (type == "int")
            {
                string param_tag = "@" + name_ + "_" + to_string(block_handler.block_cnt + 1) + "_param";
                string use_tag = "@" + name_ + "_" + to_string(block_handler.block_cnt + 1);
                koopa_str += "  " + use_tag + " = alloc i32\n";
                koopa_str += "  store " + param_tag + ", " + use_tag + "\n";
                function_params.emplace_back(make_pair(name_, 0));
            }
            else if (type == "array")
            {
                string type_tag = "i32";
                int len = array_size_list.size();
                for (int i = len - 1; i >= 0; i--)
                {
                    type_tag = "[" + type_tag + ", " + to_string(array_size_list[i]->calc()) + "]";
                }
                string param_tag = "@" + name_ + "_" + to_string(block_handler.block_cnt + 1) + "_param";
                string use_tag = "@" + name_ + "_" + to_string(block_handler.block_cnt + 1);

                koopa_str += "  " + use_tag + " = alloc *" + type_tag + "\n";
                koopa_str += "  store " + param_tag + ", " + use_tag + "\n";

                int pointer_size = len + 1;
                function_params.emplace_back(make_pair(name_, pointer_size));
            }
        }
        return make_pair(false, -1);
    }

    string name() const override
    {
        return name_;
    }
};

// lv9 ?
// lv8
// FuncRParams
class FuncRParamsAST : public BaseAST
{
public:
    vector<unique_ptr<BaseAST>> ParamList;

    pair<bool, int> Koopa() const override
    {
        return make_pair(false, -1);
    }
    vector<pair<bool, int>> get_params()
    {
        vector<pair<bool, int>> rparams;
        for (auto &i : ParamList)
        {
            auto res = i->Koopa();
            if (res.first)
            {
                rparams.push_back(res);
            }
            else
            {
                rparams.push_back(make_pair(false, reg_cnt - 1));
            }
        }
        return rparams;
    }
};

// lv9 update
// lv4+
// Block lv4
class BlockAST : public BaseAST
{
public:
    vector<unique_ptr<BaseAST>> blockItemList;

    pair<bool, int> Koopa() const override
    {
        symbol_list.newMap();

        block_handler.addBlock();

        // 参数加入符号表
        // lv9 update: 数组指针
        for (int i = 0; i < function_params.size(); i++)
        {
            if (function_params[i].second == 0)
            {
                Value param = Value(VAR, 0, block_handler.block_now.index);
                symbol_list.addSymbol(function_params[i].first, param);
            }
            else
            {
                // 指针的val是[]的数量，例：如果参数是a[]，则val记为1
                Value param = Value(POINTER, function_params[i].second, block_handler.block_now.index);
                symbol_list.addSymbol(function_params[i].first, param);
            }
        }

        function_params.clear();

        for (int i = 0; i < blockItemList.size(); i++)
        {
            if (block_handler.is_end())
            {
                break;
            }

            blockItemList[i]->Koopa();
        }

        // 子块完成后，归位block_last
        block_handler.leaveBlock();

        symbol_list.deleteMap();

        // 返回当前基本块的序号
        return make_pair(true, block_handler.block_cnt);
    }
};

// lv4+
// Stmt 语句 赋值|返回|无效表达式|if语句 LeVal "=" Exp ";" | "return" [Exp] ";" | [Exp] ";" | Block | If [Else]
class StmtAST : public BaseAST
{
public:
    int rule;
    unique_ptr<BaseAST> exp;
    unique_ptr<BaseAST> leval;
    unique_ptr<BaseAST> block;

    pair<bool, int> Koopa() const override
    {
        if (rule == 0) // 赋值
        {
            string val_name = leval->name();
            pair<bool, int> res = exp->Koopa();
            if (res.first)
            {
                koopa_str += "  store " + to_string(res.second) + ", " + val_name + "\n";
            }
            else
            {
                koopa_str += "  store %" + to_string(reg_cnt - 1) + ", " + val_name + "\n";
            }

            block_handler.set_not_end();
        }
        else if (rule == 1)
        {
            if (exp != nullptr)
            {
                pair<bool, int> res = exp->Koopa();
                if (res.first)
                {
                    koopa_str += "  ret " + to_string(res.second) + "\n";
                }
                else
                {
                    koopa_str += "  ret %" + to_string(reg_cnt - 1) + "\n";
                }
            }
            else
            { //?
                koopa_str += "  ret\n";
            }

            block_handler.set_end();
        }
        else if (rule == 2)
        {
            if (exp != nullptr)
                exp->Koopa();

            block_handler.set_not_end();
        }
        else if (rule == 3)
        {
            return block->Koopa();
        }
        return make_pair(false, -1);
    }
};

// lv6
// if_stmt ::= If ELSE else_stmt | If
class IfStmtAST : public BaseAST
{
public:
    unique_ptr<BaseAST> if_stmt;
    unique_ptr<BaseAST> else_stmt;

    pair<bool, int> Koopa() const override
    {
        if_cnt++;
        int now_if_cnt = if_cnt;
        // If
        if (else_stmt == nullptr)
        {
            if_end = true;
            if_stmt->Koopa();
            string end_tag = "%end_" + to_string(now_if_cnt);
            koopa_str += end_tag + ":\n";

            block_handler.set_not_end();
        }
        // If ELSE Stmt
        else
        {
            if_end = false;

            pair<bool, int> res_if = if_stmt->Koopa();

            bool if_stmt_end = res_if.second == 1 ? true : false;

            string end_tag = "%end_" + to_string(now_if_cnt);
            string else_tag = "%else_" + to_string(now_if_cnt);

            // else tag:
            koopa_str += else_tag + ":\n";

            else_stmt->Koopa();

            bool else_stmt_end = block_handler.is_end();

            end_tag = "%end_" + to_string(now_if_cnt);

            if (!else_stmt_end)
            {
                koopa_str += "  jump " + end_tag + "\n\n";
            }

            if (if_stmt_end && else_stmt_end)
            {
                block_handler.set_end();
            }
            else
            {
                block_handler.set_not_end();
                koopa_str += end_tag + ":\n";
            }
        }

        return make_pair(false, -1);
    }
};

// lv6
// If ::= IF (Exp) Stmt
// 不含else的if
class IfAST : public BaseAST
{
public:
    unique_ptr<BaseAST> exp;
    unique_ptr<BaseAST> stmt;

    pair<bool, int> Koopa() const override
    {
        pair<bool, int> res = exp->Koopa();
        int now_if_cnt = if_cnt;
        bool now_if_end = if_end;

        string then_tag = "%then_" + to_string(now_if_cnt);
        string else_tag = "%else_" + to_string(now_if_cnt);
        string end_tag = "%end_" + to_string(now_if_cnt);
        // 是否返回具体值
        if (res.first)
        {
            koopa_str += "  br " + to_string(res.second) + ", " + then_tag;
        }
        else
        {
            koopa_str += "  br %" + to_string(reg_cnt - 1) + ", " + then_tag;
        }
        // 没有else的情况
        if (now_if_end)
        {
            koopa_str += ", " + end_tag + "\n";
        }
        // else
        else
        {
            koopa_str += ", " + else_tag + "\n";
        }

        koopa_str += "\n";
        koopa_str += then_tag + ":\n";

        stmt->Koopa();

        bool end_now = block_handler.is_end();

        int is_end = 1;
        if (!end_now)
        {
            is_end = -1;
            koopa_str += "  jump " + end_tag + "\n\n";
        }
        // end of if branch
        // koopa_str += end_tag + ":\n";

        return make_pair(false, is_end);
    }
};

// lv7
// While ::= WHILE (Exp) Stmt
// While循环
class WhileAST : public BaseAST
{
public:
    unique_ptr<BaseAST> exp;
    unique_ptr<BaseAST> stmt;

    pair<bool, int> Koopa() const override
    {
        loop_cnt++;
        loop_dep++;
        find_loop[loop_dep] = loop_cnt;

        string entry_name = "%while_entry_" + to_string(loop_cnt);
        string body_name = "%while_body_" + to_string(loop_cnt);
        string end_name = "%while_end_" + to_string(loop_cnt);

        koopa_str += "  jump " + entry_name + "\n\n";
        koopa_str += entry_name + ":\n";

        pair<bool, int> res = exp->Koopa();
        if (res.first)
        {
            koopa_str += "  br " + to_string(res.second) + ", " + body_name + ", " + end_name + "\n\n";
        }
        else
        {
            koopa_str += "  br %" + to_string(reg_cnt - 1) + ", " + body_name + ", " + end_name + "\n\n";
        }

        koopa_str += body_name + ":\n";

        stmt->Koopa();

        if (!block_handler.is_end())
        {
            koopa_str += "  jump " + entry_name + "\n\n";
        }

        koopa_str += end_name + ":\n";

        loop_dep--;

        block_handler.set_not_end();

        return make_pair(false, -1);
    }
};

// lv7
// BREAK/CONTINUE
// break: rule -> 0
// continue: rule -> 1
class LoopJumpAST : public BaseAST
{
public:
    int rule;

    pair<bool, int> Koopa() const override
    {
        int tag = find_loop[loop_dep];
        // break: 跳转到%while_end
        if (rule == 0)
        {
            string end_name = "%while_end_" + to_string(tag);
            if (!block_handler.is_end())
                koopa_str += "  jump " + end_name + "\n\n";

            block_handler.set_end();
        }
        else
        {
            string entry_name = "%while_entry_" + to_string(tag);
            if (!block_handler.is_end())
                koopa_str += "  jump " + entry_name + "\n\n";

            block_handler.set_end();
        }

        return make_pair(false, -1);
    }
};

// lv4+
// Exp 表达式 lv3
class ExpAST : public BaseAST
{
public:
    unique_ptr<BaseAST> lorexp;

    pair<bool, int> Koopa() const override
    {
        return lorexp->Koopa();
    }
    int calc() const override
    {
        return lorexp->calc();
    }
};

// lv4+
// PrimaryExp 基本表达式，(Exp) | Number | LVal lv4
class PrimaryExpAST : public BaseAST
{
public:
    int rule;
    int number;
    unique_ptr<BaseAST> exp;
    unique_ptr<BaseAST> lval;

    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            return exp->Koopa();
        }
        else if (rule == 1)
        {
            return make_pair(true, number);
        }
        else
        {
            return lval->Koopa();
        }
    }
    int calc() const override
    {
        if (rule == 0)
        {
            return exp->calc();
        }
        else if (rule == 1)
        {
            return number;
        }
        else
        {
            return lval->calc();
        }
    }
};

// lv4+
// UnaryExp ::= PrimaryExp | UnaryOp[!-+] UnaryExp lv3
class UnaryExpAST : public BaseAST
{
public:
    int rule;
    string op;
    unique_ptr<BaseAST> primaryexp;
    unique_ptr<BaseAST> unaryexp;

    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            return primaryexp->Koopa();
        }
        else
        {
            pair<bool, int> res = unaryexp->Koopa();
            if (res.first)
            {
                if (op == "!")
                {

                    if (res.second == 0)
                        return make_pair(true, 1);
                    return make_pair(true, 0);

                    koopa_str += "  %" + to_string(reg_cnt) + " = eq " + to_string(res.second) + ", 0\n";
                    reg_cnt++;
                }
                else if (op == "-")
                {
                    return make_pair(true, -res.second);

                    koopa_str += "  %" + to_string(reg_cnt) + " = sub 0, " + to_string(res.second) + "\n";
                    reg_cnt++;
                }
                else if (op == "+")
                {
                    return res;
                }
            }
            else
            {
                if (op == "!")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = eq %" + to_string(reg_cnt - 1) + ", 0\n";
                    reg_cnt++;
                }
                else if (op == "-")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = sub 0, %" + to_string(reg_cnt - 1) + "\n";
                    reg_cnt++;
                }
            }
            return make_pair(false, -1);
        }
    }
    int calc() const override
    {
        if (rule == 0)
        {
            return primaryexp->calc();
        }
        else
        {
            if (op == "!")
            {
                return !unaryexp->calc();
            }
            else if (op == "-")
            {
                return -unaryexp->calc();
            }
            else
            {
                return unaryexp->calc();
            }
        }
    }
};

// LV8
// UnaryExp -> IDENT "(" [FuncRParams] ")"
class UnaryExpWithFuncAST : public BaseAST
{
public:
    string ident;
    unique_ptr<BaseAST> funcrparams;

    pair<bool, int> Koopa() const override
    {
        Value func = symbol_list.getSymbol(ident);
        if (func.type != TYPE::FUNC)
        {
            // assert(false); //??
        }
        vector<pair<bool, int>> rparams_now;
        if (funcrparams)
        {
            rparams_now = (dynamic_cast<FuncRParamsAST *>(funcrparams.get()))->get_params();
        }
        // int
        if (func.val == 1)
        {
            koopa_str += "  %" + to_string(reg_cnt) + " = call @" + ident;
            reg_cnt++;
        }
        // void
        else
        {
            koopa_str += "  call @" + ident;
        }
        koopa_str += "(";

        if (funcrparams)
        {
            for (int i = 0; i < rparams_now.size(); i++)
            {
                if (i != 0)
                {
                    koopa_str += ", ";
                }
                if (rparams_now[i].first)
                {
                    koopa_str += to_string(rparams_now[i].second);
                }
                else
                {
                    koopa_str += "%" + to_string(rparams_now[i].second);
                }
            }
        }

        koopa_str += ")\n";

        return make_pair(false, -1);
    }
};

// lv4+
// MulExp ::= UnaryExp | MulExp [*/%] UnaryExp lv3
class MulExpAST : public BaseAST
{
public:
    int rule;
    string op;
    unique_ptr<BaseAST> mulexp;
    unique_ptr<BaseAST> unaryexp;

    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            return unaryexp->Koopa();
        }
        else
        {
            pair<bool, int> res_l = mulexp->Koopa();
            int leftreg = reg_cnt - 1;
            pair<bool, int> res_r = unaryexp->Koopa();
            int rightreg = reg_cnt - 1;

            if (res_l.first && res_r.first)
            {
                if (op == "*")
                {
                    return make_pair(true, res_l.second * res_r.second);
                    ///////////////////////////////////////////////////
                    koopa_str += "  %" + to_string(reg_cnt) + " = mul " + to_string(res_l.second);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == "/")
                {
                    return make_pair(true, res_l.second / res_r.second);
                    /////////////////////////////////////////////////////
                    koopa_str += "  %" + to_string(reg_cnt) + " = div " + to_string(res_l.second);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == "%")
                {
                    return make_pair(true, res_l.second % res_r.second);
                    //////////////////////////////////////////////////////
                    koopa_str += "  %" + to_string(reg_cnt) + " = mod " + to_string(res_l.second);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
            }
            else if (res_l.first && !res_r.first)
            {
                if (op == "*")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = mul " + to_string(res_l.second);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == "/")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = div " + to_string(res_l.second);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == "%")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = mod " + to_string(res_l.second);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
            }
            else if (!res_l.first && res_r.first)
            {
                if (op == "*")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = mul %" + to_string(leftreg);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == "/")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = div %" + to_string(leftreg);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == "%")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = mod %" + to_string(leftreg);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
            }
            else
            {
                if (op == "*")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = mul %" + to_string(leftreg);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == "/")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = div %" + to_string(leftreg);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == "%")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = mod %" + to_string(leftreg);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
            }
            return make_pair(false, -1);
        }
    }
    int calc() const override
    {
        if (rule == 0)
        {
            return unaryexp->calc();
        }
        else
        {
            if (op == "*")
            {
                return mulexp->calc() * unaryexp->calc();
            }
            else if (op == "/")
            {
                return mulexp->calc() / unaryexp->calc();
            }
            else
            {
                return mulexp->calc() % unaryexp->calc();
            }
        }
    }
};

// lv4+
// AddExp ::= MulExp | AddExp [+-] MulExp lv3
class AddExpAST : public BaseAST
{
public:
    int rule;
    string op;
    unique_ptr<BaseAST> mulexp;
    unique_ptr<BaseAST> addexp;

    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            return mulexp->Koopa();
        }
        else
        {
            pair<bool, int> res_l = addexp->Koopa();
            int leftreg = reg_cnt - 1;
            pair<bool, int> res_r = mulexp->Koopa();
            int rightreg = reg_cnt - 1;

            if (res_l.first && res_r.first)
            {
                if (op == "+")
                {
                    return make_pair(true, res_l.second + res_r.second);
                    koopa_str += "  %" + to_string(reg_cnt) + " = add " + to_string(res_l.second);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == "-")
                {
                    return make_pair(true, res_l.second - res_r.second);
                    koopa_str += "  %" + to_string(reg_cnt) + " = sub " + to_string(res_l.second);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
            }
            else if (res_l.first && !res_r.first)
            {
                if (op == "+")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = add " + to_string(res_l.second);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == "-")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = sub " + to_string(res_l.second);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
            }
            else if (!res_l.first && res_r.first)
            {
                if (op == "+")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = add %" + to_string(leftreg);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == "-")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = sub %" + to_string(leftreg);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
            }
            else
            {
                if (op == "+")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = add %" + to_string(leftreg);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == "-")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = sub %" + to_string(leftreg);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
            }

            return make_pair(false, -1);
        }
    }
    int calc() const override
    {
        if (rule == 0)
        {
            return mulexp->calc();
        }
        else
        {
            if (op == "+")
            {
                return addexp->calc() + mulexp->calc();
            }
            else
            {
                return addexp->calc() - mulexp->calc();
            }
        }
    }
};

// lv4+
// RelExp ::= AddExp | RelExp [<>LEGE] AddExp lv3
class RelExpAST : public BaseAST
{
public:
    int rule;
    string op;
    unique_ptr<BaseAST> addexp;
    unique_ptr<BaseAST> relexp;

    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            return addexp->Koopa();
        }
        else
        {
            pair<bool, int> res_l = relexp->Koopa();
            int leftreg = reg_cnt - 1;
            pair<bool, int> res_r = addexp->Koopa();
            int rightreg = reg_cnt - 1;

            if (res_l.first && res_r.first)
            {
                if (op == "<")
                {
                    return make_pair(true, res_l.second < res_r.second);
                    ///////////////////////////////////////////////////
                    koopa_str += "  %" + to_string(reg_cnt) + " = lt " + to_string(res_l.second);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == ">")
                {
                    return make_pair(true, res_l.second > res_r.second);
                    /////////////////////////////////////////////////////
                    koopa_str += "  %" + to_string(reg_cnt) + " = gt " + to_string(res_l.second);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == "<=")
                {
                    return make_pair(true, res_l.second <= res_r.second);
                    //////////////////////////////////////////////////////
                    koopa_str += "  %" + to_string(reg_cnt) + " = le " + to_string(res_l.second);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == ">=")
                {
                    return make_pair(true, res_l.second >= res_r.second);
                    ///////////////////////////////////////////////////////
                    koopa_str += "  %" + to_string(reg_cnt) + " = ge " + to_string(res_l.second);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
            }
            else if (res_l.first && !res_r.first)
            {
                if (op == "<")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = lt " + to_string(res_l.second);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == ">")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = gt " + to_string(res_l.second);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == "<=")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = le " + to_string(res_l.second);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == ">=")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = ge " + to_string(res_l.second);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
            }
            else if (!res_l.first && res_r.first)
            {
                if (op == "<")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = lt %" + to_string(leftreg);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == ">")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = gt %" + to_string(leftreg);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == "<=")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = le %" + to_string(leftreg);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == ">=")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = ge %" + to_string(leftreg);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
            }
            else
            {
                if (op == "<")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = lt %" + to_string(leftreg);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == ">")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = gt %" + to_string(leftreg);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == "<=")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = le %" + to_string(leftreg);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == ">=")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = ge %" + to_string(leftreg);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
            }
            return make_pair(false, -1);
        }
    }
    int calc() const override
    {
        if (rule == 0)
        {
            return addexp->calc();
        }
        else
        {
            if (op == ">")
            {
                return relexp->calc() > addexp->calc();
            }
            else if (op == "<")
            {
                return relexp->calc() < addexp->calc();
            }
            else if (op == ">=")
            {
                return relexp->calc() >= addexp->calc();
            }
            else
            {
                return relexp->calc() <= addexp->calc();
            }
        }
    }
};

// lv4+
// EqExp ::= RelExp | EqExp EQ/NE RelExp lv3
class EqExpAST : public BaseAST
{
public:
    int rule;
    string op;
    unique_ptr<BaseAST> relexp;
    unique_ptr<BaseAST> eqexp;

    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            return relexp->Koopa();
        }
        else
        {
            pair<bool, int> res_l = eqexp->Koopa();
            int leftreg = reg_cnt - 1;
            pair<bool, int> res_r = relexp->Koopa();
            int rightreg = reg_cnt - 1;

            // 似乎可以优化
            if (res_l.first && res_r.first)
            {
                if (op == "==")
                {
                    return make_pair(true, res_l.second == res_r.second);
                    ////////////////////////////////////////////////////
                    koopa_str += "  %" + to_string(reg_cnt) + " = eq " + to_string(res_l.second);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == "!=")
                {
                    return make_pair(true, res_l.second != res_r.second);
                    /////////////////////////////////////////////////////
                    koopa_str += "  %" + to_string(reg_cnt) + " = ne " + to_string(res_l.second);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
            }
            else if (res_l.first && !res_r.first)
            {
                if (op == "==")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = eq " + to_string(res_l.second);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == "!=")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = ne " + to_string(res_l.second);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
            }
            else if (!res_l.first && res_r.first)
            {
                if (op == "==")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = eq %" + to_string(leftreg);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
                else if (op == "!=")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = ne %" + to_string(leftreg);
                    koopa_str += ", " + to_string(res_r.second) + "\n";
                    reg_cnt++;
                }
            }
            else
            {
                if (op == "==")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = eq %" + to_string(leftreg);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
                else if (op == "!=")
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = ne %" + to_string(leftreg);
                    koopa_str += ", %" + to_string(rightreg) + "\n";
                    reg_cnt++;
                }
            }
            return make_pair(false, -1);
        }
    }
    int calc() const override
    {
        if (rule == 0)
        {
            return relexp->calc();
        }
        else
        {
            if (op == "==")
            {
                return eqexp->calc() == relexp->calc();
            }
            else
            {
                return eqexp->calc() != relexp->calc();
            }
        }
    }
};

// lv6.2短路求值
// lv4+
// LAndExp ::= EqExp | LAndExp AND EqExp lv3
class LAndExpAST : public BaseAST
{
public:
    int rule;
    unique_ptr<BaseAST> eqexp;
    unique_ptr<BaseAST> landexp;

    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            return eqexp->Koopa();
        }
        else
        {
            logical++;
            int now_logic = logical;

            string then_tag = "%not_zero_" + to_string(now_logic);
            string end_tag = "%is_zero_" + to_string(now_logic);
            string res_tag = "@result_" + to_string(now_logic);

            // 规避错误：Instruction does not dominate all uses!
            koopa_str += "  " + res_tag + " = alloc i32\n";
            koopa_str += "  store 0, " + res_tag + "\n";

            pair<bool, int> res_l = landexp->Koopa();
            int leftreg = reg_cnt - 1;
            // 短路求值：lhs是0？
            // step1: 判断lhs是否是0
            // case1：lhs是一个数字
            if (res_l.first)
            {
                if (res_l.second == 0)
                {
                    return make_pair(true, 0);
                }
                // 如果不是0的话，直接忽略掉就好了？
                // 可以
                koopa_str += "  jump " + then_tag + "\n\n";
            }
            else
            {
                koopa_str += "  br %" + to_string(leftreg) + ", " + then_tag + ", " + end_tag + "\n\n";
            }
            koopa_str += then_tag + ":\n";

            pair<bool, int> res_r = eqexp->Koopa();
            int rightreg = reg_cnt - 1;

            // 这里，lhs非0
            if (res_r.first)
            {
                // res默认为0，所以如果是0，无需改变，直接jump即可
                if (res_r.second == 0)
                {
                    koopa_str += "  jump " + end_tag + "\n\n";
                }
                else
                { // 已知只有非0的时候会到达这里
                  // 把res记为1再jump
                    koopa_str += "  store 1, " + res_tag + "\n";
                    koopa_str += "  jump " + end_tag + "\n\n";
                }
            }
            else
            { // 结果在寄存器中
                // step1: 寄存器中的值是否为0？如果是0返回0，否则返回1
                koopa_str += "  %" + to_string(reg_cnt) + " = ne %" + to_string(rightreg) + ", 0\n";
                reg_cnt++;
                // step2: 把step1返回的结果存入res，然后jump
                koopa_str += "  store %" + to_string(reg_cnt - 1) + ", " + res_tag + "\n";
                koopa_str += "  jump " + end_tag + "\n\n";
            }

            koopa_str += end_tag + ":\n";

            koopa_str += "  %" + to_string(reg_cnt) + " = load " + res_tag + "\n";
            reg_cnt++;

            return make_pair(false, -1);
        }
    }
    int calc() const override
    {
        if (rule == 0)
        {
            return eqexp->calc();
        }
        else
        {
            return landexp->calc() && eqexp->calc();
        }
    }
};

// lv6.2短路求值
// lv4+
// LOrExp ::= LAndExp | LOrExp OR LAndExp lv3
class LOrExpAST : public BaseAST
{
public:
    int rule;
    unique_ptr<BaseAST> landexp;
    unique_ptr<BaseAST> lorexp;

    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            return landexp->Koopa();
        }
        else
        {
            logical++;
            int now_logic = logical;

            string then_tag = "%is_zero_" + to_string(now_logic);
            string end_tag = "%not_zero_" + to_string(now_logic);
            string res_tag = "@result_" + to_string(now_logic);

            // 规避错误：Instruction does not dominate all uses!
            koopa_str += "  " + res_tag + " = alloc i32\n";
            koopa_str += "  store 1, " + res_tag + "\n";

            pair<bool, int> res_l = lorexp->Koopa();
            int leftreg = reg_cnt - 1;

            // 以下过程近似于LAndExp的短路求值，注释略去

            if (res_l.first)
            {
                if (res_l.second != 0)
                {
                    return make_pair(true, 1);
                }
                koopa_str += "  jump " + then_tag + "\n\n";
            }
            else
            {
                koopa_str += "  br %" + to_string(leftreg) + ", " + end_tag + ", " + then_tag + "\n\n";
            }

            koopa_str += then_tag + ":\n";

            pair<bool, int> res_r = landexp->Koopa();
            int rightreg = reg_cnt - 1;

            if (res_r.first)
            {
                if (res_r.second != 0)
                {
                    koopa_str += "  jump " + end_tag + "\n\n";
                }
                else
                {
                    koopa_str += "  store 0, " + res_tag + "\n";
                    koopa_str += "  jump " + end_tag + "\n\n";
                }
            }
            else
            {
                koopa_str += "  %" + to_string(reg_cnt) + " = ne %" + to_string(rightreg) + ", 0\n";
                reg_cnt++;
                koopa_str += "  store %" + to_string(reg_cnt - 1) + ", " + res_tag + "\n";
                koopa_str += "  jump " + end_tag + "\n\n";
            }

            koopa_str += end_tag + ":\n";

            koopa_str += "  %" + to_string(reg_cnt) + " = load " + res_tag + "\n";
            reg_cnt++;

            return make_pair(false, -1);
        }
    }
    int calc() const override
    {
        if (rule == 0)
        {
            return landexp->calc();
        }
        else
        {
            return lorexp->calc() || landexp->calc();
        }
    }
};

// Decl 声明：常量/变量 ConstDecl | VarDecl lv4
class DeclAST : public BaseAST
{
public:
    int rule;
    unique_ptr<BaseAST> constdecl;
    unique_ptr<BaseAST> vardecl;

    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            return constdecl->Koopa();
        }
        else
        {
            return vardecl->Koopa();
        }
    }
};

// ConstDecl 声明常量，需要列表 CONST BType ConstDef {"," ConstDef} ";"; lv4
// 示例 const int a = 3,b = 5;
class ConstDeclAST : public BaseAST
{
public:
    vector<unique_ptr<BaseAST>> constDefList;

    pair<bool, int> Koopa() const override
    {
        for (auto &i : constDefList)
        {
            i->Koopa();
        }

        return make_pair(false, -1);
    }
};

// ConstDef 常量定义 IDENT "=" ConstInitVal lv4
// 示例 a = 3+5
class ConstDefAST : public BaseAST
{
public:
    string ident;
    unique_ptr<BaseAST> constinitval; // InitValAST

    pair<bool, int> Koopa() const override
    {
        int val = constinitval->calc();
        Value tmp(CONSTANT, val, block_handler.block_now.index);
        symbol_list.addSymbol(ident, tmp);

        return make_pair(false, -1);
    }
};

// lv9 done
class InitValWithListAST : public BaseAST
{
public:
    vector<unique_ptr<BaseAST>> init_val_list;

    pair<bool, int> Koopa() const override
    {
        return make_pair(false, -1);
    }
    bool is_list() const override
    {
        return true;
    }
    void getInitVal(string *ptr, const vector<int> &len)
    {
        int n = len.size();
        vector<int> width(n);
        width[n - 1] = len[n - 1];
        for (int i = n - 2; i >= 0; --i)
        {
            width[i] = width[i + 1] * len[i];
        }
        int i = 0;
        for (auto &init_val : init_val_list)
        {
            if (!init_val->is_list())
            {
                if (is_global)
                {
                    ptr[i++] = to_string(init_val->calc());
                }
                else
                {
                    pair<bool, int> res = init_val->Koopa();
                    if (res.first)
                    {
                        ptr[i++] = to_string(res.second);
                    }
                    else
                    {
                        ptr[i++] = "%" + to_string(reg_cnt - 1);
                    }
                }
            }
            else
            {
                int j = n - 1;
                if (i == 0)
                {
                    j = 1;
                }
                else
                {
                    j = n - 1;
                    for (; j >= 0; --j)
                    {
                        if (i % width[j] != 0)
                            break;
                    }
                    ++j;
                }
                dynamic_cast<InitValWithListAST *>(init_val.get())->getInitVal(ptr + i, vector<int>(len.begin() + j, len.end()));
                i += width[j];
            }
            if (i >= width[0])
                break;
        }
    }
};

// lv9 done
// 示例 const int a[10] = {1, 2, 3, 4, 5};
// 注意不能够像int常量一样直接存入符号表
// 必须体现在IR当中
class ConstDefArrayAST : public BaseAST
{
public:
    string ident;
    unique_ptr<BaseAST> constinitval; // InitValWithListAST 列表
    vector<unique_ptr<BaseAST>> array_size_list;
    pair<bool, int> Koopa() const override
    {
        vector<int> len;
        for (auto &i : array_size_list)
        {
            len.push_back(i->calc());
        }
        int array_size = array_size_list.size();
        Value const_array = Value(ARRAY, array_size, block_handler.block_now.index);
        // 数组放入符号表
        symbol_list.addSymbol(ident, const_array);

        // 处理初始化列表
        // 先把所有初始值置为0
        // 再根据constinitval的值来修改
        int tot_len = 1;
        for (auto i : len)
        {
            tot_len *= i;
        }
        string *init = new string[tot_len];
        for (int i = 0; i < tot_len; ++i)
        {
            init[i] = "0";
        }
        dynamic_cast<InitValWithListAST *>(constinitval.get())->getInitVal(init, len);
        string init_str = getInitList(init, len);

        // name tag
        string name_tag = "@" + ident + "_" + to_string(block_handler.block_now.index);

        // type tag
        string type_tag = "i32";
        int array_len = array_size_list.size();
        for (int i = array_len - 1; i >= 0; i--)
        {
            type_tag = "[" + type_tag + ", " + to_string(array_size_list[i]->calc()) + "]";
        }

        if (is_global) // 全局常量
        {
            koopa_str += "global " + name_tag + " = alloc ";
            koopa_str += type_tag + ", " + init_str + "\n";
        }
        else // 非全
        {
            koopa_str += "  " + name_tag + " = alloc " + type_tag + "\n";
            initArray(name_tag, init, len);
        }
        return make_pair(false, -1);
    }
};

// ConstInitVal 常量赋值 ConstExp lv4
class ConstInitValAST : public BaseAST
{
public:
    unique_ptr<BaseAST> constexp;

    pair<bool, int> Koopa() const override
    {
        return constexp->Koopa();
    }
};

// ConstExp 常量赋值表达式 Exp lv4
class ConstExpAST : public BaseAST
{
public:
    unique_ptr<BaseAST> exp;

    pair<bool, int> Koopa() const override
    {
        return exp->Koopa();
    }
};

// BlockItem Decl | Stmt lv4
class BlockItemAST : public BaseAST
{
public:
    int rule;
    unique_ptr<BaseAST> decl;
    unique_ptr<BaseAST> stmt;

    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
            return decl->Koopa();
        else
            return stmt->Koopa();
    }
};

// Lval （右值）变量名 IDENT lv4
class LValAST : public BaseAST
{
public:
    string ident;

    pair<bool, int> Koopa() const override
    {
        Value cur_var = symbol_list.getSymbol(ident);
        // 常量
        if (cur_var.type == CONSTANT)
        {
            return make_pair(true, cur_var.val);
        }
        // 变量
        else if (cur_var.type == VAR)
        {
            koopa_str += "  %" + to_string(reg_cnt) + " = load @" + ident + "_" + to_string(cur_var.name_index) + "\n";
            reg_cnt++;
            // !!!
            return make_pair(false, -1);
        }
        else if (cur_var.type == ARRAY)
        {
            string array_ident = "@" + ident + "_" + to_string(cur_var.name_index);
            koopa_str += "  %" + to_string(reg_cnt) + " = getelemptr " + array_ident + ", 0\n";
            reg_cnt++;

            return make_pair(false, -1);
        }
        else
        {
            // string array_ident = "@" + ident + "_" + to_string(cur_var.name_index);
            // koopa_str += "  %" + to_string(reg_cnt) + " = getptr " + array_ident + ", 0\n";
            // reg_cnt++;
            string array_ident = "@" + ident + "_" + to_string(cur_var.name_index);
            koopa_str += "  %" + to_string(reg_cnt) + " = load " + array_ident + "\n";
            reg_cnt++;
            return make_pair(false, -1);
        }
    }
    int calc() const override
    {
        Value cur_var = symbol_list.getSymbol(ident);
        return cur_var.val;
    }
};

// lv9 unfinished
// （右值）数组变量
// Lval lv9 IDENT ArraySizeList 示例 c = a[2][5]
class LValArrayAST : public BaseAST
{
public:
    string ident;
    vector<unique_ptr<BaseAST>> array_size_list;

    pair<bool, int> Koopa() const override
    {
        Value lval = symbol_list.getSymbol(ident);
        int len = array_size_list.size();
        string array_ident = "@" + ident + "_" + to_string(lval.name_index);
        // 数组
        if (lval.type == ARRAY)
        {
            if (lval.val == len)
            { // 读取一个数组项
                for (int i = 0; i < len; i++)
                {
                    string array_index;
                    int last_index = reg_cnt - 1;
                    pair<bool, int> res = array_size_list[i]->Koopa();
                    if (res.first)
                    {
                        array_index = to_string(res.second);
                    }
                    else
                    {
                        array_index = "%" + to_string(reg_cnt - 1);
                    }

                    if (i == 0)
                    {
                        koopa_str += "  %" + to_string(reg_cnt) + " = getelemptr ";
                        koopa_str += array_ident + ", " + array_index + "\n";
                    }
                    else
                    {
                        koopa_str += "  %" + to_string(reg_cnt) + " = getelemptr ";
                        koopa_str += "%" + to_string(last_index) + ", " + array_index + "\n";
                    }

                    reg_cnt++;
                }
                koopa_str += "  %" + to_string(reg_cnt) + " = load %" + to_string(reg_cnt - 1) + "\n";
                reg_cnt++;
                return make_pair(false, -1);
            }
            else // 读取一个部分解引用
            {
                for (int i = 0; i < len; i++)
                {
                    string array_index;
                    int last_index = reg_cnt - 1;
                    pair<bool, int> res = array_size_list[i]->Koopa();
                    if (res.first)
                    {
                        array_index = to_string(res.second);
                    }
                    else
                    {
                        array_index = "%" + to_string(reg_cnt - 1);
                    }

                    if (i == 0)
                    {
                        koopa_str += "  %" + to_string(reg_cnt) + " = getelemptr ";
                        koopa_str += array_ident + ", " + array_index + "\n";
                    }
                    else
                    {
                        koopa_str += "  %" + to_string(reg_cnt) + " = getelemptr ";
                        koopa_str += "%" + to_string(last_index) + ", " + array_index + "\n";
                    }

                    reg_cnt++;
                }

                koopa_str += "  %" + to_string(reg_cnt) + " = getelemptr ";
                if (array_size_list.size() == 0)
                    koopa_str += array_ident;
                else
                    koopa_str += "%" + to_string(reg_cnt - 1);
                koopa_str += ", 0\n";
                reg_cnt++;

                return make_pair(false, -1);
            }
        }
        else if (lval.type == POINTER)
        { // 指针（函数参数）
            if (lval.val == len)
            {
                // step1 取出对应参数
                koopa_str += "  %" + to_string(reg_cnt) + " = load " + array_ident + "\n";
                reg_cnt++;

                for (int i = 0; i < len; i++)
                {
                    string array_index;
                    int last_index = reg_cnt - 1;
                    pair<bool, int> res = array_size_list[i]->Koopa();
                    if (res.first)
                    {
                        array_index = to_string(res.second);
                    }
                    else
                    {
                        array_index = "%" + to_string(reg_cnt - 1);
                    }

                    if (i == 0)
                    {
                        koopa_str += "  %" + to_string(reg_cnt) + " = getptr ";
                        koopa_str += "%" + to_string(last_index) + ", " + array_index + "\n";
                    }
                    else
                    {
                        koopa_str += "  %" + to_string(reg_cnt) + " = getelemptr ";
                        koopa_str += "%" + to_string(last_index) + ", " + array_index + "\n";
                    }

                    reg_cnt++;
                }
                koopa_str += "  %" + to_string(reg_cnt) + " = load %" + to_string(reg_cnt - 1) + "\n";
                reg_cnt++;
                return make_pair(false, -1);
            }
            else
            {
                // step1 取出对应参数
                koopa_str += "  %" + to_string(reg_cnt) + " = load " + array_ident + "\n";
                reg_cnt++;

                for (int i = 0; i < len; i++)
                {
                    string array_index;
                    int last_index = reg_cnt - 1;
                    pair<bool, int> res = array_size_list[i]->Koopa();
                    if (res.first)
                    {
                        array_index = to_string(res.second);
                    }
                    else
                    {
                        array_index = "%" + to_string(reg_cnt - 1);
                    }

                    if (i == 0)
                    {
                        koopa_str += "  %" + to_string(reg_cnt) + " = getptr ";
                        koopa_str += "%" + to_string(last_index) + ", " + array_index + "\n";
                    }
                    else
                    {
                        koopa_str += "  %" + to_string(reg_cnt) + " = getelemptr ";
                        koopa_str += "%" + to_string(last_index) + ", " + array_index + "\n";
                    }

                    reg_cnt++;
                }

                if (array_size_list.size() == 0)
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = getptr ";
                    koopa_str += "%" + to_string(reg_cnt - 1) + ", 0\n";
                }
                else
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = getelemptr ";
                    koopa_str += "%" + to_string(reg_cnt - 1) + ", 0\n";
                }
                reg_cnt++;
                return make_pair(false, -1);
            }
        }
        return make_pair(false, -1);
    }
};

// Leval 左值变量 IDENT lv4
class LeValAST : public BaseAST
{
public:
    string ident;

    pair<bool, int> Koopa() const override
    {
        // koopa_str += "store %" + to_string(reg_cnt - 1) + ", @" + ident + "\n";
        return make_pair(false, -1);
    }
    string name() const override
    {
        Value var = symbol_list.getSymbol(ident);
        return "@" + ident + "_" + to_string(var.name_index);
    }
};

// lv9 unfinished
// （左值）数组变量
// lv9 IDENT ArraySizeList
class LeValArrayAST : public BaseAST
{
public:
    string ident;
    vector<unique_ptr<BaseAST>> array_size_list;

    pair<bool, int> Koopa() const override
    {
        return make_pair(false, -1);
    }
    string name() const override
    {
        Value var = symbol_list.getSymbol(ident);
        string array_ident = "@" + ident + "_" + to_string(var.name_index);
        if (var.type == ARRAY)
        {
            for (int i = 0; i < array_size_list.size(); i++)
            {
                int last_index = reg_cnt - 1;
                string array_index;
                pair<bool, int> res = array_size_list[i]->Koopa();

                if (res.first)
                {
                    array_index = to_string(res.second);
                }
                else
                {
                    array_index = "%" + to_string(reg_cnt - 1);
                }

                if (i == 0)
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = getelemptr ";
                    koopa_str += array_ident + ", " + array_index + "\n";
                }
                else
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = getelemptr ";
                    koopa_str += "%" + to_string(last_index) + ", " + array_index + "\n";
                }
                reg_cnt++;
            }
            return "%" + to_string(reg_cnt - 1);
        }
        else if (var.type == POINTER)
        {
            koopa_str += "  %" + to_string(reg_cnt) + " = load " + array_ident + "\n";
            reg_cnt++;

            for (int i = 0; i < array_size_list.size(); i++)
            {
                string array_index;
                int last_index = reg_cnt - 1;
                pair<bool, int> res = array_size_list[i]->Koopa();
                if (res.first)
                {
                    array_index = to_string(res.second);
                }
                else
                {
                    array_index = "%" + to_string(reg_cnt - 1);
                }

                if (i == 0)
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = getptr ";
                    koopa_str += "%" + to_string(last_index) + ", " + array_index + "\n";
                }
                else
                {
                    koopa_str += "  %" + to_string(reg_cnt) + " = getelemptr ";
                    koopa_str += "%" + to_string(last_index) + ", " + array_index + "\n";
                }

                reg_cnt++;
            }
            return "%" + to_string(reg_cnt - 1);
        }
        else
        { // ???
            return "%" + to_string(reg_cnt - 1);
        }
    }
};

// VarDecl 声明变量，需要列表 BType VarDef {"," VarDef} ";" lv4
class VarDeclAST : public BaseAST
{
public:
    vector<unique_ptr<BaseAST>> varDefList;

    pair<bool, int> Koopa() const override
    {
        for (auto &i : varDefList)
        {
            i->Koopa();
        }

        return make_pair(false, -1);
    }
};

// VarDef 变量定义 IDENT | IDENT "=" InitVal lv4
// 变量名分配策略：在第index层基本块，变量命名为 ident_index
// rule = 0 未初始化
// rule = 1 初始化 = initval
class VarDefAST : public BaseAST
{
public:
    int rule;
    string ident;
    unique_ptr<BaseAST> initval;

    pair<bool, int> Koopa() const override
    {
        // 全局变量koopa
        if (is_global)
        {
            // 初始化为0
            if (rule == 0)
            {
                Value tmp(VAR, 0, block_handler.block_now.index);

                string name = ident + "_" + to_string(block_handler.block_now.index);

                symbol_list.addSymbol(ident, tmp);

                koopa_str += "global @" + name + " = alloc i32, zeroinit\n";
            }
            else
            {
                int var_init = initval->calc();

                Value tmp(VAR, var_init, block_handler.block_now.index);

                string name = ident + "_" + to_string(block_handler.block_now.index);

                symbol_list.addSymbol(ident, tmp);

                koopa_str += "global @" + name + " = alloc i32, " + to_string(var_init) + "\n";
            }
            return make_pair(false, -1);
        }
        else
        {
            // 没有初始值
            if (rule == 0)
            {
                // var_type[ident] = VAR;
                Value tmp(VAR, 0, block_handler.block_now.index);

                string name = ident + "_" + to_string(block_handler.block_now.index);

                symbol_list.addSymbol(ident, tmp);

                koopa_str += "  @" + name + " = alloc i32 " + "\n";
            }
            else
            {
                // var_type[ident] = VAR;
                // var_val[ident] = initval->cal_value();
                // int val = (initval->Koopa()).second;

                string name = ident + "_" + to_string(block_handler.block_now.index);

                koopa_str += "  @" + name + " = alloc i32 " + "\n";
                pair<bool, int> res = initval->Koopa();
                if (res.first)
                {
                    koopa_str += "  store " + to_string(res.second) + ", @" + name + "\n";
                    Value tmp(VAR, res.second, block_handler.block_now.index);
                    symbol_list.addSymbol(ident, tmp);
                }
                else
                {
                    koopa_str += "  store %" + to_string(reg_cnt - 1) + ", @" + name + "\n";
                    Value tmp(VAR, 0, block_handler.block_now.index);
                    symbol_list.addSymbol(ident, tmp);
                }
            }
            return make_pair(false, -1);
        }
    }
};

// lv9 done
// unfinished
// 和ConstDefArray分外相似
// 但初始化列表有可能是空的
class VarDefArrayAST : public BaseAST
{
public:
    int rule;
    string ident;
    unique_ptr<BaseAST> init_val;
    vector<unique_ptr<BaseAST>> array_size_list;

    pair<bool, int> Koopa() const override
    {
        vector<int> len;
        for (auto &i : array_size_list)
        {
            len.push_back(i->calc());
        }

        int array_size = array_size_list.size();
        Value var_array = Value(ARRAY, array_size, block_handler.block_now.index);
        // 数组放入符号表
        symbol_list.addSymbol(ident, var_array);

        // 处理初始化列表
        // 先把所有初始值置为0
        // 再根据constinitval的值来修改
        int tot_len = 1;
        for (auto i : len)
        {
            tot_len *= i;
        }
        string *init = new string[tot_len];
        for (int i = 0; i < tot_len; ++i)
        {
            init[i] = "0";
        }

        // name tag
        string name_tag = "@" + ident + "_" + to_string(block_handler.block_now.index);

        // type tag
        string type_tag = "i32";
        int array_len = array_size_list.size();
        for (int i = array_len - 1; i >= 0; i--)
        {
            type_tag = "[" + type_tag + ", " + to_string(array_size_list[i]->calc()) + "]";
        }

        if (is_global)
        {
            if (init_val != nullptr) // 全局变量 有初值
            {
                dynamic_cast<InitValWithListAST *>(init_val.get())->getInitVal(init, len);
                string init_str = getInitList(init, len);
                koopa_str += "global " + name_tag + " = alloc ";
                koopa_str += type_tag + ", " + init_str + "\n";
            }
            else
            {
                string init_str = getInitList(init, len);
                koopa_str += "global " + name_tag + " = alloc ";
                koopa_str += type_tag + ", zeroinit" + "\n";
            }
        }
        else
        {
            koopa_str += "  " + name_tag + " = alloc " + type_tag + "\n";
            if (init_val == nullptr) // 局部变量 不用初始化就直接返回
            {
                return make_pair(false, -1);
            }
            dynamic_cast<InitValWithListAST *>(init_val.get())->getInitVal(init, len);
            string init_str = getInitList(init, len);
            initArray(name_tag, init, len);
        }

        return make_pair(false, -1);
    }
};

// lv4+
// InitVal 变量赋值 Exp
class InitValAST : public BaseAST
{
public:
    unique_ptr<BaseAST> exp;

    pair<bool, int> Koopa() const override
    {
        return exp->Koopa();
    }
    int calc() const override
    {
        return exp->calc();
    }
};
