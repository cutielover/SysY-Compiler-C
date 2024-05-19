#pragma once

#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <unordered_map>
#include "symbol.h"
using namespace std;

extern string koopa_str;
extern int reg_cnt;
extern int if_cnt;

static int block_cnt;
static int block_now;
static int block_last;
static vector<bool> block_end;
static unordered_map<int, int> block_parent;

static bool re = false;
static bool if_end = true;

extern SymbolList symbol_list;

// lv4+
// 所有 AST 的基类
class BaseAST
{
public:
    virtual ~BaseAST() = default;

    virtual void Dump() const = 0;

    // !!!
    // 用于优化，对于能直接计算出值的表达式，直接返回值，减少寄存器浪费
    // bool: true-能计算 false-不能
    // int: 能计算出的值；对于不能计算出的值，返回-1
    virtual pair<bool, int> Koopa() const { return make_pair(false, -1); }

    virtual string name() const { return "oops"; };

    virtual int cal_value() const { return 0; }
};

// lv4+
// CompUnit 是 BaseAST
class CompUnitAST : public BaseAST
{
public:
    // 用智能指针管理对象
    unique_ptr<BaseAST> func_def;

    void Dump() const override
    {
        cout << "CompUnitAST { ";
        func_def->Dump();
        cout << " }";
    }

    pair<bool, int> Koopa() const override
    {
        func_def->Koopa();
        return make_pair(false, -1);
    }
};

// lv4+
// FuncDef 也是 BaseAST
class FuncDefAST : public BaseAST
{
public:
    unique_ptr<BaseAST> func_type;
    string ident;
    unique_ptr<BaseAST> block;

    void Dump() const override
    {
        cout << "FuncDefAST { ";
        func_type->Dump();
        cout << ", " << ident << ", ";
        block->Dump();
        cout << " }";
    }

    pair<bool, int> Koopa() const override
    {
        koopa_str += "fun ";
        koopa_str += "@";
        koopa_str += ident;
        koopa_str += "(): ";

        func_type->Koopa();

        koopa_str += "{\n";
        koopa_str += "\%entry:\n";

        block->Koopa();

        koopa_str += "}";
        return make_pair(false, -1);
    }
};

// lv4+
// FuncType
class FuncTypeAST : public BaseAST
{
public:
    string type;

    void Dump() const override
    {
        cout << "FuncTypeAST { ";
        cout << type;
        cout << " }";
    }

    pair<bool, int> Koopa() const override
    {
        if (type == "int")
        {
            koopa_str += "i32 ";
        }
        else
        {
            koopa_str += "i32 ";
        }
        return make_pair(false, -1);
    }
};

// lv4+
// Block lv4
class BlockAST : public BaseAST
{
public:
    vector<unique_ptr<BaseAST>> blockItemList;

    void Dump() const override
    {
        cout << "BlockAST { ";
        for (auto &i : blockItemList)
        {
            i->Dump();
            cout << ",";
        }
        cout << " }";
    }

    pair<bool, int> Koopa() const override
    {
        re = false;
        symbol_list.newMap();

        // 块计数：当前块为修改后的block_cnt
        block_cnt++;
        block_last = block_now;
        int parent_block = block_now;
        // 记录：当前块的父亲为block_last（即上一个执行的块）
        // 边界：起始block_cnt为1，block_now为0
        block_now = block_cnt;
        block_parent[block_now] = parent_block;
        // 修改：将当前块正式修改为block_cnt
        // 记录：设置当前块为未完结的
        block_end.push_back(false);

        for (auto &i : blockItemList)
        {
            if (block_end[block_now])
            {
                // koopa_str += "\n";
                // koopa_str += "Block" + to_string(block_now) + " end, Stop Process BlockItem\n\n";
                break;
            }
            i->Koopa();
        }

        // koopa_str += "\n";
        // koopa_str += "/////////////Block" + to_string(block_now) + ": block_end = ";
        // if (block_end[block_now])
        // {
        //     koopa_str += "true/////////////\n\n";
        // }
        // else
        // {
        //     koopa_str += "false////////////\n\n";
        // }

        // 处理if{}的情况
        if (parent_block != 0)
            block_end[parent_block] = block_end[block_now];

        // koopa_str += "/////////////Block" + to_string(parent_block) + ": block_end = ";
        // if (block_end[parent_block])
        // {
        //     koopa_str += "true/////////////\n\n";
        // }
        // else
        // {
        //     koopa_str += "false////////////\n\n";
        // }

        // 子块完成后，归位block_last
        block_now = block_parent[block_now];

        symbol_list.deleteMap();

        return make_pair(false, -1);
    }
};

