#include "ir.h"
#include <string.h>
#include <cstdarg>

extern FILE* immediateFile;

static void printToFile(FILE* file, const char* format, ...) {
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

static void linkToTail(AssemblyNode*& tail, AssemblyNode* target) {
    if (tail) {
        tail->next = target;
    } else {
        throw std::runtime_error("tail is nullptr");
    }
    tail = target;
}

int GenerateTable::insert(std::string ident, int size) {
    if (identMap.find(ident) != identMap.end()) {
        return 0;
    }
    stackOffset += size;
    identMap[ident] = stackOffset;
    return size;
}

Register GenerateTable::allocateTemp(std::string ident, AssemblyNode*& tail) {
    if (identMap.find(ident) == identMap.end()) {
        throw std::runtime_error("allocateTemp: Identifier" + ident + " not found in table");
    }
    for (int i : TEMP_REGISTERS) {
        if (registers[i].empty()) {
            registers[i] = ident;
            Register reg(i);
            linkToTail(tail, new Lw(reg, Register(2), ImmAssembly(identMap[ident])));
            return reg;
        }
    }
    throw std::runtime_error("No available register");
}

Register GenerateTable::allocateArg(std::string ident, AssemblyNode*& tail) {
    if (identMap.find(ident) == identMap.end()) {
        throw std::runtime_error("allocateArg: Identifier" + ident + " not found in table");
    }
    for (int i : ARG_REGISTERS) {
        if (registers[i].empty()) {
            registers[i] = ident;
            Register reg(i);
            linkToTail(tail, new Lw(reg, Register(2), ImmAssembly(identMap[ident])));
            return reg;
        }
    }
    throw std::runtime_error("No available register");
}

void GenerateTable::free(Register reg, AssemblyNode*& tail) {
    int i = reg.index;
    if (registers[i].empty()) {
        return;
    }
    linkToTail(tail, new Sw(reg, Register(2), ImmAssembly(identMap[registers[i]])));
    registers[i] = "";
}

void LoadImm::print() { printToFile(immediateFile, "%s = #%d\n", ident.ident.c_str(), value.value); }

void LoadImm::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register reg = table->allocateTemp(ident.ident, tail);
    linkToTail(tail, new Li(reg, ImmAssembly(value.value)));
    table->free(reg, tail);
}

void Assign::print() { printToFile(immediateFile, "%s = %s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Assign::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register lhsReg = table->allocateTemp(lhs.ident, tail);
    Register rhsReg = table->allocateTemp(rhs.ident, tail);
    linkToTail(tail, new Mv(lhsReg, rhsReg));
    table->free(lhsReg, tail);
    table->free(rhsReg, tail);
}

void Binop::print() {
    printToFile(immediateFile, "%s = %s %s %s\n", lhs.ident.c_str(), rhs1.ident.c_str(), op.c_str(),
                rhs2.ident.c_str());
}

void Binop::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register lhsReg = table->allocateTemp(lhs.ident, tail);
    Register rhs1Reg = table->allocateTemp(rhs1.ident, tail);
    Register rhs2Reg = table->allocateTemp(rhs2.ident, tail);
    linkToTail(tail, new BinaryAssembly(lhsReg, rhs1Reg, rhs2Reg, op));
    table->free(lhsReg, tail);
    table->free(rhs1Reg, tail);
    table->free(rhs2Reg, tail);
}

void BinopImm::print() {
    printToFile(immediateFile, "%s = %s %s #%d\n", lhs.ident.c_str(), rhs.ident.c_str(), op.c_str(), imm.value);
}

void BinopImm::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register lhsReg = table->allocateTemp(lhs.ident, tail);
    Register rhsReg = table->allocateTemp(rhs.ident, tail);
    linkToTail(tail, new BinaryImmAssembly(lhsReg, rhsReg, ImmAssembly(imm.value), op));
    table->free(lhsReg, tail);
    table->free(rhsReg, tail);
}

void Unop::print() { printToFile(immediateFile, "%s = %s%s\n", lhs.ident.c_str(), op.c_str(), rhs.ident.c_str()); }

void Unop::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register lhsReg = table->allocateTemp(lhs.ident, tail);
    Register rhsReg = table->allocateTemp(rhs.ident, tail);
    switch (op[0]) {
        case '+':
            linkToTail(tail, new Mv(lhsReg, rhsReg));
            break;
        case '-':
            linkToTail(tail, new BinaryAssembly(lhsReg, Register(0), rhsReg, "-"));
            break;
        default:
            throw std::runtime_error("Invalid unary operator");
    }
    table->free(lhsReg, tail);
    table->free(rhsReg, tail);
}

