<!-- @format -->

# 编译原理课程实践报告：玩具编译器

信息科学技术学院 2100013093 王子琪

## 一、编译器概述

### 1.1 基本功能

本编译器基本具备如下功能：

1. 功能 A：将 SysY 语言翻译为中间表示形式 (SysY -> AST -> Koopa IR)；
2. 功能 B：将 Koopa IR 转换为 RISC-V 汇编程序。

### 1.2 主要特点

+ **实现简单直观**：Koopa IR 以直观的文本形式呈现，用 koopa_str 表示，不涉及复杂的转换  
+ **低耦合**：前端直接将生成的 Koopa IR 传递给后端，没有其它数据的依赖  
+ **基本功能齐全**：能够通过所有测试用例，具备基本的正确性  
+ **可拓展性强**：构建 Koopa IR 使用了多态方法，便于在现有基础上拓展

## 二、编译器设计

### 2.1 主要模块组成

编译器主要分成如下四大模块：

+ **词法分析模块**: 通过词法分析，将 SysY 源程序转换为 token 流。(sysy.l)
+ **语法分析模块**: 通过语法分析，得到 AST.h 中定义的抽象语法树。(sysy.y)
+ **IR 生成模块**: 遍历抽象语法树，进行语义分析，得到 Koopa IR 中间表示。(AST.h)  
+ **代码生成模块**: 借 koopa 库将 Koopa IR 转换为 raw program，进一步转换为 RISC-V 代码。(riscv.[h|cpp])

### 2.2 主要数据结构

本编译器最核心的数据结构是 AST，其定义在 src/AST.h。语法分析器生成的 AST 即为整个 SysY 源程序的抽象语法树。如果将一个 SysY 程序视作一棵树，那么一个`class CompUnitAST`的实例就是这棵树的根，该树的每个结点都是一个 `class BaseAST` 的派生类的实例。

**抽象语法树**  `AST`

这里贴上BaseAST的定义，其中一些函数只在部分派生类里涉及，一并写在BaseAST里是为了调用方便。

```c
class BaseAST
{
public:
    virtual ~BaseAST() = default;

    /**
     * @brief 递归地生成Koopa IR
     * @return 用于表达式求值的优化，返回的bool值指示是否直接求出了表达式的值，int记录求出的值
     */
    virtual pair<bool, int> Koopa() const { return make_pair(false, -1); }
    virtual string name() const { return "oops"; }
    // 用于计算初始化的值
    // 必定能计算出来，否则是程序的问题
    virtual int calc() const { return 0; }
    virtual bool is_list() const { return false; }
};
```


**基本块处理器**  `BlockHandler`

+ 对每个基本块进行编号记录并管理  
+ 建立基本块之间的父子关系  
+ 维护基本块的完结情况

```c
class BlockHandler
{
public:
    // @block_cnt: 记录基本块总数，从0开始，用于编号
    int block_cnt;
    // @block_list: 保存当前所有基本块
    vector<Block_Unit> block_list;
    // @block_now: 当前所在基本块
    Block_Unit block_now;

    BlockHandler();
    bool is_end();
    void set_end();
    void set_not_end();
    void addBlock();
    void leaveBlock();
};
```

**符号表的值** `Value`

符号表 SymbolList 的设计详见 `2.3.1 符号表的设计考虑`  
符号表要存储符号对应的信息，为此我设计了一个数据结构Value，如下所示：

```c
enum TYPE
{
    CONSTANT,
    VAR,
    FUNC,
    ARRAY,
    POINTER
};

// 符号对应的值
// 对CONST类型，val : 常量的值
// 对VAR类型，val无实际意义
// 对FUNC类型，void->val = 0 ; int->val = 1
// 对ARRAY类型，val : 数组长度
// 对POINTER类型，val : []的数量
struct Value
{
    // @type: 符号的类型
    TYPE type;
    int val;
    // @name_index: 符号的命名序号，这里用的是符号所在的基本块号
    int name_index;
    Value() = default;
    Value(TYPE type_, int val_, int name_index_) : type(type_), val(val_), name_index(name_index_) {}
};
```

除此之外，为了代码编写的便利性，前端和后端部分都使用了一些全局变量。

```c
AST.h

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


riscv.h

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
```

### 2.3 主要设计考虑及算法选择

#### 2.3.1 符号表的设计考虑

符号表需要实现的功能主要可以归纳为：

1. 支持作用域之间的嵌套 --建立父子关系
2. 跨作用域的符号查询 --随机访问
3. 移除无效的作用域 --元素增删