// Leval 左值变量 IDENT lv4
class LeValAST : public BaseAST
{
public:
    string ident;
    void Dump() const override
    {
        cout << "LeVal { ";
        cout << ident;
        cout << " }";
    };
    pair<bool, int> Koopa() const override
    {
        // koopa_str += "store %" + to_string(reg_cnt - 1) + ", @" + ident + "\n";
        return make_pair(false, -1);
    };
    string name() const override
    {
        Value var = symbol_list.getSymbol(ident);
        return ident + "_" + to_string(var.name_index);
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
    // unique_ptr<BaseAST> if_stmt;

    void Dump() const override
    {
        cout << "StmtAST { ";
        if (rule == 0)
        {
            leval->Dump();
            exp->Dump();
        }
        else if (rule == 1)
        {
            cout << "return ";
            exp->Dump();
        }
        else if (rule == 2)
        {
            cout << "exp ";
            exp->Dump();
        }
        else if (rule == 3)
        {
            cout << "block ";
            block->Dump();
        }
        cout << "; }";
    }

    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            pair<bool, int> res = exp->Koopa();
            if (res.first)
            {
                koopa_str += "  store " + to_string(res.second) + ", @" + leval->name() + "\n";
            }
            else
            {
                koopa_str += "  store %" + to_string(reg_cnt - 1) + ", @" + leval->name() + "\n";
            }
            block_end[block_now] = false;
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
            // koopa_str += "Ret: Block" + to_string(block_now) + "\n";
            block_end[block_now] = true;
            // re = true;
        }
        else if (rule == 2)
        {
            if (exp != nullptr)
                exp->Koopa();
            block_end[block_now] = false;
        }
        else if (rule == 3)
        {
            block->Koopa();
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
    void Dump() const override
    {
        cout << "IfStmtAST { ";
        if_stmt->Dump();
        cout << "; }";
    }
    pair<bool, int> Koopa() const override
    {
        if_cnt++;
        int now_if_cnt = if_cnt;
        // If
        if (else_stmt == nullptr)
        {
            // 说明if的结尾是end
            if_end = true;
            if_stmt->Koopa();
            // now_if_cnt = if_cnt;
            string end_tag = "%end_" + to_string(now_if_cnt);
            koopa_str += end_tag + ":\n";
            // if条件语句不能意味着结束
            block_end[block_now] = false;
        }
        // If ELSE Stmt
        else
        {
            if_end = false;

            if_stmt->Koopa();
            bool if_stmt_end = block_end[block_now];

            // now_if_cnt = if_cnt;
            string end_tag = "%end_" + to_string(now_if_cnt);
            string else_tag = "%else_" + to_string(now_if_cnt);

            // else tag:
            koopa_str += else_tag + ":\n";

            else_stmt->Koopa();
            bool else_stmt_end = block_end[block_now];

            end_tag = "%end_" + to_string(now_if_cnt);

            if (!block_end[block_now])
            {
                // cout << end_tag << " jump at line 358" << endl;
                koopa_str += "  jump " + end_tag + "\n\n";
            }
            if (if_stmt_end && else_stmt_end)
            {
                block_end[block_now] = true;
            }
            else
            {
                block_end[block_now] = false;
                koopa_str += end_tag + ":\n";
            }
            // koopa_str += end_tag + ":\n";
        }
        // if_cnt--;
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
    void Dump() const override
    {
        cout << "IfAST { ";
        cout << "if ( ";
        exp->Dump();
        cout << " ) ";
        stmt->Dump();
        cout << "; }";
    }
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

        if (!block_end[block_now])
        {
            // koopa_str += "Jump: block_last = " + to_string(block_last) + "\n";
            // koopa_str += "Jump: block_now = " + to_string(block_now) + "\n";
            // koopa_str += "Jump: Block" + to_string(block_now) + "\n";
            // cout << end_tag << " jump at line 435" << endl;
            koopa_str += "  jump " + end_tag + "\n";
        }
        koopa_str += "\n";
        // end of if branch
        // koopa_str += end_tag + ":\n";

        return make_pair(false, -1);
    }
};

// lv4+
// Exp 表达式 lv3
class ExpAST : public BaseAST
{
public:
    unique_ptr<BaseAST> lorexp;
    void Dump() const override
    {
        cout << "Exp { ";
        lorexp->Dump();
        cout << " }";
    }
    pair<bool, int> Koopa() const override
    {
        return lorexp->Koopa();
    }
    int cal_value() const override { return lorexp->cal_value(); }
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
    void Dump() const override
    {
        cout << "PrimaryExp { ";
        if (rule == 0)
        {
            exp->Dump();
        }
        else if (rule == 1)
        {
            cout << number;
        }
        else
        {
            lval->Dump();
        }
        cout << " }";
    }
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
    int cal_value() const override
    {
        if (rule == 0)
        {
            return exp->cal_value();
        }
        else if (rule == 1)
        {
            return number;
        }
        // error case
        return lval->cal_value();
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
    void Dump() const override
    {
        cout << "UnaryExp { ";
        if (rule == 0)
        {
            primaryexp->Dump();
        }
        else
        {
            cout << op << " ";
            unaryexp->Dump();
        }
        cout << " }";
    }
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

                    ////////////////
                    koopa_str += "  %" + to_string(reg_cnt) + " = eq " + to_string(res.second) + ", 0\n";
                    reg_cnt++;
                }
                else if (op == "-")
                {
                    return make_pair(true, -res.second);

                    /////////////////
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
    int cal_value() const override
    {
        if (rule == 0)
        {
            return primaryexp->cal_value();
        }
        else
        {
            if (op == "!")
            {
                return !unaryexp->cal_value();
            }
            else if (op == "-")
            {
                return -unaryexp->cal_value();
            }
            else
            {
                return unaryexp->cal_value();
            }
        }
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
    void Dump() const override
    {
        cout << "MulExp { ";
        if (rule == 0)
        {
            unaryexp->Dump();
        }
        else
        {
            mulexp->Dump();
            cout << op << " ";
            unaryexp->Dump();
        }
        cout << " }";
    }
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
    int cal_value() const override
    {
        if (rule == 0)
            return unaryexp->cal_value();
        else
        {
            if (op == "*")
            {
                return mulexp->cal_value() * unaryexp->cal_value();
            }
            else if (op == "/")
            {
                return mulexp->cal_value() / unaryexp->cal_value();
            }
            else
            {
                return mulexp->cal_value() % unaryexp->cal_value();
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
    void Dump() const override
    {
        cout << "AddExp { ";
        if (rule == 0)
        {
            mulexp->Dump();
        }
        else
        {
            addexp->Dump();
            cout << op << " ";
            mulexp->Dump();
        }
        cout << " }";
    }
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
    int cal_value() const override
    {
        if (rule == 0)
        {
            return mulexp->cal_value();
        }
        else
        {
            if (op == "+")
            {
                return addexp->cal_value() + mulexp->cal_value();
            }
            else
            {
                return addexp->cal_value() - mulexp->cal_value();
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
    void Dump() const override
    {
        cout << "RelExp { ";
        if (rule == 0)
        {
            addexp->Dump();
        }
        else
        {
            relexp->Dump();
            cout << op << " ";
            addexp->Dump();
        }
        cout << " }";
    }
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
    int cal_value() const override
    {
        if (rule == 0)
        {
            return addexp->cal_value();
        }
        else
        {
            if (op == "<")
            {
                return relexp->cal_value() < addexp->cal_value();
            }
            else if (op == ">")
            {
                return relexp->cal_value() > addexp->cal_value();
            }
            else if (op == "<=")
            {
                return relexp->cal_value() <= addexp->cal_value();
            }
            else
            {
                return relexp->cal_value() >= addexp->cal_value();
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

    void Dump() const override
    {
        cout << "EqExp { ";
        if (rule == 0)
        {
            relexp->Dump();
        }
        else
        {
            eqexp->Dump();
            cout << op << " ";
            relexp->Dump();
        }
        cout << " }";
    }
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
    int cal_value() const override
    {
        if (rule == 0)
            return relexp->cal_value();
        else
        {
            if (op == "==")
            {
                return eqexp->cal_value() == relexp->cal_value();
            }
            else
            {
                return eqexp->cal_value() != relexp->cal_value();
            }
        }
    }
};

// lv4+
// LAndExp ::= EqExp | LAndExp AND EqExp lv3
class LAndExpAST : public BaseAST
{
public:
    int rule;
    unique_ptr<BaseAST> eqexp;
    unique_ptr<BaseAST> landexp;

    void Dump() const override
    {
        cout << "LAndExp { ";
        if (rule == 0)
        {
            eqexp->Dump();
        }
        else
        {
            landexp->Dump();
            cout << "&&" << " ";
            eqexp->Dump();
        }
        cout << " }";
    }
    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            return eqexp->Koopa();
        }
        else
        {
            pair<bool, int> res_l = landexp->Koopa();
            int leftreg = reg_cnt - 1;
            pair<bool, int> res_r = eqexp->Koopa();
            int rightreg = reg_cnt - 1;

            if (res_l.first && res_r.first)
            {
                return make_pair(true, res_l.second && res_r.second);
            }
            else if (res_l.first && !res_r.first)
            {
                /* 逻辑与 */
                // 左边逻辑取反
                // koopa_str += "%" + to_string(reg_cnt) + " = eq %";
                // koopa_str += to_string(leftreg) + ", 0\n";
                // reg_cnt++;
                if (res_l.second == 0)
                    return make_pair(true, 0);
                // 右边逻辑取反
                koopa_str += "  %" + to_string(reg_cnt) + " = eq %";
                koopa_str += to_string(rightreg) + ", 0\n";
                reg_cnt++;
                // 对两边逻辑取反的结果取或
                koopa_str += "  %" + to_string(reg_cnt) + " = or ";
                koopa_str += to_string(!res_l.second) + ", %" + to_string(reg_cnt - 1) + "\n";
                reg_cnt++;
                // 再取反
                koopa_str += "  %" + to_string(reg_cnt) + " = eq %";
                koopa_str += to_string(reg_cnt - 1) + ", 0\n";
                reg_cnt++;
            }
            else if (!res_l.first && res_r.first)
            {
                if (res_r.second == 0)
                    return make_pair(true, 0);
                /* 逻辑与 */
                // 左边逻辑取反
                koopa_str += "  %" + to_string(reg_cnt) + " = eq %";
                koopa_str += to_string(leftreg) + ", 0\n";
                reg_cnt++;
                // 右边逻辑取反
                // koopa_str += "%" + to_string(reg_cnt) + " = eq %";
                // koopa_str += to_string(rightreg) + ", 0\n";
                // reg_cnt++;
                // 对两边逻辑取反的结果取或
                koopa_str += "  %" + to_string(reg_cnt) + " = or ";
                koopa_str += to_string(!res_r.second) + ", %" + to_string(reg_cnt - 1) + "\n";
                reg_cnt++;
                // 再取反
                koopa_str += "  %" + to_string(reg_cnt) + " = eq %";
                koopa_str += to_string(reg_cnt - 1) + ", 0\n";
                reg_cnt++;
            }
            else
            {
                /* 逻辑与 */
                // 左边逻辑取反
                koopa_str += "  %" + to_string(reg_cnt) + " = eq %";
                koopa_str += to_string(leftreg) + ", 0\n";
                reg_cnt++;
                // 右边逻辑取反
                koopa_str += "  %" + to_string(reg_cnt) + " = eq %";
                koopa_str += to_string(rightreg) + ", 0\n";
                reg_cnt++;
                // 对两边逻辑取反的结果取或
                koopa_str += "  %" + to_string(reg_cnt) + " = or %";
                koopa_str += to_string(reg_cnt - 2) + ", %" + to_string(reg_cnt - 1) + "\n";
                reg_cnt++;
                // 再取反
                koopa_str += "  %" + to_string(reg_cnt) + " = eq %";
                koopa_str += to_string(reg_cnt - 1) + ", 0\n";
                reg_cnt++;
            }
            return make_pair(false, -1);
        }
    }
    int cal_value() const override
    {
        if (rule == 0)
            return eqexp->cal_value();
        else
        {
            return landexp->cal_value() && eqexp->cal_value();
        }
    }
};

// lv4+
// LOrExp ::= LAndExp | LOrExp OR LAndExp lv3
class LOrExpAST : public BaseAST
{
public:
    int rule;
    unique_ptr<BaseAST> landexp;
    unique_ptr<BaseAST> lorexp;
    void Dump() const override
    {
        cout << "LOrExp { ";
        if (rule == 0)
        {
            landexp->Dump();
        }
        else
        {
            lorexp->Dump();
            cout << "||" << " ";
            landexp->Dump();
        }
        cout << " }";
    }
    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            return landexp->Koopa();
        }
        else
        {
            pair<bool, int> res_l = lorexp->Koopa();
            int leftreg = reg_cnt - 1;
            pair<bool, int> res_r = landexp->Koopa();
            int rightreg = reg_cnt - 1;

            if (res_l.first && res_r.first)
            {
                int ans = res_l.second || res_r.second;
                return make_pair(true, ans);
            }
            else if (res_l.first && !res_r.first)
            {
                /* 逻辑或 */
                // 左边不等于0？不等于则返回1
                // koopa_str += "%" + to_string(reg_cnt) + " = ne %";
                // koopa_str += to_string(leftreg) + ", 0\n";
                // reg_cnt++;
                int ans_l = res_l.second == 0 ? 0 : 1;
                if (ans_l != 0)
                    return make_pair(true, 1);
                // 右边不等于0？
                koopa_str += "  %" + to_string(reg_cnt) + " = ne %";
                koopa_str += to_string(rightreg) + ", 0\n";
                reg_cnt++;
                // 对两边结果取或
                koopa_str += "  %" + to_string(reg_cnt) + " = or ";
                koopa_str += to_string(ans_l) + ", %" + to_string(reg_cnt - 1) + "\n";
                reg_cnt++;
            }
            else if (!res_l.first && res_r.first)
            {
                /* 逻辑或 */
                // 右边不等于0？
                // koopa_str += "%" + to_string(reg_cnt) + " = ne %";
                // koopa_str += to_string(rightreg) + ", 0\n";
                // reg_cnt++;
                int ans_r = res_r.second == 0 ? 0 : 1;
                if (ans_r != 0)
                    return make_pair(true, 1);
                // 左边不等于0？不等于则返回1
                koopa_str += "  %" + to_string(reg_cnt) + " = ne %";
                koopa_str += to_string(leftreg) + ", 0\n";
                reg_cnt++;
                // 对两边结果取或
                // 好像不需要
                koopa_str += "  %" + to_string(reg_cnt) + " = or ";
                koopa_str += to_string(ans_r) + ", %" + to_string(reg_cnt - 1) + "\n";
                reg_cnt++;
            }
            else
            {
                /* 逻辑或 */
                // 左边不等于0？不等于则返回1
                koopa_str += "  %" + to_string(reg_cnt) + " = ne %";
                koopa_str += to_string(leftreg) + ", 0\n";
                reg_cnt++;
                // 右边不等于0？
                koopa_str += "  %" + to_string(reg_cnt) + " = ne %";
                koopa_str += to_string(rightreg) + ", 0\n";
                reg_cnt++;
                // 对两边结果取或
                koopa_str += "  %" + to_string(reg_cnt) + " = or %";
                koopa_str += to_string(reg_cnt - 2) + ", %" + to_string(reg_cnt - 1) + "\n";
                reg_cnt++;
            }
            return make_pair(false, -1);
        }
    }
    int cal_value() const override
    {
        if (rule == 0)
            return landexp->cal_value();
        else
            return lorexp->cal_value() || landexp->cal_value();
    }
};

// Decl 声明：常量/变量 ConstDecl | VarDecl lv4
class DeclAST : public BaseAST
{
public:
    int rule;
    unique_ptr<BaseAST> constdecl;
    unique_ptr<BaseAST> vardecl;
    void Dump() const override
    {
        cout << "Decl { ";
        if (rule == 0)
        {
            constdecl->Dump();
        }
        else
        {
            vardecl->Dump();
        }
        cout << " }";
    }
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
    void Dump() const override
    {
        cout << "ConstDecl { ";
        for (auto &i : constDefList)
        {
            i->Dump();
            cout << ",";
        }
        cout << " }";
    }
    pair<bool, int> Koopa() const override
    {
        for (auto &i : constDefList)
        {
            i->Koopa();
        }
        return make_pair(false, -1);
    }
};

// Btype 目前只有“INT”一种类型，暂略 lv4
class BtypeAST : public BaseAST
{
public:
    void Dump() const override {}
};

// ConstDef 常量定义 IDENT "=" ConstInitVal lv4
// 示例 a = 3+5
class ConstDefAST : public BaseAST
{
public:
    string ident;
    unique_ptr<BaseAST> constinitval;
    void Dump() const override
    {
        cout << "ConstDef { ";
        cout << ident << " = ";
        constinitval->Dump();
        cout << " }";
    }
    pair<bool, int> Koopa() const override
    {
        int val = (constinitval->Koopa()).second;
        Value tmp(CONSTANT, val, block_now);
        symbol_list.addSymbol(ident, tmp);

        // var_type[ident] = CONSTANT;
        // // var_val[ident] = constinitval->cal_value();
        // var_val[ident] = (constinitval->Koopa()).second;
        return make_pair(false, -1);
    }
};

// ConstInitVal 常量赋值 ConstExp lv4
class ConstInitValAST : public BaseAST
{
public:
    unique_ptr<BaseAST> constexp;
    void Dump() const override
    {
        cout << "ConstInitVal { ";
        constexp->Dump();
        cout << " }";
    }
    pair<bool, int> Koopa() const override
    {
        return constexp->Koopa();
    }
    int cal_value() const override
    {
        return constexp->cal_value();
    }
};

// ConstExp 常量赋值表达式 Exp lv4
class ConstExpAST : public BaseAST
{
public:
    unique_ptr<BaseAST> exp;
    void Dump() const override
    {
        cout << "ConstExp { ";
        exp->Dump();
        cout << " }";
    }
    pair<bool, int> Koopa() const override
    {
        return exp->Koopa();
    }
    int cal_value() const override
    {
        return exp->cal_value();
    }
};

// BlockItem Decl | Stmt lv4
class BlockItemAST : public BaseAST
{
public:
    int rule;
    unique_ptr<BaseAST> decl;
    unique_ptr<BaseAST> stmt;
    void Dump() const override
    {
        cout << "BlockItem { ";
        if (rule == 0)
            decl->Dump();
        else
            stmt->Dump();
        cout << " }";
    }
    pair<bool, int> Koopa() const override
    {
        if (re)
            return make_pair(false, -1);
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
    void Dump() const override
    {
        cout << "LVal { ";
        cout << ident;
        cout << " }";
    }
    pair<bool, int> Koopa() const override
    {
        Value cur_var = symbol_list.getSymbol(ident);
        // 常量
        if (cur_var.type == CONSTANT)
        {
            return make_pair(true, cur_var.val);
        }
        // 变量
        else
        {
            koopa_str += "  %" + to_string(reg_cnt) + " = load @" + ident + "_" + to_string(cur_var.name_index) + "\n";
            reg_cnt++;
            // !!!
            return make_pair(false, -1);
        }
    }
};

// VarDecl 声明变量，需要列表 BType VarDef {"," VarDef} ";" lv4
class VarDeclAST : public BaseAST
{
public:
    vector<unique_ptr<BaseAST>> varDefList;
    void Dump() const override
    {
        cout << "VarDecl { ";
        for (auto &i : varDefList)
        {
            i->Dump();
            cout << ",";
        }
        cout << " }";
    }
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
class VarDefAST : public BaseAST
{
public:
    int rule;
    string ident;
    unique_ptr<BaseAST> initval;
    void Dump() const override
    {
        cout << "VarDef { ";
        cout << ident;
        if (rule == 1)
        {
            cout << " = ";
            initval->Dump();
        }
        cout << " }";
    }
    pair<bool, int> Koopa() const override
    {
        if (rule == 0)
        {
            // var_type[ident] = VAR;
            Value tmp(VAR, 0, block_now);

            string name = ident + "_" + to_string(block_now);

            symbol_list.addSymbol(ident, tmp);

            koopa_str += "  @" + name + " = alloc i32 " + "\n";
            return make_pair(false, -1);
        }
        else
        {
            // var_type[ident] = VAR;
            // var_val[ident] = initval->cal_value();
            int val = (initval->Koopa()).second;
            Value tmp(VAR, val, block_now);

            string name = ident + "_" + to_string(block_now);

            symbol_list.addSymbol(ident, tmp);

            koopa_str += "  @" + name + " = alloc i32 " + "\n";
            pair<bool, int> res = initval->Koopa();
            if (res.first)
            {
                koopa_str += "  store " + to_string(res.second) + ", @" + name + "\n";
            }
            else
            {
                koopa_str += "  store %" + to_string(reg_cnt - 1) + ", @" + name + "\n";
            }
            return make_pair(false, -1);
        }
    }
};

// lv4+
// InitVal 变量赋值 Exp
class InitValAST : public BaseAST
{
public:
    unique_ptr<BaseAST> exp;
    void Dump() const override
    {
        cout << "InitVal { ";
        exp->Dump();
        cout << " }";
    }
    pair<bool, int> Koopa() const override
    {
        return exp->Koopa();
    }
    int cal_value() const override
    {
        return exp->cal_value();
    }
};

// Number
// class NumberAST : public BaseAST
// {
// public:
//     int val;

//     void Dump() const override
//     {
//         std::cout << "NumberAST { ";
//         std::cout << val;
//         std::cout << " }";
//     }
// };