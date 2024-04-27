#pragma once

#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include "symbol.h"
using namespace std;

extern string koopa_str;
extern int reg_cnt;
static bool re = false;

// 所有 AST 的基类
class BaseAST
{
public:
    virtual ~BaseAST() = default;

    virtual void Dump() const = 0;

    virtual void Koopa() const = 0;

    virtual int cal_value() const { return 0; }
};

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

    void Koopa() const override
    {
        func_def->Koopa();
    }
};

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

    void Koopa() const override
    {
        koopa_str += "fun ";
        koopa_str += "@";
        koopa_str += ident;
        koopa_str += "(): ";
        func_type->Koopa();
        block->Koopa();
    }
};

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

    void Koopa() const override
    {
        if (type == "int")
        {
            koopa_str += "i32 ";
        }
        else
        {
            koopa_str += "i32 ";
        }
    }
};

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

    void Koopa() const override
    {
        koopa_str += "{\n";
        koopa_str += "\%entry:\n";
        for (auto &i : blockItemList)
        {
            i->Koopa();
        }
        koopa_str += "}";
    }
};

// Stmt 语句 赋值|返回 LeVal "=" Exp ";" | "return" Exp ";" lv4
class StmtAST : public BaseAST
{
public:
    int rule;
    unique_ptr<BaseAST> exp;
    unique_ptr<BaseAST> leval;

    void Dump() const override
    {
        cout << "StmtAST { ";
        if (rule == 0)
        {
            leval->Dump();
            exp->Dump();
        }
        else
        {
            cout << "return ";
            exp->Dump();
        }
        cout << "; }";
    }

    void Koopa() const override
    {
        if (rule == 0)
        {
            if (re)
                return;
            exp->Koopa();
            leval->Koopa();
        }
        else
        {
            if (re)
                return;
            exp->Koopa();
            koopa_str += "  ret ";
            koopa_str += "%";
            koopa_str += to_string(reg_cnt - 1);
            koopa_str += "\n";
            re = true;
        }
    }
};

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
    void Koopa() const override
    {
        lorexp->Koopa();
    }
    int cal_value() const override { return lorexp->cal_value(); }
};

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
    void Koopa() const override
    {
        if (rule == 0)
        {
            exp->Koopa();
        }
        else if (rule == 1)
        {
            koopa_str += "%";
            koopa_str += to_string(reg_cnt);
            koopa_str += " = add 0, ";
            koopa_str += to_string(number);
            koopa_str += "\n";
            reg_cnt++;
        }
        else
        {
            lval->Koopa();
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

// UnaryExp 一元表达式，PrimaryExp | UnaryOp[!-+] UnaryExp lv3
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
    void Koopa() const override
    {
        if (rule == 0)
        {
            primaryexp->Koopa();
        }
        else
        {
            unaryexp->Koopa();
            if (op == "!")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = eq %";
                koopa_str += to_string(reg_cnt - 1);
                koopa_str += ", 0\n";
                reg_cnt++;
            }
            else if (op == "-")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = sub 0, %";
                koopa_str += to_string(reg_cnt - 1);
                koopa_str += "\n";
                reg_cnt++;
            }
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

// MulExp 乘法表达式 UnaryExp | MulExp [*/%] UnaryExp lv3
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
    void Koopa() const override
    {
        if (rule == 0)
        {
            unaryexp->Koopa();
        }
        else
        {
            mulexp->Koopa();
            int leftreg = reg_cnt - 1;
            unaryexp->Koopa();
            int rightreg = reg_cnt - 1;
            if (op == "*")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = mul %";
                koopa_str += to_string(leftreg);
                koopa_str += ", %";
                koopa_str += to_string(rightreg);
                koopa_str += "\n";
                reg_cnt++;
            }
            else if (op == "/")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = div %";
                koopa_str += to_string(leftreg);
                koopa_str += ", %";
                koopa_str += to_string(rightreg);
                koopa_str += "\n";
                reg_cnt++;
            }
            else if (op == "%")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = mod %";
                koopa_str += to_string(leftreg);
                koopa_str += ", %";
                koopa_str += to_string(rightreg);
                koopa_str += "\n";
                reg_cnt++;
            }
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

// AddExp MulExp | AddExp [+-] MulExp lv3
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
    void Koopa() const override
    {
        if (rule == 0)
        {
            mulexp->Koopa();
        }
        else
        {
            addexp->Koopa();
            int leftreg = reg_cnt - 1;
            mulexp->Koopa();
            int rightreg = reg_cnt - 1;
            if (op == "+")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = add %";
                koopa_str += to_string(leftreg);
                koopa_str += ", %";
                koopa_str += to_string(rightreg);
                koopa_str += "\n";
                reg_cnt++;
            }
            else if (op == "-")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = sub %";
                koopa_str += to_string(leftreg);
                koopa_str += ", %";
                koopa_str += to_string(rightreg);
                koopa_str += "\n";
                reg_cnt++;
            }
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

// RelExp AddExp | RelExp [<>LEGE] AddExp lv3
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
    void Koopa() const override
    {
        if (rule == 0)
        {
            addexp->Koopa();
        }
        else
        {
            relexp->Koopa();
            int leftreg = reg_cnt - 1;
            addexp->Koopa();
            int rightreg = reg_cnt - 1;
            if (op == "<")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = lt %";
                koopa_str += to_string(leftreg);
                koopa_str += ", %";
                koopa_str += to_string(rightreg);
                koopa_str += "\n";
                reg_cnt++;
            }
            else if (op == ">")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = gt %";
                koopa_str += to_string(leftreg);
                koopa_str += ", %";
                koopa_str += to_string(rightreg);
                koopa_str += "\n";
                reg_cnt++;
            }
            else if (op == "<=")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = le %";
                koopa_str += to_string(leftreg);
                koopa_str += ", %";
                koopa_str += to_string(rightreg);
                koopa_str += "\n";
                reg_cnt++;
            }
            else if (op == ">=")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = ge %";
                koopa_str += to_string(leftreg);
                koopa_str += ", %";
                koopa_str += to_string(rightreg);
                koopa_str += "\n";
                reg_cnt++;
            }
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