实现思路如下：

>+ 建立一个结构体 SymbolList 管理符号表。  
>+ 基于以上要求，最终考虑使用 vector 来实现符号表，其中元素为单个作用域的符号表。  
>+ 对于单个作用域中的符号表，采用 unordered_map 实现，分别记录符号名和对应的值，值的含义在 2.2 数据结构中给出。  
>+ 进入/离开基本块时分别执行 push_back 和 pop_back 操作，对应了基本块之间的嵌套关系，并保证 vector 中的后一个符号表总是嵌套在前一个当中。  
>+ 而实现符号查询，只需要对当前 vector 进行后向遍历查询即可。

```c
class SymbolList
{
public:
    // @symbol_list_array: 符号表构成的vector 前后符号表有着嵌套关系
    vector<unordered_map<string, Value>> symbol_list_array;
    SymbolList() = default;

    /**
     * @brief Create a new symbol list
     */
    void newMap();

    /**
     * @brief Delete a disabled symbol list
     */
    void deleteMap();

    /**
     * @brief Add a symbol to the current symbol list
     * @param name The original name of the symbol
     * @param value Related information of the symbol
     */
    void addSymbol(string name, Value value);

    /**
     * @brief Finds the corresponding symbol by name
     * @param name The original name of the symbol
     * @return The value of the symbol
     */
    Value getSymbol(string name);
};
```


#### 2.3.2 寄存器分配策略

变量采用全部放在栈上的策略，进入函数时，计算所有语句以及变量等占用的空间大小，在栈上分配足够的空间。

每个语句确保自己执行的内部寄存器不会冲突，执行完毕将语句的结构存在栈上指定位置，寄存器只起到了中介的作用。

由于时间不足，我没有进行寄存器分配。我设想中一个简单可实现的优化策略是：  

1. 为寄存器设置两种状态 *0-空闲/当前变量不活跃* *1-当前变量活跃*
2. 搜索寄存器时，首先找状态值为 0 的寄存器，有则直接使用该寄存器；否则选择一个状态值为 1 的寄存器，将其原来的值保存回栈，并作为分配的寄存器。

#### 2.3.3 采用的优化策略

我使用的优化策略主要体现在 Koopa IR 当中，对于表达式的 Koopa IR 生成，我选择了最简化的方式，只要一个表达式能够被直接求值，就略去中间的所有计算过程，这样能有效提高简洁性，减少临时变量的使用，进一步提高 RISC-V 生成的简洁度。这一优化借助Koopa()函数的返回值来实现，类型为*pair<bool, int>*，对于可以直接计算出来的表达式，返回值为*make_pair(true, value)*。更详细的说明请参见 `3.1 Lv3表达式` 部分

#### 2.3.4 其它补充设计考虑

CompUnitAST中的DefList包含函数定义和变量/常量定义两种类型，而根据编译规则，变量/常量定义需要按照定义时的相对先后顺序排列，并且位于所有函数的前面，函数也需要维护相对顺序。  
为了实现这一效果，我最终考虑使用`双端队列deque`这一结构来实现DefList，在语法分析的环节，遇到变量/常量定义就push_front，遇到函数定义就push_back，这样能保证变量/常量和函数之间的先后关系，但根据语法分析的定义式还有一个问题，那就是函数会呈倒序排列，所以我采用了一个变量func_num来记录函数的个数，在CompUnitAST中调用Koopa遍历DefList时，先正序遍历变量/常量，再后序遍历函数，就能得到正确的顺序。

## 三、编译器实现

### 3.1 各阶段编码细节

#### Lv1. main 函数和 Lv2. 初试目标代码生成

该阶段基本只是按照文档完成。构建了AST的类结构，选择生成文本形式 Koopa 。

借助 void Koopa() 函数完成 Koopa IR 的生成，后面会意识到可以对此作出改进。

---

#### Lv3. 表达式

这部分的主要内容是利用 AST 的层次结构处理优先级，优先级较低的运算在语法树中层次较高。

由此，在对语法树递归处理时便能正确实现优先级。

写到这个部分我意识到，按照文档生成表达式的Koopa IR会出现许多冗余的计算，比如下面的示例：

```koopa
fun @main(): i32 {
%entry:
  %0 = eq 6, 0
  %1 = sub 0, %0
  %2 = sub 0, %1
  ret %2
}
```

完全可以被简化为

```koopa
fun @main(): i32 {
%entry:
  ret 0
}
```

