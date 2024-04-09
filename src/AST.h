#pragma once
#include <memory>
#include <string>
#include <vector>
#include <iostream>

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
    std::unique_ptr<BaseAST> func_def;

    void Dump() const override
    {
        std::cout << "CompUnitAST { ";
        func_def->Dump();
        std::cout << " }";
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
    std::unique_ptr<BaseAST> func_type;
    std::string ident;
    std::unique_ptr<BaseAST> block;

    void Dump() const override
    {
        std::cout << "FuncDefAST { ";
        func_type->Dump();
        std::cout << ", " << ident << ", ";
        block->Dump();
        std::cout << " }";
    }

    void Koopa() const override
    {
        std::cout << "fun ";
        std::cout << "@" << ident << "(): ";
        func_type->Koopa();
        block->Koopa();
    }
};

// FuncType
class FuncTypeAST : public BaseAST
{
public:
    std::string type;

    void Dump() const override
    {
        std::cout << "FuncTypeAST { ";
        std::cout << type;
        std::cout << " }";
    }

    void Koopa() const override
    {
        if (type == "int")
        {
            std::cout << "i32 ";
        }
        else
        {
            std::cout << "i32 ";
        }
    }
};

// Block
class BlockAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> stmt;

    void Dump() const override
    {
        std::cout << "BlockAST { ";
        stmt->Dump();
        std::cout << " }";
    }

    void Koopa() const override
    {
        std::cout << "{" << "\n";
        std::cout << "\%entry:" << "\n";
        stmt->Koopa();
        std::cout << "}";
    }
};

// Stmt
class StmtAST : public BaseAST
{
public:
    int number;

    void Dump() const override
    {
        std::cout << "StmtAST { return ";
        std::cout << number;
        std::cout << "; }";
    }

    void Koopa() const override
    {
        std::cout << "  ret ";
        std::cout << number;
        std::cout << "\n";
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