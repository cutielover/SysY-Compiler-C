#include "riscv.h"
using namespace std;
// 函数声明略
// ...

// 记录一条指令对应的寄存器
// lv4：目前暂时全部用于记录变量在栈帧中的位置，即offset(sp)
// 因为目前所有变量/指令都存放在栈帧里
unordered_map<koopa_raw_value_t, int> regs;
static int stack_offset = 0;

static int align_t = 16;
static int sp_size;

// lv8
bool has_call;
int max_stack_arg;

// 返回变量在栈上的偏移量
int get_stack_pos(const koopa_raw_value_t &value)
{
    if (regs.count(value))
    {
        return regs[value];
    }
    regs[value] = stack_offset;
    stack_offset += cal_inst_size(value);
    return regs[value];
}

// lv8
// 访问 raw program
void Visit(const koopa_raw_program_t &program)
{
    // 执行一些其他的必要操作
    // ...
    // 访问所有全局变量
    if (program.values.len != 0)
    {
        cout << "  .data";
        Visit(program.values);
    }
    // 访问所有函数
    cout << "\n  .text";
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

// lv8
// lv4完成
// 访问函数
void Visit(const koopa_raw_function_t &func)
{
    // 执行一些其他的必要操作
    // ...
    has_call = false;
    max_stack_arg = 0;
    stack_offset = 0;
    // 跳过库函数
    if (func->bbs.len == 0)
    {
        return;
    }
    cout << "\n  .globl " << func->name + 1 << "\n";
    cout << func->name + 1 << ":\n";

    // 开辟栈帧
    sp_size = cal_func_size(func);
    // 16字节对齐
    sp_size = (sp_size + align_t - 1) & ~(align_t - 1);
    // 函数的prologue
    if (sp_size >= 2048)
    {
        cout << "  li t0, -" << sp_size << "\n";
        cout << "  add sp, sp, t0\n";
    }
    else
    {
        cout << "  addi sp, sp, -" << sp_size << "\n";
    }
    if (has_call)
    {
        if (sp_size - 4 >= 2048)
        {
            cout << "  li t0, " << sp_size - 4 << "\n";
            cout << "  add t0, t0, sp\n";
            cout << "  sw ra, 0(t0)\n";
        }
        else
        {
            cout << "  sw ra, " << to_string(sp_size - 4) << "(sp)\n";
        }
    }
    Visit(func->bbs);
}

// lv4完成
// 访问基本块
void Visit(const koopa_raw_basic_block_t &bb)
{
    // 执行一些其他的必要操作
    // ...
    // 访问所有指令
    if (strcmp(bb->name + 1, "entry"))
    {
        cout << bb->name + 1 << ":\n";
    }
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
    case KOOPA_RVT_BRANCH:
        // 访问 branch 指令
        Visit(kind.data.branch);
        break;
    case KOOPA_RVT_JUMP:
        // 访问 jump 指令
        Visit(kind.data.jump);
        break;
    case KOOPA_RVT_CALL:
        Visit(kind.data.call, value);
        break;
    case KOOPA_RVT_GLOBAL_ALLOC:
        Visit(kind.data.global_alloc, value);
        break;
    case KOOPA_RVT_GET_ELEM_PTR:
        Visit(kind.data.get_elem_ptr, value);
        break;
    case KOOPA_RVT_GET_PTR:
        Visit(kind.data.get_ptr, value);
        break;
    default:
        // 其他类型暂时遇不到
        assert(false);
    }
}

