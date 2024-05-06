#include "riscv.h"
using namespace std;
// 函数声明略
// ...

// 记录一条指令对应的寄存器
// lv4：目前暂时全部用于记录变量在栈帧中的位置，即offset(sp)
// 因为目前所有变量/指令都存放在栈帧里
unordered_map<koopa_raw_value_t, string> regs;
static int stack_offset = 0;

static int align_t = 16;
static int sp_size;

string get_stack_pos(const koopa_raw_value_t &value)
{
    if (regs.count(value))
    {
        return regs[value];
    }
    string stack_pos = to_string(stack_offset) + "(sp)";
    regs[value] = stack_pos;
    stack_offset += 4;
    return stack_pos;
}

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

// lv4完成
// 访问函数
void Visit(const koopa_raw_function_t &func)
{
    // 执行一些其他的必要操作
    // ...
    cout << "  .text\n";
    cout << "  .globl " << func->name + 1 << "\n";
    cout << func->name + 1 << ":\n";
    // 开辟栈帧
    sp_size = cal_func_size(func);
    // 16字节对齐
    sp_size = (sp_size + align_t - 1) & ~(align_t - 1);
    // 函数的prologue
    if (sp_size > 2048)
    {
        cout << "  li t0, " << -sp_size << "\n";
        cout << "  add sp, sp, t0\n";
    }
    else
    {
        cout << "  addi sp, sp, " << -sp_size << "\n";
    }
    // 访问所有基本块
    // 此时只有一个基本块
    Visit(func->bbs);
}

// lv4完成
// 访问基本块
void Visit(const koopa_raw_basic_block_t &bb)
{
    // 执行一些其他的必要操作
    // ...
    // 访问所有指令
    Visit(bb->insts);
}

// lv4完成
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
    case KOOPA_RVT_ALLOC:
        // 访问 alloc 指令
        break;
    case KOOPA_RVT_LOAD:
        // 访问 load 指令
        Visit(kind.data.load, value);
        break;
    case KOOPA_RVT_STORE:
        // 访问 store 指令
        Visit(kind.data.store);
        break;
    default:
        // 其他类型暂时遇不到
        assert(false);
    }
}

// lv4 :
// 把value放入临时寄存器reg
// 返回值含义：
// 实际使用的寄存器
string load_to_reg(const koopa_raw_value_t &value, const string &reg)
{
    // 0直接使用x0寄存器
    // 如果0在左边就不用x0，因为要确保左值的寄存器可用
    if (value->kind.tag == KOOPA_RVT_INTEGER && value->kind.data.integer.value == 0 && reg != "t0")
    {
        // regs[value] = "x0";
        return "x0";
    }

    // 非0整型，使用li指令加载
    else if (value->kind.tag == KOOPA_RVT_INTEGER)
    {
        // step1: 加载变量到名为reg的寄存器
        cout << "  li " << reg << ", ";
        Visit(value->kind.data.integer);
        cout << "\n";

        return reg;
    }

    // 变量 位于栈上
    else
    {
        cout << "  lw " << reg << ", ";
        cout << get_stack_pos(value);
        cout << "\n";
        return reg;
    }

    return reg;
}

// lv4完成
// binary指令
void Visit(const koopa_raw_binary_t &binary, const koopa_raw_value_t &value)
{
    switch (binary.op)
    {
    case KOOPA_RBO_NOT_EQ:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");

        cout << "  xor " << reg_l << ", " << reg_l << ", " << reg_r << "\n";
        cout << "  snez " << reg_l << ", " << reg_l << "\n";

        break;
    }
    case KOOPA_RBO_EQ:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");

        cout << "  xor " << reg_l << ", " << reg_l << ", " << reg_r << "\n";
        cout << "  seqz " << reg_l << ", " << reg_l << "\n";

        break;
    }
    case KOOPA_RBO_GT:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");

        cout << "  sgt " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_LT:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");

        cout << "  slt " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_GE:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");

        cout << "  slt " << reg_l << ", " << reg_l << ", " << reg_r << "\n";
        cout << "  seqz " << reg_l << ", " << reg_l << "\n";

        break;
    }
    case KOOPA_RBO_LE:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");

        cout << "  sgt " << reg_l << ", " << reg_l << ", " << reg_r << "\n";
        cout << "  seqz " << reg_l << ", " << reg_l << "\n";

        break;
    }
    case KOOPA_RBO_ADD:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");

        cout << "  add " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_SUB:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");

        cout << "  sub " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_MUL:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");

        cout << "  mul " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_DIV:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");

        cout << "  div " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_MOD:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");
        cout << "  rem " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_AND:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");

        cout << "  and " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_OR:
    {
        // 默认使用t0,t1
        string reg_l = load_to_reg(binary.lhs, "t0");
        string reg_r = load_to_reg(binary.rhs, "t1");

        cout << "  or " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    }
    // 结果保存到栈帧
    cout << "  sw t0, " << get_stack_pos(value) << "\n";
}

// store指令
void Visit(const koopa_raw_store_t &store)
{
    string reg = load_to_reg(store.value, "t2");
    cout << "  sw " << reg << ", " << get_stack_pos(store.dest) << "\n";
}

// load指令
void Visit(const koopa_raw_load_t &load, const koopa_raw_value_t &value)
{
    load_to_reg(load.src, "t0");
    cout << "  sw " << "t0" << ", " << get_stack_pos(value) << "\n";
}

// lv4完成
// ret指令
void Visit(const koopa_raw_return_t &ret)
{
    // 将返回值放入a0
    if (ret.value->kind.tag == KOOPA_RVT_INTEGER)
    {
        cout << "  li a0, ";
        Visit(ret.value->kind.data.integer);
        cout << "\n";
    }
    else
    {
        string ret_reg = regs[ret.value];
        cout << "  lw a0, " << ret_reg << "\n";
    }
    // 函数的epilogue
    if (sp_size >= 2048)
    {
        cout << "  li t0, " << sp_size << "\n";
        cout << "  add sp, sp, t0\n";
    }
    else
    {
        cout << "  addi sp, sp, " << sp_size << "\n";
    }
    // 返回
    cout << "  ret\n";
}

void Visit(const koopa_raw_integer_t &integer)
{
    cout << integer.value;
}

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

// 计算函数栈帧（未对齐）
int cal_func_size(const koopa_raw_function_t &func)
{
    int func_size = 0;
    for (uint32_t i = 0; i < func->bbs.len; i++)
    {
        const koopa_raw_basic_block_t block = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);
        func_size += cal_basic_block_size(block);
    }
    return func_size;
}

// 计算基本块栈帧
int cal_basic_block_size(const koopa_raw_basic_block_t &bb)
{
    int bb_size = 0;
    for (uint32_t i = 0; i < bb->insts.len; i++)
    {
        const koopa_raw_value_t inst = reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[i]);
        bb_size += cal_inst_size(inst);
    }
    return bb_size;
}

// 单条指令栈帧
int cal_inst_size(const koopa_raw_value_t &inst)
{
    switch (inst->ty->tag)
    {
    case KOOPA_RTT_INT32:
        return 4;
    case KOOPA_RTT_POINTER:
        return 4;
    case KOOPA_RTT_UNIT:
        return 0;
    default:
        break;
    }
    return 4;
}