#include "ir.h"
#include <string.h>
#include <cstdarg>

static void printToFile(FILE* file, const char* format, ...) {
    // if format begin with "LABEL"
    if (strncmp(format, "LABEL", 5) == 0) {
        fprintf(file, "  ");
    } else if (strncmp(format, "FUNCTION", 8) != 0) {
        fprintf(file, "    ");
    }

    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
}

void LoadImm::print() { printToFile(immediateFile, "%s = #%d\n", ident.ident.c_str(), value.value); }

void Assign::print() { printToFile(immediateFile, "%s = %s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Binop::print() {
    printToFile(immediateFile, "%s = %s %s %s\n", lhs.ident.c_str(), rhs1.ident.c_str(), op.c_str(), rhs2.ident.c_str());
}

void BinopImm::print() {
    printToFile(immediateFile, "%s = %s %s #%d\n", lhs.ident.c_str(), rhs.ident.c_str(), op.c_str(), imm.value);
}

void Unop::print() { printToFile(immediateFile, "%s = %s%s\n", lhs.ident.c_str(), op.c_str(), rhs.ident.c_str()); }

void Load::print() { printToFile(immediateFile, "%s = *%s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Store::print() { printToFile(immediateFile, "*%s = %s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Label::print() { printToFile(immediateFile, "LABEL %s:\n", name.c_str()); }

void Goto::print() { printToFile(immediateFile, "GOTO %s\n", label.c_str()); }

void CondGoto::print() {
    printToFile(immediateFile, "IF %s %s %s GOTO %s\n", lhs.ident.c_str(), op.c_str(), rhs.ident.c_str(), label.c_str());
}

void FuncDefNode::print() { printToFile(immediateFile, "FUNCTION %s:\n", name.ident.c_str()); }

void CallWithRet::print() {
    printToFile(immediateFile, "%s = CALL %s\n", lhs.ident.c_str(), name.c_str());
}

void Call::print() { printToFile(immediateFile, "CALL %s\n", name.c_str()); }

void Param::print() { printToFile(immediateFile, "PARAM %s\n", ident.ident.c_str()); }

void Arg::print() { printToFile(immediateFile, "ARG %s\n", ident.ident.c_str()); }

void ReturnWithVal::print() { printToFile(immediateFile, "RETURN %s\n", ident.ident.c_str()); }

void Return::print() { printToFile(immediateFile, "RETURN\n"); }

void VarDec::print() { printToFile(immediateFile, "DEC %s #%d\n", ident.ident.c_str(), size); }

void GlobalVar::print() { printToFile(immediateFile, "GLOBAL %s:\n", ident.ident.c_str()); }

void LoadGlobal::print() { printToFile(immediateFile, "%s = &%s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Word::print() { printToFile(immediateFile, ".WORD #%d\n", imm.value); }