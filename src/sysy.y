%code requires {
  #include <memory>
  #include <string>
  #include "AST.h"
}

%{

#include <iostream>
#include <memory>
#include <string>
#include <cstring>
#include <vector>
#include <deque>
#include "AST.h"

// 声明 lexer 函数和错误处理函数
int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

int funcnum = 0;

using namespace std;

%}

// 定义 parser 函数和错误处理函数的附加参数
// 我们需要返回一个字符串作为 AST, 所以我们把附加参数定义成字符串的智能指针
// 解析完成后, 我们要手动修改这个参数, 把它设置成解析得到的字符串
%parse-param { std::unique_ptr<BaseAST> &ast }

// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 因为 token 的值有的是字符串指针, 有的是整数
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 至于为什么要用字符串指针而不直接用 string 或者 unique_ptr<string>?
// 请自行 STFW 在 union 里写一个带析构函数的类会出现什么情况
%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
  deque<unique_ptr<BaseAST>> *ast_deq;
  vector<unique_ptr<BaseAST>> *ast_vec;
}

// lexer 返回的所有 token 种类的声明
// 注意 IDENT 和 INT_CONST 会返回 token 的值, 分别对应 str_val 和 int_val
%token INT RETURN CONST IF ELSE LE GE EQ NE AND OR WHILE BREAK CONTINUE
%token <str_val> IDENT
%token <int_val> INT_CONST
%token VOID

// 非终结符的类型定义
%type <ast_val> FuncDef Block Stmt If
%type <ast_val> Exp PrimaryExp UnaryExp AddExp MulExp LOrExp LAndExp EqExp RelExp
%type <ast_val> Decl ConstDecl ConstDef ConstDefList BlockItem LVal LeVal BlockItemList VarDecl VarDef InitVal VarDefList
%type <int_val> Number
%type <ast_deq> DefList
%type <ast_val> FuncFParams FuncFParam FuncRParams
%type <ast_vec> ArraySizeList InitValList
%%


CompUnit
  : DefList {
    auto comp_unit = make_unique<CompUnitAST>();
    comp_unit->DefList = unique_ptr<deque<unique_ptr<BaseAST>>>($1);
    comp_unit->func_num = funcnum;
    ast = move(comp_unit);
  }
  ;


DefList
  : FuncDef DefList {
    auto deq = (deque<unique_ptr<BaseAST>>*)($2);
    auto func = unique_ptr<BaseAST>($1);
    deq->push_back(move(func));
    funcnum++;
    $$ = deq;
  }
  | Decl DefList {
    auto deq = (deque<unique_ptr<BaseAST>>*)($2);
    auto decl = unique_ptr<BaseAST>($1);
    deq->push_front(move(decl));
    $$ = deq;
  }
  | FuncDef {
    auto deq = new deque<unique_ptr<BaseAST>>();
    auto func = unique_ptr<BaseAST>($1);
    deq->push_back(move(func));
    funcnum++;
    $$ = deq;
  }
  | Decl {
    auto deq = new deque<unique_ptr<BaseAST>>();
    auto decl = unique_ptr<BaseAST>($1);
    deq->push_front(move(decl));
    $$ = deq;
  }
  ;

