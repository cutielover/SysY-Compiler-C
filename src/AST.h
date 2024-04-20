#pragma once

#include <memory>
#include <string>
#include <vector>
#include <iostream>
using namespace std;

extern string koopa_str;
extern int reg_cnt;

// 所有 AST 的基类
class BaseAST
{
public:
    virtual ~BaseAST() = default;

    virtual void Dump() const = 0;

    virtual void Koopa() const = 0;
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

// Block
class BlockAST : public BaseAST
{
public:
    unique_ptr<BaseAST> stmt;

    void Dump() const override
    {
        cout << "BlockAST { ";
        stmt->Dump();
        cout << " }";
    }

    void Koopa() const override
    {
        koopa_str += "{\n";
        koopa_str += "\%entry:\n";
        stmt->Koopa();
        koopa_str += "}";
    }
};

// Stmt
class StmtAST : public BaseAST
{
public:
    unique_ptr<BaseAST> exp;

    void Dump() const override
    {
        cout << "StmtAST { return ";
        exp->Dump();
        cout << "; }";
    }

    void Koopa() const override
    {
        exp->Koopa();
        koopa_str += "  ret ";
        koopa_str += "%";
        koopa_str += to_string(reg_cnt - 1);
        koopa_str += "\n";
    }
};

// Exp 表达式
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
};

// PrimaryExp 基本表达式，(Exp) | Number
class PrimaryExpAST : public BaseAST
{
public:
    int rule;
    int number;
    unique_ptr<BaseAST> exp;
    void Dump() const override
    {
        cout << "PrimaryExp { ";
        if (rule == 0)
        {
            exp->Dump();
        }
        else
        {
            cout << number;
        }
        cout << " }";
    }
    void Koopa() const override
    {
        if (rule == 0)
        {
            exp->Koopa();
        }
        else
        {
            koopa_str += "%";
            koopa_str += to_string(reg_cnt);
            koopa_str += " = add 0, ";
            koopa_str += to_string(number);
            koopa_str += "\n";
            reg_cnt++;
        }
    }
};

// UnaryExp 一元表达式，PrimaryExp | UnaryOp UnaryExp
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
};

// MulExp 乘法表达式 UnaryExp|MulExp [*/%] UnaryExp
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
};

// AddExp
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
};

// RelExp
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
};

// EqExp
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
};

// LAndExp
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
};

// LOrExp
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