// EqExp RelExp | EqExp EQ/NE RelExp lv3
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
    void Koopa() const override
    {
        if (rule == 0)
        {
            relexp->Koopa();
        }
        else
        {
            eqexp->Koopa();
            int leftreg = reg_cnt - 1;
            relexp->Koopa();
            int rightreg = reg_cnt - 1;
            if (op == "==")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = eq %";
                koopa_str += to_string(leftreg);
                koopa_str += ", %";
                koopa_str += to_string(rightreg);
                koopa_str += "\n";
                reg_cnt++;
            }
            else if (op == "!=")
            {
                koopa_str += "%";
                koopa_str += to_string(reg_cnt);
                koopa_str += " = ne %";
                koopa_str += to_string(leftreg);
                koopa_str += ", %";
                koopa_str += to_string(rightreg);
                koopa_str += "\n";
                reg_cnt++;
            }
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

// LAndExp EqExp | LAndExp AND EqExp lv3
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
    void Koopa() const override
    {
        if (rule == 0)
        {
            eqexp->Koopa();
        }
        else
        {
            landexp->Koopa();
            int leftreg = reg_cnt - 1;
            eqexp->Koopa();
            int rightreg = reg_cnt - 1;
            /* 逻辑与 */
            // 左边逻辑取反
            koopa_str += "%";
            koopa_str += to_string(reg_cnt);
            koopa_str += " = eq %";
            koopa_str += to_string(leftreg);
            koopa_str += ", 0\n";
            reg_cnt++;
            // 右边逻辑取反
            koopa_str += "%";
            koopa_str += to_string(reg_cnt);
            koopa_str += " = eq %";
            koopa_str += to_string(rightreg);
            koopa_str += ", 0\n";
            reg_cnt++;
            // 对两边逻辑取反的结果取或
            koopa_str += "%";
            koopa_str += to_string(reg_cnt);
            koopa_str += " = or %";
            koopa_str += to_string(reg_cnt - 2);
            koopa_str += ", %";
            koopa_str += to_string(reg_cnt - 1);
            koopa_str += "\n";
            reg_cnt++;
            // 再取反
            koopa_str += "%";
            koopa_str += to_string(reg_cnt);
            koopa_str += " = eq %";
            koopa_str += to_string(reg_cnt - 1);
            koopa_str += ", 0\n";
            reg_cnt++;
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

// LOrExp LAndExp | LOrExp OR LAndExp lv3
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
    void Koopa() const override
    {
        if (rule == 0)
        {
            landexp->Koopa();
        }
        else
        {
            lorexp->Koopa();
            int leftreg = reg_cnt - 1;
            landexp->Koopa();
            int rightreg = reg_cnt - 1;
            /* 逻辑或 */
            // 左边不等于0？不等于则返回1
            koopa_str += "%";
            koopa_str += to_string(reg_cnt);
            koopa_str += " = ne %";
            koopa_str += to_string(leftreg);
            koopa_str += ", 0\n";
            reg_cnt++;
            // 右边不等于0？
            koopa_str += "%";
            koopa_str += to_string(reg_cnt);
            koopa_str += " = ne %";
            koopa_str += to_string(rightreg);
            koopa_str += ", 0\n";
            reg_cnt++;
            // 对两边结果取或
            koopa_str += "%";
            koopa_str += to_string(reg_cnt);
            koopa_str += " = or %";
            koopa_str += to_string(reg_cnt - 2);
            koopa_str += ", %";
            koopa_str += to_string(reg_cnt - 1);
            koopa_str += "\n";
            reg_cnt++;
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
    void Koopa() const override
    {
        if (rule == 0)
        {
            constdecl->Koopa();
        }
        else
        {
            vardecl->Koopa();
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
    void Koopa() const override
    {
        for (auto &i : constDefList)
        {
            i->Koopa();
        }
    }
};

// Btype 目前只有“INT”一种类型，暂略 lv4
class BtypeAST : public BaseAST
{
public:
    void Dump() const override {}
    void Koopa() const override {}
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
    void Koopa() const override
    {
        var_type[ident] = CONSTANT;
        var_val[ident] = constinitval->cal_value();
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
    void Koopa() const override {}
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
    void Koopa() const override {}
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
    void Koopa() const override
    {
        if (re)
            return;
        if (rule == 0)
            decl->Koopa();
        else
            stmt->Koopa();
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
    void Koopa() const override
    {
        // 常量
        if (var_type[ident] == CONSTANT)
        {
            koopa_str += "%" + to_string(reg_cnt) + " = add 0 ," + to_string(var_val[ident]) + "\n";
            reg_cnt++;
        }
        // 变量
        else
        {
            koopa_str += "%" + to_string(reg_cnt) + " = load @" + ident + "\n";
            reg_cnt++;
        }
    }
    int cal_value() const override
    {
        return var_val[ident];
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
    void Koopa() const override
    {
        koopa_str += "store %" + to_string(reg_cnt - 1) + ", @" + ident + "\n";
    };
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
    void Koopa() const override
    {
        for (auto &i : varDefList)
        {
            i->Koopa();
        }
    }
};

// VarDef 变量定义 IDENT | IDENT "=" InitVal lv4
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
    void Koopa() const override
    {
        if (rule == 0)
        {
            var_type[ident] = VAR;
            koopa_str += "@" + ident + " = alloc i32 " + "\n";
        }
        else
        {
            var_type[ident] = VAR;
            var_val[ident] = initval->cal_value();
            koopa_str += "@" + ident + " = alloc i32 " + "\n";
            initval->Koopa();
            koopa_str += "store %" + to_string(reg_cnt - 1) + ", @" + ident + "\n";
        }
    }
};

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
    void Koopa() const override
    {
        exp->Koopa();
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