// lv8
// 把value放入临时寄存器reg
// 返回值含义：
// 实际使用的寄存器
string load_to_reg(const koopa_raw_value_t &value, const string &reg)
{

    // 0直接使用x0寄存器
    // 如果0在左边就不用x0，因为要确保左值的寄存器可用
    // if (value->kind.tag == KOOPA_RVT_INTEGER && value->kind.data.integer.value == 0 && reg != "t0")
    // {
    //     // regs[value] = "x0";
    //     return "x0";
    // }

    // 非0整型，使用li指令加载
    if (value->kind.tag == KOOPA_RVT_INTEGER)
    {
        // step1: 加载变量到名为reg的寄存器
        cout << "  li " << reg << ", ";
        Visit(value->kind.data.integer);
        cout << "\n";

        return reg;
    }

    // 参数
    else if (value->kind.tag == KOOPA_RVT_FUNC_ARG_REF)
    {
        int idx = value->kind.data.func_arg_ref.index;
        // 寄存器里的参数
        if (idx < 8)
        {
            return "a" + to_string(idx);
        }
        // 栈上的参数
        else
        {
            int stack_o = sp_size + 4 * (idx - 8);
            if (stack_o >= 2048 || stack_o < -2048)
            {
                cout << "  li t3, " << stack_o << "\n";
                cout << "  add t3, t3, sp\n";
                cout << "  lw " << reg << ", 0(t3)\n";
            }
            else
            {
                cout << "  lw " << reg << ", ";
                cout << stack_o << "(sp)\n";
            }
            // cout << "  lw " << reg << ", " << to_string(sp_size + 4 * (idx - 8)) << "(sp)\n";
            return reg;
        }
    }

    // 全局变量
    // 先获取变量所在地址
    // 然后把地址中的变量加载到寄存器
    else if (value->kind.tag == KOOPA_RVT_GLOBAL_ALLOC)
    {
        cout << "  la " << reg << ", " << value->name + 1 << "\n";
        cout << "  lw " << reg << ", 0(" << reg << ")\n";
        return reg;
    }

    // 变量 位于栈上
    else
    {
        int stack_pos = get_stack_pos(value);
        if (stack_pos >= 2048 || stack_pos < -2048)
        {
            cout << "  li t3, " << stack_pos << "\n";
            cout << "  add t3, t3, sp\n";
            cout << "  lw " << reg << ", 0(t3)\n";
        }
        else
        {
            cout << "  lw " << reg << ", ";
            cout << stack_pos << "(sp)\n";
        }
        return reg;
    }

    return reg;
}