实现这样的简化，需要对能计算出结果的表达式的值作前递处理，比如这里的 `+(- -!6)` ，不能逐步按照识别出的AST来生成 Koopa IR，要能直接把0这一计算结果传递给ret，同时ret要能够区分接收到的整数是否是返回值。  

由此，将 Koopa() 函数的返回值类型修改为 `pair<bool, int>` ，其中 bool 值表示返回的是否是一个可用的计算结果， int 值则记录计算结果，而对那些不需要计算的，或是计算结果不能直接计算出来，保存在临时变量%x里的情况，返回`make_pair(false, -1)`

处理ret的示例如下：

```c
if (exp != nullptr)
{
    pair<bool, int> res = exp->Koopa();
    if (res.first) // 返回了表达式的计算结果
    {
      koopa_str += "  ret " + to_string(res.second) + "\n";
    }
    else
    {
      koopa_str += "  ret %" + to_string(reg_cnt - 1) + "\n";
    }
}
```

而表达式处理的那些中间部分，只需要逐层返回 Koopa() 的结果即可，一个示例如下：

```c
class ExpAST : public BaseAST
{
public:
    unique_ptr<BaseAST> lorexp;

    pair<bool, int> Koopa() const override
    {
        return lorexp->Koopa();
    }
};
```

对于RISC-V代码，此时采用了把 Koopa 中的临时变量存入临时寄存器的策略，在某些 case 中会出现寄存器不足的错误，这一问题将在lv4中解决。

---

#### Lv4. 常量和变量


**前端部分**：  

对于常量，在AST中添加了 Cal c函数，用于求值。

加之变量的出现，添加了 SymbolList 类（符号表），作为全局信息管理。

这里出现了 LVal 这个非终结符号，表示变量/常量，而它作为左值和作为右值有着截然不同的作用，相应的 Koopa IR 也不一样，所以我用了两个AST来分别处理这两种情况，分别是 LeVal（左值）和 LVal（右值）

**后端部分**：

这里引入了内存分配，其实就是栈内存分配，实现了一种最简单的寄存器分配方式: 把所有变量都放在栈上。这种方式将贯穿始终。。

大致实现方法是，使用一个unordered_map，记录类型为 koopa_raw_value 的语句的返回值在栈中相对 sp 的位置。语句结束时将返回值存入栈中，此后别的语句以该语句为参数时再从栈中读入寄存器。

```c
unordered_map<koopa_raw_value_t, int> regs;

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
```

---

#### Lv5. 语句块和作用域

本章的改动在于 block 也可以作为 stmt，这就涉及到了作用域的问题，并且需要正确维护符号表。实际上，设计SymbolList时已经实现为类似栈的结构，能够处理嵌套的作用域。  
具体处理上，只要在 BlockAST 的 Koopa() 函数前后分别增删符号表即可。增删符号表的函数如下：

```c
void SymbolList::newMap()
{
    symbol_list_array.push_back(unordered_map<string, Value>());
}

void SymbolList::deleteMap()
{
    symbol_list_array.pop_back();
}
```

#### Lv6. if 语句

在if语句这一level，语法分析出现了二义性问题，导致else与if的匹配有歧义，我对二义性的处理比较简单，就是把增加一个非终结符 `If`，从而把 `IfStmt` 分成了两种模式：  
`If ELSE Stmt` 和 `If`， `If` 如下所示：

```y
If
  : IF '(' Exp ')' Stmt {
    auto ast = new IfAST();
    ast->exp = unique_ptr<BaseAST>($3);
    ast->stmt = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  ;
```

这样就能保证一旦遇到 `ELSE` ，它就会与前面相邻的 `If` 匹配，不过这样简单的设计就意味着要在生成 Koopa IR 时多下功夫。特别是对于 IfAST 的 Koopa()，需要识别出在调用时，是否有else的存在，所以我增加了一个全局变量if_end，没有else时标记为true，有else时标记为false。

同时，考虑到多个 IfStmt 嵌套的情况，当前的 if_end 值可能会在函数调用过程中改变，于是在函数内部设计一个bool值 now_if_end ，  通过 `bool now_if_end = if_end;` 确保一致性。

对于短路求值，由于我实现了表达式的简便求值，所以也比较易于处理。

```c
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
                // 如果不是0的话，直接忽略
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
```

---

#### Lv7. while 语句

