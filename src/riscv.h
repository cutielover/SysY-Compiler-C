#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <cstring>
#include <unordered_map>
#include "koopa.h"
#include "assert.h"

void Visit(const koopa_raw_program_t &program);
void Visit(const koopa_raw_slice_t &slice);
void Visit(const koopa_raw_function_t &func);
void Visit(const koopa_raw_basic_block_t &bb);
void Visit(const koopa_raw_value_t &value);

void Visit(const koopa_raw_return_t &ret);
void Visit(const koopa_raw_integer_t &integer);

void Visit(const koopa_raw_binary_t &binary, const koopa_raw_value_t &value);

void Visit(const koopa_raw_store_t &store);
void Visit(const koopa_raw_load_t &load, const koopa_raw_value_t &value);

void Visit(const koopa_raw_branch_t &branch);
void Visit(const koopa_raw_jump_t &jump);

void parse_raw_program(const char *str);

int cal_func_size(const koopa_raw_function_t &func);
int cal_basic_block_size(const koopa_raw_basic_block_t &bb);
int cal_inst_size(const koopa_raw_value_t &inst);