// lv4完成
// binary指令
void Visit(const koopa_raw_binary_t &binary, const koopa_raw_value_t &value)
{
    string reg_l = load_to_reg(binary.lhs, "t0");
    string reg_r = load_to_reg(binary.rhs, "t1");
    switch (binary.op)
    {
    case KOOPA_RBO_NOT_EQ:
    {
        // 默认使用t0,t1

        cout << "  xor " << reg_l << ", " << reg_l << ", " << reg_r << "\n";
        cout << "  snez " << reg_l << ", " << reg_l << "\n";

        break;
    }
    case KOOPA_RBO_EQ:
    {
        // 默认使用t0,t1

        cout << "  xor " << reg_l << ", " << reg_l << ", " << reg_r << "\n";
        cout << "  seqz " << reg_l << ", " << reg_l << "\n";

        break;
    }
    case KOOPA_RBO_GT:
    {
        // 默认使用t0,t1

        cout << "  sgt " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_LT:
    {
        // 默认使用t0,t1

        cout << "  slt " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_GE:
    {
        // 默认使用t0,t1

        cout << "  slt " << reg_l << ", " << reg_l << ", " << reg_r << "\n";
        cout << "  seqz " << reg_l << ", " << reg_l << "\n";

        break;
    }
    case KOOPA_RBO_LE:
    {
        // 默认使用t0,t1

        cout << "  sgt " << reg_l << ", " << reg_l << ", " << reg_r << "\n";
        cout << "  seqz " << reg_l << ", " << reg_l << "\n";

        break;
    }
    case KOOPA_RBO_ADD:
    {
        // 默认使用t0,t1

        cout << "  add " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_SUB:
    {
        // 默认使用t0,t1
        cout << "  sub " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_MUL:
    {
        // 默认使用t0,t1

        cout << "  mul " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_DIV:
    {
        // 默认使用t0,t1

        cout << "  div " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_MOD:
    {
        // 默认使用t0,t1
        cout << "  rem " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_AND:
    {
        // 默认使用t0,t1

        cout << "  and " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    case KOOPA_RBO_OR:
    {
        // 默认使用t0,t1

        cout << "  or " << reg_l << ", " << reg_l << ", " << reg_r << "\n";

        break;
    }
    }
    // 结果保存到栈帧
    int stack_pos = get_stack_pos(value);
    if (stack_pos >= 2048 || stack_pos < -2048)
    {
        cout << "  li t4, " << stack_pos << "\n";
        cout << "  add t4, t4, sp\n";
        cout << "  sw t0, 0(t4)\n";
    }
    else
    {
        cout << "  sw t0, " << stack_pos << "(sp)\n";
    }
}

// lv8: global alloc
// store指令
void Visit(const koopa_raw_store_t &store)
{
    if (store.dest->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) // destination:全局变量
    {
        cout << "  la t1, " << store.dest->name + 1 << "\n";
        load_to_reg(store.value, "t0");
        cout << "  sw t0, 0(t1)\n";
    }
    else if (store.dest->kind.tag == KOOPA_RVT_GET_ELEM_PTR || store.dest->kind.tag == KOOPA_RVT_GET_PTR)
    {
        load_to_reg(store.dest, "t1");
        load_to_reg(store.value, "t0");
        cout << "  sw t0, 0(t1)\n";
    }
    else // destination: 栈
    {
        string reg = load_to_reg(store.value, "t0");
        int stack_pos = get_stack_pos(store.dest);
        if (stack_pos >= 2048 || stack_pos < -2048)
        {
            cout << "  li t4, " << stack_pos << "\n";
            cout << "  add t4, t4, sp\n";
            cout << "  sw " << reg << ", 0(t4)\n";
        }
        else
        {
            cout << "  sw " << reg << ", " << stack_pos << "(sp)\n";
        }
    }
}

// load指令
void Visit(const koopa_raw_load_t &load, const koopa_raw_value_t &value)
{
    string reg = load_to_reg(load.src, "t0");

    if (load.src->kind.tag == KOOPA_RVT_GET_ELEM_PTR || load.src->kind.tag == KOOPA_RVT_GET_PTR)
    {
        cout << "  lw " << reg << ", 0(" << reg << ")\n";
    }

    int stack_pos = get_stack_pos(value);
    if (stack_pos >= 2048 || stack_pos < -2048)
    {
        cout << "  li t3, " << stack_pos << "\n";
        cout << "  add t3, t3, sp\n";
        cout << "  sw " << reg << ", 0(t3)\n";
    }
    else
    {
        cout << "  sw " << reg << ", " << stack_pos << "(sp)\n";
    }
}

// ret指令
void Visit(const koopa_raw_return_t &ret)
{
    // 将返回值放入a0
    if (ret.value != nullptr)
    {
        if (ret.value->kind.tag == KOOPA_RVT_INTEGER)
        {
            cout << "  li a0, ";
            Visit(ret.value->kind.data.integer);
            cout << "\n";
        }
        else
        {
            int stack_pos = regs[ret.value];
            if (stack_pos >= 2048 || stack_pos < -2048)
            {
                cout << "  li t0, " << stack_pos << "\n";
                cout << "  add t0, t0, sp\n";
                cout << "  lw a0, 0(t0)\n";
            }
            else
            {
                cout << "  lw a0, " << stack_pos << "(sp)\n";
            }
        }
    }
    // 函数的epilogue

    // 恢复ra
    if (has_call)
    {
        if (sp_size - 4 >= 2048)
        {
            cout << "  li t0, " << sp_size - 4 << "\n";
            cout << "  add t0, t0, sp\n";
            cout << "  lw ra, 0(t0)\n";
        }
        else
        {
            cout << "  lw ra, " << to_string(sp_size - 4) << "(sp)\n";
        }
    }

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

// lv9 条件跳转范围问题
// simple solution: bnez只跳转到相邻的两个jump指令，统一用jump指令
// lv6 branch指令
void Visit(const koopa_raw_branch_t &branch)
{
    string reg_branch = load_to_reg(branch.cond, "t0");
    cout << "  bnez " << reg_branch << ", " << (branch.true_bb->name + 1) << "_tmp\n";
    cout << "  j " << branch.false_bb->name + 1 << "\n";
    cout << branch.true_bb->name + 1 << "_tmp:\n";
    cout << "  j " << branch.true_bb->name + 1 << "\n";
}

// lv6 jump指令
void Visit(const koopa_raw_jump_t &jump)
{
    cout << "  j " << jump.target->name + 1 << "\n";
}

// lv8 call
void Visit(const koopa_raw_call_t &call, const koopa_raw_value_t &value)
{

    int args_num = call.args.len;
    for (int i = 0; i < min(args_num, 8); i++)
    {
        const koopa_raw_value_t arg = reinterpret_cast<koopa_raw_value_t>(call.args.buffer[i]);
        load_to_reg(arg, "a" + to_string(i));
    }

    // 保存在调用者栈帧
    if (args_num > 8)
    {
        for (int i = 8; i < args_num; i++)
        {
            const koopa_raw_value_t arg = reinterpret_cast<koopa_raw_value_t>(call.args.buffer[i]);
            load_to_reg(arg, "t0");
            cout << "  sw t0, " << to_string((i - 8) * 4) + "(sp)\n";
        }
    }

    cout << "  call " << call.callee->name + 1 << "\n";

    if (value->ty->tag != KOOPA_RTT_UNIT)
    {
        int stack_pos = get_stack_pos(value);
        if (stack_pos >= 2048 || stack_pos < -2048)
        {
            cout << "  li t4, " << stack_pos << "\n";
            cout << "  add t4, t4, sp\n";
            cout << "  sw " << "a0" << ", 0(t4)\n";
        }
        else
        {
            cout << "  sw a0, " << stack_pos << "(sp)\n";
        }
    }
}

// lv9 aggregate unfinished
// lv8 global variable
void Visit(const koopa_raw_global_alloc_t &global, const koopa_raw_value_t &value)
{
    cout << "\n  .globl " << value->name + 1 << "\n";
    cout << value->name + 1 << ":\n";
    if (global.init->kind.tag == KOOPA_RVT_ZERO_INIT)
    {
        cout << "  .zero " << cal_type_size(value->ty->data.pointer.base) << "\n";
    }
    else if (global.init->kind.tag == KOOPA_RVT_INTEGER)
    {
        cout << "  .word " << global.init->kind.data.integer.value << "\n";
    }
    else
    { // aggregate
        Visit(global.init->kind.data.aggregate);
    }
}

// lv9 aggregate_init
void Visit(const koopa_raw_aggregate_t &aggregate)
{
    for (size_t i = 0; i < aggregate.elems.len; ++i)
    {
        auto ptr = aggregate.elems.buffer[i];
        koopa_raw_value_t value = reinterpret_cast<koopa_raw_value_t>(ptr);
        if (value->kind.tag == KOOPA_RVT_INTEGER)
        {
            cout << "  .word " << value->kind.data.integer.value << "\n";
        }
        else if (value->kind.tag == KOOPA_RVT_AGGREGATE)
        {
            Visit(value->kind.data.aggregate);
        }
        else
            assert(false);
    }
}
// lv9 get_elem_ptr
void Visit(const koopa_raw_get_elem_ptr_t &get_elem_ptr, const koopa_raw_value_t &value)
{
    if (get_elem_ptr.src->kind.tag == KOOPA_RVT_GLOBAL_ALLOC)
    {
        cout << "  la t0, " << get_elem_ptr.src->name + 1 << "\n";
    }
    else
    {
        int src_stack_pos = get_stack_pos(get_elem_ptr.src);

        // t0存放src的地址 sp + src_stack_pos
        if (src_stack_pos < 2048 && src_stack_pos >= -2048)
            cout << "  addi t0, sp, " << src_stack_pos << "\n";
        else
        {
            cout << "  li t3, " << src_stack_pos << "\n";
            cout << "  add t0, sp, t3\n";
        }

        // 指针操作 进一步把值load到t0
        if (get_elem_ptr.src->kind.tag == KOOPA_RVT_GET_ELEM_PTR ||
            get_elem_ptr.src->kind.tag == KOOPA_RVT_GET_PTR)
        {
            cout << "  lw t0, 0(t0)\n";
        }
    }
    string reg = load_to_reg(get_elem_ptr.index, "t1");
    int size = cal_type_size(get_elem_ptr.src->ty->data.pointer.base->data.array.base);
    cout << "  li t2, " << size << "\n";
    cout << "  mul t1, t1, t2\n";
    cout << "  add t0, t0, t1\n";

    int stack_pos = get_stack_pos(value);
    if (stack_pos >= 2048 || stack_pos < -2048)
    {
        cout << "  li t4, " << stack_pos << "\n";
        cout << "  add t4, t4, sp\n";
        cout << "  sw " << "t0" << ", 0(t4)\n";
    }
    else
    {
        cout << "  sw " << "t0" << ", " << stack_pos << "(sp)\n";
    }
}

// lv9 get_ptr
void Visit(const koopa_raw_get_ptr_t &get_ptr, const koopa_raw_value_t &value)
{
    int src_stack_pos = get_stack_pos(get_ptr.src);

    // t0存放src的地址 sp + src_stack_pos
    if (src_stack_pos < 2048 && src_stack_pos >= -2048)
        cout << "  addi t0, sp, " << src_stack_pos << "\n";
    else
    {
        cout << "  li t3, " << src_stack_pos << "\n";
        cout << "  add t0, sp, t3\n";
    }

    cout << "  lw t0, 0(t0)\n";
    load_to_reg(get_ptr.index, "t1");
    int size = cal_type_size(get_ptr.src->ty->data.pointer.base);
    cout << "  li t2, " << size << "\n";
    cout << "  mul t1, t1, t2\n";
    cout << "  add t0, t0, t1\n";
    int stack_pos = get_stack_pos(value);
    if (stack_pos >= 2048 || stack_pos < -2048)
    {
        cout << "  li t4, " << stack_pos << "\n";
        cout << "  add t4, t4, sp\n";
        cout << "  sw " << "t0" << ", 0(t4)\n";
    }
    else
    {
        cout << "  sw " << "t0" << ", " << stack_pos << "(sp)\n";
    }
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

// lv8
// 考虑call
// 计算函数栈帧（未对齐）
int cal_func_size(const koopa_raw_function_t &func)
{
    int func_size = 0;
    for (uint32_t i = 0; i < func->bbs.len; i++)
    {
        const koopa_raw_basic_block_t block = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);
        func_size += cal_basic_block_size(block);
    }
    // ra寄存器
    if (has_call)
    {
        func_size += 4;
    }
    // caller参数栈
    func_size += max_stack_arg * 4;

    stack_offset += max_stack_arg * 4;
    // 需要吗？
    //  func_size += func->params.len;

    return func_size;
}

// lv8
// 计算基本块栈帧
int cal_basic_block_size(const koopa_raw_basic_block_t &bb)
{
    int bb_size = 0;
    for (uint32_t i = 0; i < bb->insts.len; i++)
    {
        const koopa_raw_value_t inst = reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[i]);
        if (inst->kind.tag == KOOPA_RVT_CALL)
        {
            has_call = true;
            max_stack_arg = max(max_stack_arg, (int)(inst->kind.data.call.args.len - 8));
        }
        bb_size += cal_inst_size(inst);
    }
    return bb_size;
}

// 单条指令栈帧
int cal_inst_size(const koopa_raw_value_t &inst)
{
    if (inst->kind.tag == KOOPA_RVT_ALLOC)
    {
        return cal_type_size(inst->ty->data.pointer.base);
    }
    else
    {
        return cal_type_size(inst->ty);
    }
}

// 类型大小
int cal_type_size(const koopa_raw_type_t &ty)
{
    switch (ty->tag)
    {
    case KOOPA_RTT_INT32:
        return 4;
    case KOOPA_RTT_UNIT:
        return 0;
    case KOOPA_RTT_POINTER:
        return 4;
    case KOOPA_RTT_ARRAY:
        return ty->data.array.len * cal_type_size(ty->data.array.base);
    default:
        break;
    }
    return 0;
}