WhileStmt本身实现不难，主要需要注意的是 while 之间可能会产生循环嵌套关系。为了在遇到 continue 或 break 时能确定当前应该跳到哪个 while 的对应标号，我使用了全局变量来维护循环嵌套树和当前语句在循环嵌套树中的位置。

```c
// lv7 while
// @loop_cnt: 当前循环总数，每次进入while递增
static int loop_cnt;
// @loop_dep: 循环深度，每次进入while递增，离开while递减
static int loop_dep;
// @find_loop: 在进入while时，记录关系find_loop[loop_cnt] = loop_dep
static unordered_map<int, int> find_loop;
```

---

#### Lv8. 函数和全局变量

**前端部分**：  

函数部分，主要关注形参和实参的区分、处理上。
用一个全局变量 param_block 来区分当前处理的参数是形参还是实参
同时 function_params 记录函数的形参，因为形参读取时还未进入函数体内部，而为了维护符号表的准确性，需要在进入函数体时才能把参数加入符号表。

```c
static bool param_block = false;
static vector<pair<string, int>> function_params; // 记录函数的形参，用于在函数体内部延迟加入符号表
```

全局变量部分，则要注意的是区分全局/非全局变量的 Koopa IR，在这里用一个全局 bool 变量 is_global 来完成。

**后端部分**：

生成目标代码比较复杂，但理解基本逻辑后，只需考虑以下几点：

按照文档正确生成函数的 prologue 和 epilogue。  
前8个参数放置在寄存器中，其他参数放置在栈中，函数在使用后面的参数时，要记得保存的位置是在调用者的栈。  
处理 ret 语句时 a0 sp 和 ra 的设置。

由于没有实现寄存器分配，所以无需考虑保存的问题。

---

#### Lv9. 数组

数组的处理一大问题在初始化上，由于有各种奇葩的初始化列表，所以第一步是要把初始化列表转换成填充完整的列表。在这里我实现了一个函数，借助递归调用得到完整的初始化列表。

```c
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
```

得到正确的初始化列表之后，再去初始化就没有什么困难了，依次调用 getelemptr 填入初值即可。值得一提的是对于未初始化的全局数组，最好使用zeroinit来初始化，以免在-perf测试时出现爆炸的情况。

当数组作为左值的时候，我们要根据其是否是函数参数来选择使用 getptr 还是 getelemptr，而数组作为右值时，也要根据其是否是函数参数、是指针还是整数，选择适当的指令。这就涉及解引用、部分解引用的概念。这里给出部分示例：

```c
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
```

目标代码部分，我认为最重要的是size的计算，其它部分不像Koopa IR需要考虑那么多。具体地，数组的size需要这样计算：

```c
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
```

### 3.2 工具软件介绍

1. `flex/bison`：编写sysy.l和sysy.y，完成词法分析和语法分析，通过表达式识别代码生成了语法分析树。
2. `libkoopa`：用于生成 Koopa IR 中间代码的结构，以便 RISC-V 目标代码的生成。

## 四、实习总结

### 4.1 收获和体会

实现了一个非常简单的、玩具式的编译器，虽然能够处理的代码比较有限，也还有很多优化没有来得及实现，但至少在这个过程中我理解了编译器的基本框架，对课程内容也有了更加深刻的认识，亲身体会了在编程过程中，最初的代码架构设计对后续功能扩充的影响，一个好的框架能让我们省去许多麻烦。此外，思考代码中可能存在的问题，逐步攻克黑盒测试点中的错误，这样的挑战也让人很有成就感。  

除此之外还有一点体会，那就是设计和实现编译器确实很不容易，稍有不慎就会出错，错误叠加起来又是更多的错误，对理论和实践方面的能力都是不小的考验。

### 4.2 学习过程中的难点，以及对实习过程和内容的建议

对我来说，实习过程中的难点主要体现在开始和结束的部分。

刚开始做lab的时候对课程的内容理解还不是很深，授课也还在前端部分，所以基本上是对着文档照猫画虎，不太了解自己在写些什么，所以写到后面，就发现前面写的代码结构比较乱，不方便管理。  

结束的部分也就是lv9数组的部分，个人感觉这部分应该是最难的部分，但是可能因为文档马上写完了，这部分涉及的指针等问题又比较抽象，所以文档写得有点草率，这也是人之常情，正如我的报告写到lv9部分反而变得简洁。但文档的简略就导致了我的悲剧，被各种解引用、部分解引用、指针等概念搞得很乱，Koopa里的各种类型也花了些时间搞清楚，希望这里的文档可以补充一些内容，或是多一些示例，给期末季增添一点温暖。