FuncDef
  : INT IDENT '(' ')' Block {
    auto ast = new FuncDefAST();
    ast->type = "int";
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  | VOID IDENT '(' ')' Block {
    auto ast = new FuncDefAST();
    ast->type = "void";
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  | INT IDENT '(' FuncFParams ')' Block {
    auto ast = new FuncDefWithParamsAST();
    ast->type = "int";
    ast->ident = *unique_ptr<string>($2);
    ast->funcfparams = unique_ptr<BaseAST>($4);
    ast->block = unique_ptr<BaseAST>($6);
    $$ = ast;
  }
  | VOID IDENT '(' FuncFParams ')' Block {
    auto ast = new FuncDefWithParamsAST();
    ast->type = "void";
    ast->ident = *unique_ptr<string>($2);
    ast->funcfparams = unique_ptr<BaseAST>($4);
    ast->block = unique_ptr<BaseAST>($6);
    $$ = ast;
  }
  ;

FuncFParams
  : FuncFParam ',' FuncFParams {
    auto ast = new FuncFParamsAST();
    auto ast_params = unique_ptr<FuncFParamsAST>((FuncFParamsAST*)$3);
    ast->ParamList.emplace_back($1); 
    for(auto &i : ast_params->ParamList) {
      ast->ParamList.emplace_back(i.release());
    }
    $$ = ast;
  }
  | FuncFParam {
    auto ast = new FuncFParamsAST();
    ast->ParamList.emplace_back($1);
    $$ = ast;
  }
  ;

// lv9 done
FuncFParam
  : INT IDENT {
    auto ast = new FuncFParamAST();
    ast->type = "int";
    ast->name_ = *unique_ptr<string>($2);
    $$ = ast;
  } 
  | INT IDENT '[' ']' {
    auto ast = new FuncFParamAST();
    ast->type = "array";
    ast->name_ = *unique_ptr<string>($2);
    $$ = ast;
  } 
  | INT IDENT '[' ']' ArraySizeList {
    auto ast = new FuncFParamAST();
    ast->type = "array";
    ast->name_ = *unique_ptr<string>($2);
    vector<unique_ptr<BaseAST>> *v_ptr = ($5);
    for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
      ast->array_size_list.push_back(move(*it));
    $$ = ast;
  }
  ;

// lv9 done
ArraySizeList
  : '[' Exp ']' {
    vector<unique_ptr<BaseAST>> *v = new vector<unique_ptr<BaseAST>>;
    v->push_back(unique_ptr<BaseAST>($2));
    $$ = v;
  } 
  | ArraySizeList '[' Exp ']' {
    vector<unique_ptr<BaseAST>> *v = ($1);
    v->push_back(unique_ptr<BaseAST>($3));
    $$ = v;
  }
  ;

FuncRParams
  : Exp ',' FuncRParams {
    auto ast = new FuncRParamsAST();
    auto ast_params = unique_ptr<FuncRParamsAST>((FuncRParamsAST*)$3);
    ast->ParamList.emplace_back($1);
    for (auto &i : ast_params->ParamList){
      ast->ParamList.emplace_back(i.release());
    }
    $$ = ast;
  }
  | Exp {
    auto ast = new FuncRParamsAST();
    ast->ParamList.emplace_back($1);
    $$ = ast;
  }
  ;

/* // 同上, 不再解释
FuncType
  : INT {
    auto ast = new FuncTypeAST();
    ast->type = "int";
    $$ = ast;
  }
  | VOID {
    auto ast = new FuncTypeAST();
    ast->type = "void";
    $$ = ast;
  }
  ; */

Block
  : '{' BlockItemList '}' {
    $$ = $2;
  }
  ;

BlockItemList
  : {
    auto ast = new BlockAST();
    $$ = ast;
  }
  | BlockItem BlockItemList {
    auto ast = new BlockAST();
    auto ast_item_list = (BlockAST*)$2;
    ast->blockItemList.emplace_back($1);
    int n = ast_item_list->blockItemList.size();
    for(int i=0; i < n; i++) {
      ast->blockItemList.emplace_back(ast_item_list->blockItemList[i].release());
    }
    $$ = ast;
  }
  ;

BlockItem
  : Decl {
    auto ast = new BlockItemAST();
    ast->decl = unique_ptr<BaseAST>($1);
    ast->rule = 0;
    $$ = ast;
  }
  | Stmt {
    auto ast = new BlockItemAST();
    ast->stmt = unique_ptr<BaseAST>($1);
    ast->rule = 1;
    $$ = ast;
  }
  ;

Stmt
  : LeVal '=' Exp ';'{
    auto ast = new StmtAST();
    ast->leval = unique_ptr<BaseAST>($1);
    ast->exp = unique_ptr<BaseAST>($3);
    ast->rule = 0;
    $$ = ast;
  }
  | RETURN Exp ';' {
    auto ast = new StmtAST();
    ast->exp = unique_ptr<BaseAST>($2);
    ast->rule = 1;
    $$ = ast;
  }
  | RETURN ';' {
    auto ast = new StmtAST();
    ast->rule = 1;
    $$ = ast;
  }
  | Exp ';' {
    auto ast = new StmtAST();
    ast->exp = unique_ptr<BaseAST>($1);
    ast->rule = 2;
    $$ = ast;
  }
  | ';' {
    auto ast = new StmtAST();
    ast->rule = 2;
    $$ = ast;
  }
  | Block {
    auto ast = new StmtAST();
    ast->block = unique_ptr<BaseAST>($1);
    ast->rule = 3;
    $$ = ast;
  }
  | If ELSE Stmt{
    auto ast = new IfStmtAST();
    ast->if_stmt = unique_ptr<BaseAST>($1);
    ast->else_stmt = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | If {
    auto ast = new IfStmtAST();
    ast->if_stmt = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | WHILE '(' Exp ')' Stmt {
    auto ast = new WhileAST();
    ast->exp = unique_ptr<BaseAST>($3);
    ast->stmt = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  | CONTINUE ';' {
    auto ast = new LoopJumpAST();
    ast->rule = 1;
    $$ = ast;
  }
  | BREAK ';' {
    auto ast = new LoopJumpAST();
    ast->rule = 0;
    $$ = ast;
  }
  ;

If
  : IF '(' Exp ')' Stmt {
    auto ast = new IfAST();
    ast->exp = unique_ptr<BaseAST>($3);
    ast->stmt = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  ;

Number
  : INT_CONST {
    $$ = ($1);
  }
  ;


Exp
  : LOrExp {
    auto ast = new ExpAST();
    ast->lorexp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

PrimaryExp
  : '(' Exp ')' {
    auto ast = new PrimaryExpAST();
    ast -> rule = 0;
    ast -> exp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | Number {
    auto ast = new PrimaryExpAST();
    ast -> rule = 1;
    ast ->number = ($1);
    $$ = ast;
  }
  | LVal {
    auto ast = new PrimaryExpAST();
    ast->rule = 2;
    ast->lval = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;


UnaryExp
  : PrimaryExp {
    auto ast = new UnaryExpAST();
    ast -> rule = 0;
    ast -> primaryexp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | '+' UnaryExp {
    auto ast = new UnaryExpAST();
    ast -> rule = 1;
    ast -> op = "+";
    ast -> unaryexp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | '-' UnaryExp {
    auto ast = new UnaryExpAST();
    ast -> rule = 1;
    ast -> op = "-";
    ast -> unaryexp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | '!' UnaryExp {
    auto ast = new UnaryExpAST();
    ast -> rule = 1;
    ast -> op = "!";
    ast -> unaryexp = unique_ptr<BaseAST>($2);
    $$ = ast;
  }
  | IDENT '(' ')' {
    auto ast = new UnaryExpWithFuncAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT '(' FuncRParams ')' {
    auto ast = new UnaryExpWithFuncAST();
    ast->ident = *unique_ptr<string>($1);
    ast->funcrparams = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

MulExp
  : UnaryExp {
    auto ast = new MulExpAST();
    ast -> rule = 0;
    ast -> unaryexp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | MulExp '*' UnaryExp {
    auto ast = new MulExpAST();
    ast -> rule = 1;
    ast -> mulexp = unique_ptr<BaseAST>($1);
    ast -> op = "*";
    ast -> unaryexp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | MulExp '/' UnaryExp {
    auto ast = new MulExpAST();
    ast -> rule = 1;
    ast -> mulexp = unique_ptr<BaseAST>($1);
    ast -> op = "/";
    ast -> unaryexp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | MulExp '%' UnaryExp {
    auto ast = new MulExpAST();
    ast -> rule = 1;
    ast -> mulexp = unique_ptr<BaseAST>($1);
    ast -> op = "%";
    ast -> unaryexp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

AddExp
  : MulExp {
    auto ast = new AddExpAST();
    ast -> rule = 0;
    ast -> mulexp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | AddExp '+' MulExp {
    auto ast = new AddExpAST();
    ast -> rule = 1;
    ast -> addexp = unique_ptr<BaseAST>($1);
    ast -> op = "+";
    ast -> mulexp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | AddExp '-' MulExp {
    auto ast = new AddExpAST();
    ast -> rule = 1;
    ast -> addexp = unique_ptr<BaseAST>($1);
    ast -> op = "-";
    ast -> mulexp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

RelExp
  : AddExp {
    auto ast = new RelExpAST();
    ast -> rule = 0;
    ast -> addexp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | RelExp '<' AddExp {
    auto ast = new RelExpAST();
    ast -> rule = 1;
    ast -> relexp = unique_ptr<BaseAST>($1);
    ast -> op = "<";
    ast -> addexp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | RelExp '>' AddExp {
    auto ast = new RelExpAST();
    ast -> rule = 1;
    ast -> relexp = unique_ptr<BaseAST>($1);
    ast -> op = ">";
    ast -> addexp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | RelExp LE AddExp {
    auto ast = new RelExpAST();
    ast -> rule = 1;
    ast -> relexp = unique_ptr<BaseAST>($1);
    ast -> op = "<=";
    ast -> addexp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | RelExp GE AddExp {
    auto ast = new RelExpAST();
    ast -> rule = 1;
    ast -> relexp = unique_ptr<BaseAST>($1);
    ast -> op = ">=";
    ast -> addexp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  ;

EqExp
  : RelExp {
    auto ast = new EqExpAST();
    ast -> rule = 0;
    ast -> relexp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | EqExp EQ RelExp {
    auto ast = new EqExpAST();
    ast -> rule = 1;
    ast -> eqexp = unique_ptr<BaseAST>($1);
    ast -> op = "==";
    ast -> relexp = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | EqExp NE RelExp {
    auto ast = new EqExpAST();
    ast -> rule = 1;
    ast -> eqexp = unique_ptr<BaseAST>($1);
    ast -> op = "!=";
    ast -> relexp = unique_ptr<BaseAST>($3);
    $$ = ast;   
  }
  ;

LAndExp
  : EqExp {
    auto ast = new LAndExpAST();
    ast -> rule = 0;
    ast -> eqexp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LAndExp AND EqExp {
    auto ast = new LAndExpAST();
    ast -> rule = 1;
    ast -> landexp = unique_ptr<BaseAST>($1);
    ast -> eqexp = unique_ptr<BaseAST>($3);
    $$ = ast; 
  }
  ;

LOrExp
  : LAndExp {
    auto ast = new LOrExpAST();
    ast -> rule = 0;
    ast -> landexp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | LOrExp OR LAndExp {
    auto ast = new LOrExpAST();
    ast -> rule = 1;
    ast -> lorexp = unique_ptr<BaseAST>($1);
    ast -> landexp = unique_ptr<BaseAST>($3);
    $$ = ast; 
  }
  ;

Decl
  : ConstDecl {
    auto ast = new DeclAST();
    ast->rule = 0;
    ast->constdecl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | VarDecl{
    auto ast = new DeclAST();
    ast->rule = 1;
    ast->vardecl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

ConstDecl
  : CONST INT ConstDefList ';' {
    auto ast = $3;
    $$ = ast;
  }
  ;

ConstDefList
  : ConstDef ',' ConstDefList {
    auto ast = new ConstDeclAST();
    auto ast_def = (ConstDeclAST* )$3;
    ast->constDefList.emplace_back($1); 
    int n = ast_def->constDefList.size();
    for(int i = 0; i < n; i++) {
      ast->constDefList.emplace_back(ast_def->constDefList[i].release());
    }
    $$ = ast;
  }
  | ConstDef {
    auto ast = new ConstDeclAST();
    ast->constDefList.emplace_back($1);
    $$ = ast;
  }
  ;

// lv9 done
ConstDef
  : IDENT '=' InitVal {
    auto ast = new ConstDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->constinitval = unique_ptr<BaseAST>($3);
    $$ = ast;
  } 
  | IDENT ArraySizeList '=' InitVal {
    auto ast = new ConstDefArrayAST();
    ast->ident = *unique_ptr<string>($1);
    ast->constinitval = unique_ptr<BaseAST>($4);
    vector<unique_ptr<BaseAST>> *v_ptr = ($2);
    for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
      ast->array_size_list.push_back(move(*it));
    $$ = ast;
  }
  ;




LVal 
  : IDENT {
    auto ast = new LValAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT ArraySizeList {
    auto ast = new LValArrayAST();
    ast->ident = *unique_ptr<string>($1);
    vector<unique_ptr<BaseAST>> *v_ptr = ($2);
    for(auto it = v_ptr->begin(); it != v_ptr->end(); it++)
      ast->array_size_list.push_back(move(*it));
    $$ = ast;
  }
  ;

LeVal
  : IDENT{
    auto ast = new LeValAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT ArraySizeList {
    auto ast = new LeValArrayAST();
    ast->ident = *unique_ptr<string>($1);
    vector<unique_ptr<BaseAST>> *v_ptr = ($2);
    for(auto it = v_ptr->begin(); it != v_ptr->end(); it++)
      ast->array_size_list.push_back(move(*it));
    $$ = ast;
  }
  ;

VarDecl
  : INT VarDefList ';' {
    $$ = $2;
  }
  ;

VarDefList
  : VarDef ',' VarDefList {
    auto ast = new VarDeclAST();
    auto ast_def = (VarDeclAST* )$3;
    ast->varDefList.emplace_back($1); 
    int n = ast_def->varDefList.size();
    for(int i = 0; i < n; i++) {
      ast->varDefList.emplace_back(ast_def->varDefList[i].release());
    }
    $$ = ast;
  }
  | VarDef {
    auto ast = new VarDeclAST();
    ast->varDefList.emplace_back($1);
    $$ = ast;
  }
  ;

// lv9 done
VarDef
  : IDENT {
    auto ast = new VarDefAST();
    ast->rule = 0;
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT '=' InitVal {
    auto ast = new VarDefAST();
    ast->rule = 1;
    ast->ident = *unique_ptr<string>($1);
    ast->initval = unique_ptr<BaseAST>($3);
    $$ = ast;
  }
  | IDENT ArraySizeList {
    auto ast = new VarDefArrayAST();
    ast->rule = 0;
    ast->ident = *unique_ptr<string>($1);
    vector<unique_ptr<BaseAST>> *v_ptr = ($2);
    for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
      ast->array_size_list.push_back(move(*it));
    $$ = ast;
  }
  | IDENT ArraySizeList '=' InitVal {
    auto ast = new VarDefArrayAST();
    ast->rule = 1;
    ast->ident = *unique_ptr<string>($1);
    ast->init_val = unique_ptr<BaseAST>($4);
    vector<unique_ptr<BaseAST>> *v_ptr = ($2);
    for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
      ast->array_size_list.push_back(move(*it));
    $$ = ast;
  }
  ;

// lv9 done
InitVal
  : Exp {
    auto ast = new InitValAST();
    ast->exp = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | '{' '}' {
    auto ast = new InitValWithListAST();
    $$ = ast;
  }
  | '{' InitValList '}' {
    auto ast = new InitValWithListAST();
    vector<unique_ptr<BaseAST>> *v_ptr = ($2);
    for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
      ast->init_val_list.push_back(move(*it));
    $$ = ast;  
  }
  ;


// lv9 done
InitValList
  : InitVal {
    vector<unique_ptr<BaseAST>> *v = new vector<unique_ptr<BaseAST>>;
    v->push_back(unique_ptr<BaseAST>($1));
    $$ = v;
  }
  | InitValList ',' InitVal {
    vector<unique_ptr<BaseAST>> *v = ($1);
    v->push_back(unique_ptr<BaseAST>($3));
    $$ = v;
  }
  ;


%%

// 定义错误处理函数, 其中第二个参数是错误信息
// parser 如果发生错误 (例如输入的程序出现了语法错误), 就会调用这个函数
void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
    extern int yylineno;    
    extern char *yytext; 
    int len=strlen(yytext);
    int i;
    char buf[512]={0};
    for (i=0;i<len;++i)
    {
        sprintf(buf,"%s%d ",buf,yytext[i]);
    }
    fprintf(stderr, "ERROR: %s at symbol '%s' on line %d\n", s, buf, yylineno);
}