void Load::print() { printToFile(immediateFile, "%s = *%s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Load::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register lhsReg = table->allocateTemp(lhs.ident, tail);
    Register rhsReg = table->allocateTemp(rhs.ident, tail);
    linkToTail(tail, new Lw(lhsReg, rhsReg));
    table->free(lhsReg, tail);
    table->free(rhsReg, tail);
}

void Store::print() { printToFile(immediateFile, "*%s = %s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Store::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register lhsReg = table->allocateTemp(lhs.ident, tail);
    Register rhsReg = table->allocateTemp(rhs.ident, tail);
    linkToTail(tail, new Sw(lhsReg, rhsReg));
    table->free(lhsReg, tail);
    table->free(rhsReg, tail);
}

void Label::print() { printToFile(immediateFile, "LABEL %s:\n", name.c_str()); }

void Label::generate(GenerateTable* table, AssemblyNode*& tail) { linkToTail(tail, new LabelAssembly(name)); }

void Goto::print() { printToFile(immediateFile, "GOTO %s\n", label.c_str()); }

void Goto::generate(GenerateTable* table, AssemblyNode*& tail) { linkToTail(tail, new J(label)); }

void CondGoto::print() {
    printToFile(immediateFile, "IF %s %s %s GOTO %s\n", lhs.ident.c_str(), op.c_str(), rhs.ident.c_str(),
                label.c_str());
}

void CondGoto::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register lhsReg = table->allocateTemp(lhs.ident, tail);
    Register rhsReg = table->allocateTemp(rhs.ident, tail);
    linkToTail(tail, new Branch(lhsReg, rhsReg, label, op));
    table->free(lhsReg, tail);
    table->free(rhsReg, tail);
}

void FuncDefNode::print() { printToFile(immediateFile, "FUNCTION %s:\n", name.ident.c_str()); }

void FuncDefNode::generate(GenerateTable* table, AssemblyNode*& tail) {
    // prologue
    table->clearStackOffset();
    int prologueSize = SIZE_OF_INT;
    for (IRNode* cur = this->next; cur != nullptr; cur = cur->next) {
        if (typeid(*cur) == typeid(FuncDefNode)) {
            break;
        }
        prologueSize += cur->prologue(table);
    }
    table->insert("_ra", SIZE_OF_INT);
    linkToTail(tail, new LabelAssembly(name.ident));
    if (prologueSize > 0) {
        if (prologueSize > 2048) {
            throw std::runtime_error("TODO: prologue size too large");
        } else {
            linkToTail(tail, new BinaryImmAssembly(Register(2), Register(2), ImmAssembly(-prologueSize), "+"));
        }
    }
    linkToTail(tail, new Sw(Register(1), Register(2), table->getStatckOffset()));    // ra
}

static void freeArgs(GenerateTable* table, AssemblyNode*& tail) {
    for (int i : ARG_REGISTERS) {
        table->free(Register(i), tail);
    }
}

void CallWithRet::print() { printToFile(immediateFile, "%s = CALL %s\n", lhs.ident.c_str(), name.c_str()); }

void CallWithRet::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register lhsReg = table->allocateTemp(lhs.ident, tail);
    linkToTail(tail, new CallAssembly(name));
    linkToTail(tail, new Mv(lhsReg, Register(10)));
    table->free(lhsReg, tail);
    freeArgs(table, tail);
}

void Call::print() { printToFile(immediateFile, "CALL %s\n", name.c_str()); }

void Call::generate(GenerateTable* table, AssemblyNode*& tail) {
    linkToTail(tail, new CallAssembly(name));
    freeArgs(table, tail);
}

void Param::print() { printToFile(immediateFile, "PARAM %s\n", ident.ident.c_str()); }

void Param::generate(GenerateTable* table, AssemblyNode*& tail) {}

void Arg::print() { printToFile(immediateFile, "ARG %s\n", ident.ident.c_str()); }

void Arg::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register identReg = table->allocateArg(ident.ident, tail);
    Register argReg = table->allocateTemp(ident.ident, tail);
    linkToTail(tail, new Mv(identReg, argReg));
    table->free(identReg, tail);
}

static void epilogue(GenerateTable* table, AssemblyNode*& tail) {
    linkToTail(tail, new Lw(Register(1), Register(2), table->getStatckOffset()));    // ra
    linkToTail(tail, new BinaryImmAssembly(Register(2), Register(2), ImmAssembly(table->getStatckOffset()), "+"));
}

void ReturnWithVal::print() { printToFile(immediateFile, "RETURN %s\n", ident.ident.c_str()); }

void ReturnWithVal::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register retReg = table->allocateTemp(ident.ident, tail);
    linkToTail(tail, new Mv(Register(10), retReg));
    epilogue(table, tail);
    linkToTail(tail, new Ret());
}

void Return::print() { printToFile(immediateFile, "RETURN\n"); }

void Return::generate(GenerateTable* table, AssemblyNode*& tail) {
    epilogue(table, tail);
    linkToTail(tail, new Ret());
}

void VarDec::print() { printToFile(immediateFile, "DEC %s #%d\n", ident.ident.c_str(), size); }

void VarDec::generate(GenerateTable* table, AssemblyNode*& tail) {}

void GlobalVar::print() { printToFile(immediateFile, "GLOBAL %s:\n", ident.ident.c_str()); }

void GlobalVar::generate(GenerateTable* table, AssemblyNode*& tail) {}

void LoadGlobal::print() { printToFile(immediateFile, "%s = &%s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void LoadGlobal::generate(GenerateTable* table, AssemblyNode*& tail) {}

void Word::print() { printToFile(immediateFile, ".WORD #%d\n", imm.value); }

void Word::generate(GenerateTable* table, AssemblyNode*& tail) {}