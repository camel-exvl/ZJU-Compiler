#include "assembly.h"
#include <string.h>
#include <cstdarg>

extern FILE* outputFile;

static void printToFile(FILE* file, const char* format, ...) {
    if (format[strlen(format) - 2] != ':') {
        fprintf(file, "    ");
    }
    
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
}

void BinaryAssembly::print() {
    switch (op[0]) {
        case '+':
            printToFile(outputFile, "add %s, %s, %s\n", lhs.name.c_str(), rhs1.name.c_str(), rhs2.name.c_str());
            break;
        case '-':
            printToFile(outputFile, "sub %s, %s, %s\n", lhs.name.c_str(), rhs1.name.c_str(), rhs2.name.c_str());
            break;
        case '*':
            printToFile(outputFile, "mul %s, %s, %s\n", lhs.name.c_str(), rhs1.name.c_str(), rhs2.name.c_str());
            break;
        case '/':
            printToFile(outputFile, "div %s, %s, %s\n", lhs.name.c_str(), rhs1.name.c_str(), rhs2.name.c_str());
            break;
        case '%':
            printToFile(outputFile, "rem %s, %s, %s\n", lhs.name.c_str(), rhs1.name.c_str(), rhs2.name.c_str());
            break;
        default:
            throw std::runtime_error("Invalid binary operator");
    }
}

void BinaryImmAssembly::print() {
    switch (op[0]) {
        case '+':
            printToFile(outputFile, "addi %s, %s, %d\n", lhs.name.c_str(), rhs.name.c_str(), imm.value);
            break;
        case '-':
            printToFile(outputFile, "subi %s, %s, %d\n", lhs.name.c_str(), rhs.name.c_str(), imm.value);
            break;
        case '*':
            printToFile(outputFile, "muli %s, %s, %d\n", lhs.name.c_str(), rhs.name.c_str(), imm.value);
            break;
        case '/':
            printToFile(outputFile, "divi %s, %s, %d\n", lhs.name.c_str(), rhs.name.c_str(), imm.value);
            break;
        case '%':
            printToFile(outputFile, "remi %s, %s, %d\n", lhs.name.c_str(), rhs.name.c_str(), imm.value);
            break;
        default:
            throw std::runtime_error("Invalid binary operator");
    }
}

void Mv::print() { printToFile(outputFile, "mv %s, %s\n", lhs.name.c_str(), rhs.name.c_str()); }

void Li::print() { printToFile(outputFile, "li %s, %d\n", lhs.name.c_str(), imm.value); }

void LabelAssembly::print() { printToFile(outputFile, "%s:\n", label.ident.c_str()); }

void J::print() { printToFile(outputFile, "j %s\n", label.ident.c_str()); }

void CallAssembly::print() { printToFile(outputFile, "call %s\n", label.ident.c_str()); }

void Ret::print() { printToFile(outputFile, "ret\n"); }

void Lw::print() { printToFile(outputFile, "lw %s, %d(%s)\n", lhs.name.c_str(), offset, rhs.name.c_str()); }

void Sw::print() { printToFile(outputFile, "sw %s, %d(%s)\n", lhs.name.c_str(), offset, rhs.name.c_str()); }

void Branch::print() {
    switch (op[0]) {
        case '>':
            if (op.length() == 1) {
                printToFile(outputFile, "bgt %s, %s, %s\n", lhs.name.c_str(), rhs.name.c_str(), label.ident.c_str());
            } else {
                printToFile(outputFile, "bge %s, %s, %s\n", lhs.name.c_str(), rhs.name.c_str(), label.ident.c_str());
            }
            break;
        case '<':
            if (op.length() == 1) {
                printToFile(outputFile, "blt %s, %s, %s\n", lhs.name.c_str(), rhs.name.c_str(), label.ident.c_str());
            } else {
                printToFile(outputFile, "ble %s, %s, %s\n", lhs.name.c_str(), rhs.name.c_str(), label.ident.c_str());
            }
            break;
        case '=':
            printToFile(outputFile, "beq %s, %s, %s\n", lhs.name.c_str(), rhs.name.c_str(), label.ident.c_str());
            break;
        case '!':
            printToFile(outputFile, "bne %s, %s, %s\n", lhs.name.c_str(), rhs.name.c_str(), label.ident.c_str());
            break;
        default:
            throw std::runtime_error("Invalid branch operator");
    }
}

void La::print() { printToFile(outputFile, "la %s, %s\n", lhs.name.c_str(), ident.ident.c_str()); }

void WordAssembly::print() { printToFile(outputFile, ".word %d\n", val.value); }