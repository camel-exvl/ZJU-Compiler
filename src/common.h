#ifndef _COMMON_H_
#define _COMMON_H_

#include <string>
#include <vector>

const int SIZE_OF_INT = 4;
const int NUM_OF_REG = 32;
const std::vector<std::string> REGISTER_NAMES = {"zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
                                                 "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
                                                 "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
const std::vector<int> TEMP_REGISTERS = {5, 6, 7, 28, 29, 30, 31};
const std::vector<int> SAVED_REGISTERS = {8, 9, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};
const std::vector<int> ARG_REGISTERS = {10, 11, 12, 13, 14, 15, 16, 17};

const std::string TEXT =
    ".text\n\
_minilib_start:\n\
    la sp,_stack_top\n\
    call main\n\
    mv a1,a0\n\
    li a0,17\n\
    ecall\n\
read:\n\
    li a0,6\n\
    ecall\n\
    ret\n\
write:\n\
    mv a1,a0\n\
    li a0,1\n\
    ecall\n\
    ret\n";
const std::string DATA =
    ".data\n\
    .align 4\n\
_stack_start:\n\
.space 1145140\n\
_stack_top:\n\
.word 0\n";

#endif