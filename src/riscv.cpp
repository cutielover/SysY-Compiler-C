#include "riscv.h"
using namespace std;
// 函数声明略
// ...

// 记录一条指令对应的寄存器
unordered_map<koopa_raw_value_t, string> regs;
static int reg_num = 0;

static string r = "t";

// 访问 raw program
void Visit(const koopa_raw_program_t &program)
{
    // 执行一些其他的必要操作
    // ...
    // 访问所有全局变量
    Visit(program.values);
    // 访问所有函数
    Visit(program.funcs);
}

// 访问 raw slice
void Visit(const koopa_raw_slice_t &slice)
{
    for (size_t i = 0; i < slice.len; ++i)
    {
        auto ptr = slice.buffer[i];
        // 根据 slice 的 kind 决定将 ptr 视作何种元素
        switch (slice.kind)
        {
        case KOOPA_RSIK_FUNCTION:
            // 访问函数
            Visit(reinterpret_cast<koopa_raw_function_t>(ptr));
            break;
        case KOOPA_RSIK_BASIC_BLOCK:
            // 访问基本块
            Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr));
            break;
        case KOOPA_RSIK_VALUE:
            // 访问指令
            Visit(reinterpret_cast<koopa_raw_value_t>(ptr));
            break;
        default:
            // 我们暂时不会遇到其他内容, 于是不对其做任何处理
            assert(false);
        }
    }
}

// 访问函数
void Visit(const koopa_raw_function_t &func)
{
    // 执行一些其他的必要操作
    // ...
    cout << "  .text\n";
    cout << "  .globl " << func->name + 1 << "\n";
    cout << func->name + 1 << ":\n";
    // 访问所有基本块
    // 此时只有一个基本块
    Visit(func->bbs);
}

// 访问基本块
void Visit(const koopa_raw_basic_block_t &bb)
{
    // 执行一些其他的必要操作
    // ...
    // 访问所有指令
    Visit(bb->insts);
}

// 访问指令
void Visit(const koopa_raw_value_t &value)
{
    // 根据指令类型判断后续需要如何访问
    const auto &kind = value->kind;
    switch (kind.tag)
    {
    case KOOPA_RVT_RETURN:
        // 访问 return 指令
        Visit(kind.data.ret);
        break;
    case KOOPA_RVT_INTEGER:
        // 访问 integer 指令
        Visit(kind.data.integer);
        break;
    case KOOPA_RVT_BINARY:
        // 访问 binary 指令
        Visit(kind.data.binary, value);
        break;
    default:
        // 其他类型暂时遇不到
        assert(false);
    }
}

// 为（整数）变量选择合适的寄存器
// 返回值含义：
// false表示未使用临时寄存器 true表示使用
bool Reg_Select(const koopa_raw_value_t &value)
{
    if (reg_num == 7)
    {
        reg_num = 0;
        r = "a";
    }

    // 0直接使用x0寄存器
    if (value->kind.tag == KOOPA_RVT_INTEGER && value->kind.data.integer.value == 0)
    {
        regs[value] = "x0";
        return false;
    }

    // 非0，使用li指令加载
    else if (value->kind.tag == KOOPA_RVT_INTEGER && value->kind.data.integer.value != 0)
    {
        // step1: 加载变量到某寄存器
        cout << "  li " << r << reg_num << ", ";
        Visit(value->kind.data.integer);

        // step2: 记录使用的寄存器
        regs[value] = r + to_string(reg_num);
        reg_num++;

        return true;
    }

    return false;
}

void Visit(const koopa_raw_binary_t &binary, const koopa_raw_value_t &value)
{
    static string r_0 = r;
    switch (binary.op)
    {
    case KOOPA_RBO_NOT_EQ:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }

        cout << "  xor " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";
        cout << "  snez " << r_0 << cur << ", " << r_0 << cur << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    case KOOPA_RBO_EQ:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }
        cout << "  xor " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";
        cout << "  seqz " << r_0 << cur << ", " << r_0 << cur << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    case KOOPA_RBO_GT:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }

        cout << "  sgt " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    case KOOPA_RBO_LT:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }

        cout << "  slt " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    case KOOPA_RBO_GE:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }

        cout << "  slt " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";
        cout << "  seqz " << r_0 << cur << ", " << r_0 << cur << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    case KOOPA_RBO_LE:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }

        cout << "  sgt " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";
        cout << "  seqz " << r_0 << cur << ", " << r_0 << cur << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    case KOOPA_RBO_ADD:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }

        cout << "  add " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    case KOOPA_RBO_SUB:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }

        cout << "  sub " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    case KOOPA_RBO_MUL:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }

        cout << "  mul " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    case KOOPA_RBO_DIV:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }

        cout << "  div " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    case KOOPA_RBO_MOD:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }

        cout << "  rem " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    case KOOPA_RBO_AND:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }

        cout << "  and " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    case KOOPA_RBO_OR:
    {
        bool reg_l = Reg_Select(binary.lhs);
        bool reg_r = Reg_Select(binary.rhs);

        // 这一步使用了几个寄存器？
        int cur = reg_num;
        if (reg_l && reg_r)
        {
            cur -= 2;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
            if (cur == -2)
            {
                cur = 5;
                r_0 = "t";
            }
        }
        else if (!reg_l && !reg_r)
        {
            cur = reg_num;
            reg_num++;
        }
        else
        {
            cur -= 1;
            if (cur == -1)
            {
                cur = 6;
                r_0 = "t";
            }
        }

        cout << "  or " << r_0 << cur << ", " << regs[binary.lhs] << ", " << regs[binary.rhs] << "\n";

        regs[value] = r_0 + to_string(cur);

        break;
    }
    }
}

void Visit(const koopa_raw_return_t &ret)
{
    if (ret.value->kind.tag == KOOPA_RVT_INTEGER)
    {
        cout << "  li a0, ";
        Visit(ret.value->kind.data.integer);
    }
    else
    {
        string ret_reg = regs[ret.value];
        cout << "  mv a0, " << ret_reg << "\n";
    }

    cout << "  ret\n";
}

void Visit(const koopa_raw_integer_t &integer)
{
    cout << integer.value << "\n";
}
// 访问对应类型指令的函数定义略
// 视需求自行实现
// ...
void parse_raw_program(const char *str)
{
    // 解析字符串 str, 得到 Koopa IR 程序
    koopa_program_t program;
    koopa_error_code_t ret = koopa_parse_from_string(str, &program);
    assert(ret == KOOPA_EC_SUCCESS); // 确保解析时没有出错
    // 创建一个 raw program builder, 用来构建 raw program
    koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
    // 将 Koopa IR 程序转换为 raw program
    koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
    // 释放 Koopa IR 程序占用的内存
    koopa_delete_program(program);

    // 处理 raw program
    // ...
    Visit(raw);
    // 处理完成, 释放 raw program builder 占用的内存
    // 注意, raw program 中所有的指针指向的内存均为 raw program builder 的内存
    // 所以不要在 raw program 处理完毕之前释放 builder
    koopa_delete_raw_program_builder(builder);
}
