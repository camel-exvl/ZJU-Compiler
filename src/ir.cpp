#include "ir.h"
#include <string.h>
#include <cstdarg>

extern FILE* immediateFile;

static void printToFile(FILE* file, const char* format, ...) {
    if (strncmp(format, "LABEL", 5) == 0) {
        fprintf(file, "  ");
    } else if (strncmp(format, "FUNCTION", 8) != 0 && strncmp(format, "GLOBAL", 6) != 0) {
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

int GenerateTable::insert(std::string ident, int size, bool storeAddr) {
    if (identStack.find(ident) != identStack.end()) {
        return 0;
    }
    if (storeAddr) {
        arraySet.insert(ident);
    }
    identStack[ident] = stackOffset;
    stackOffset += size;
    return size;
}

void GenerateTable::allocateReg(Register reg, int offset, AssemblyNode*& tail, bool isArray, bool needLoad) {
    regUsed[reg.index] |= 1;
    if (isArray) {
        linkToTail(tail, new Mv(reg, Register(2)));
        if (offset + preservedOffset > 0) {
            linkToTail(tail, new BinaryImmAssembly(reg, reg, ImmAssembly(offset + preservedOffset), "+"));
        }
    } else if (needLoad) {
        linkToTail(tail, new Lw(reg, Register(2), ImmAssembly(offset + preservedOffset)));
    }
}

Register GenerateTable::allocateTemp(std::string ident, AssemblyNode*& tail, bool needLoad) {
    if (identRegMap.find(ident) != identRegMap.end() && identRegMap[ident] != -1) {
        if (spillParams.empty() || std::find(spillParams.begin(), spillParams.end(), ident) == spillParams.end()) {
            return Register(identRegMap[ident]);
        }
    }
    if (identStack.find(ident) == identStack.end()) {
        if (spillParams.empty() || std::find(spillParams.begin(), spillParams.end(), ident) == spillParams.end()) {
            throw std::runtime_error("allocateTemp: Identifier " + ident + " not found in table");
        }
        identStack[ident] =
            stackOffset +
            (std::find(spillParams.begin(), spillParams.end(), ident) - spillParams.begin()) * SIZE_OF_INT;
    }
    if (registers[8].empty()) {
        registers[8] = ident;
        Register reg(8);
        allocateReg(reg, identStack[ident], tail, arraySet.find(ident) != arraySet.end(), needLoad);
        return reg;
    }
    // for (int i : TEMP_REGISTERS) {
    //     if (registers[i] == ident) {
    //         regUsed[i] = 0b11;
    //         return Register(i);
    //     }
    // }
    // for (int i : TEMP_REGISTERS) {
    //     if (registers[i].empty()) {
    //         registers[i] = ident;
    //         Register reg(i);
    //         allocateReg(reg, identStack[ident], tail, arraySet.find(ident) != arraySet.end(), needLoad);
    //         return reg;
    //     }
    // }
    // for (int index = (lastVictim + 1) % NUM_OF_TEMP_REG; index != lastVictim; index = (index + 1) % NUM_OF_TEMP_REG) {
    //     int i = TEMP_REGISTERS[index];
    //     if ((regUsed[i] & 1) == 0) {
    //         clear(Register(i), tail, regUsed[i] & 0b10);
    //         registers[i] = ident;
    //         Register reg(i);
    //         allocateReg(reg, identStack[ident], tail, arraySet.find(ident) != arraySet.end(), needLoad);
    //         lastVictim = index;
    //         return reg;
    //     }
    // }
    throw std::runtime_error("No available register");
}

Register GenerateTable::allocateArg(std::string ident, AssemblyNode*& tail) {
    if (identStack.find(ident) == identStack.end() && identRegMap.find(ident) == identRegMap.end()) {
        throw std::runtime_error("allocateArg: Identifier " + ident + " not found in table");
    }
    for (int i : ARG_REGISTERS) {
        if (registers[i].empty()) {
            registers[i] = ident;
            Register reg(i);
            if (identRegMap.find(ident) != identRegMap.end()) {
                regUsed[i] |= 1;
                linkToTail(tail, new Mv(reg, Register(identRegMap[ident])));
            } else {
                allocateReg(reg, identStack[ident], tail, arraySet.find(ident) != arraySet.end(), false);
            }
            return reg;
        }
    }
    // spill to stack
    Register reg = allocateTemp(ident, tail, false);  // TODO: 最好能用a*寄存器
    allocateReg(reg, identStack[ident], tail, arraySet.find(ident) != arraySet.end(), false);
    linkToTail(tail, new Sw(reg, Register(2), ImmAssembly(preservedUsed)));
    preservedUsed += SIZE_OF_INT;
    free(reg, tail, false);
    return Register(0);
}

void GenerateTable::free(Register reg, AssemblyNode*& tail, bool needStore) {
    int i = reg.index;
    if ((regUsed[i] & 1) == 0 || registers[i].empty()) {
        return;
    }
    regUsed[i] |= needStore ? 0b10 : 0;  // set need store bit
    regUsed[i] &= 0b10;                  // clear used bit
}

void GenerateTable::clear(Register reg, AssemblyNode*& tail, bool needStore) {
    int i = reg.index;
    if ((needStore || regUsed[i] & 0b10) && reg.name[0] == 't' && arraySet.find(registers[i]) == arraySet.end()) {
        linkToTail(tail, new Sw(reg, Register(2), ImmAssembly(identStack[registers[i]] + preservedOffset)));
    }
    regUsed[i] = 0;
    registers[i] = "";
}

void GenerateTable::clearStack() {
    stackOffset = 0;
    preservedOffset = 0;
    identStack.clear();
    spillParams.clear();
    params.clear();
    arraySet.clear();
}

bool IRNode::_livenessAnalysis(IRNode* next) {
    std::unordered_set<std::string> newOut = next ? next->in : std::unordered_set<std::string>();
    std::unordered_set<std::string> newIn;
    std::set_difference(newOut.begin(), newOut.end(), def.begin(), def.end(), std::inserter(newIn, newIn.begin()));
    std::set_union(use.begin(), use.end(), newIn.begin(), newIn.end(), std::inserter(newIn, newIn.begin()));
    if (newIn != in || newOut != out) {
        in = newIn;
        out = newOut;
        return true;
    }
    return false;
}

bool IRNode::_livenessAnalysis(IRNode* first, IRNode* second) {
    std::unordered_set<std::string> newOut = first ? first->in : std::unordered_set<std::string>();
    if (second) {
        std::set_union(newOut.begin(), newOut.end(), second->in.begin(), second->in.end(),
                       std::inserter(newOut, newOut.begin()));
    }
    std::unordered_set<std::string> newIn;
    std::set_difference(newOut.begin(), newOut.end(), def.begin(), def.end(), std::inserter(newIn, newIn.begin()));
    std::set_union(use.begin(), use.end(), newIn.begin(), newIn.end(), std::inserter(newIn, newIn.begin()));
    if (newIn != in || newOut != out) {
        in = newIn;
        out = newOut;
        return true;
    }
    return false;
}

void LoadImm::print() { printToFile(immediateFile, "%s = #%d\n", ident.ident.c_str(), value.value); }

void LoadImm::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register reg = table->allocateTemp(ident.ident, tail, false);
    linkToTail(tail, new Li(reg, ImmAssembly(value.value)));
    table->free(reg, tail, true);
}

void Assign::print() { printToFile(immediateFile, "%s = %s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Assign::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register rhsReg = table->allocateTemp(rhs.ident, tail, true);
    Register lhsReg = table->allocateTemp(lhs.ident, tail, false);
    linkToTail(tail, new Mv(lhsReg, rhsReg));
    table->free(lhsReg, tail, true);
    table->free(rhsReg, tail, false);
}

void Binop::print() {
    printToFile(immediateFile, "%s = %s %s %s\n", lhs.ident.c_str(), rhs1.ident.c_str(), op.c_str(),
                rhs2.ident.c_str());
}

void Binop::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register rhs1Reg = table->allocateTemp(rhs1.ident, tail, true);
    Register rhs2Reg = table->allocateTemp(rhs2.ident, tail, true);
    Register lhsReg = table->allocateTemp(lhs.ident, tail, false);
    linkToTail(tail, new BinaryAssembly(lhsReg, rhs1Reg, rhs2Reg, op));
    table->free(lhsReg, tail, true);
    table->free(rhs1Reg, tail, false);
    table->free(rhs2Reg, tail, false);
}

void BinopImm::print() {
    printToFile(immediateFile, "%s = %s %s #%d\n", lhs.ident.c_str(), rhs.ident.c_str(), op.c_str(), imm.value);
}

void BinopImm::generate(GenerateTable* table, AssemblyNode*& tail) {
    if (imm.value == 0) {
        Assign(lhs, rhs).generate(table, tail);
        return;
    }
    Register rhsReg = table->allocateTemp(rhs.ident, tail, true);
    Register lhsReg = table->allocateTemp(lhs.ident, tail, false);
    linkToTail(tail, new BinaryImmAssembly(lhsReg, rhsReg, ImmAssembly(imm.value), op));
    table->free(lhsReg, tail, true);
    table->free(rhsReg, tail, false);
}

void Unop::print() { printToFile(immediateFile, "%s = %s%s\n", lhs.ident.c_str(), op.c_str(), rhs.ident.c_str()); }

void Unop::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register rhsReg = table->allocateTemp(rhs.ident, tail, true);
    Register lhsReg = table->allocateTemp(lhs.ident, tail, false);
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
    table->free(lhsReg, tail, true);
    table->free(rhsReg, tail, false);
}

void Load::print() { printToFile(immediateFile, "%s = *%s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Load::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register rhsReg = table->allocateTemp(rhs.ident, tail, true);
    Register lhsReg = table->allocateTemp(lhs.ident, tail, false);
    linkToTail(tail, new Lw(lhsReg, rhsReg));
    table->free(lhsReg, tail, true);
    table->free(rhsReg, tail, false);
}

void Store::print() { printToFile(immediateFile, "*%s = %s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Store::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register lhsReg = table->allocateTemp(lhs.ident, tail, true);
    Register rhsReg = table->allocateTemp(rhs.ident, tail, true);
    linkToTail(tail, new Sw(rhsReg, lhsReg));
    table->free(lhsReg, tail, false);
    table->free(rhsReg, tail, false);
}

static void saveTemp(GenerateTable* table, AssemblyNode*& tail) {
    for (int i : TEMP_REGISTERS) {
        table->clear(Register(i), tail, false);
    }
}

void Label::print() { printToFile(immediateFile, "LABEL %s:\n", name.c_str()); }

void Label::generate(GenerateTable* table, AssemblyNode*& tail) {
    saveTemp(table, tail);
    linkToTail(tail, new LabelAssembly(name));
}

void Goto::print() { printToFile(immediateFile, "GOTO %s\n", label.c_str()); }

void Goto::generate(GenerateTable* table, AssemblyNode*& tail) {
    saveTemp(table, tail);
    linkToTail(tail, new J(label));
}

void CondGoto::print() {
    printToFile(immediateFile, "IF %s %s %s GOTO %s\n", lhs.ident.c_str(), op.c_str(), rhs.ident.c_str(),
                label.c_str());
}

void CondGoto::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register lhsReg = table->allocateTemp(lhs.ident, tail, true);
    Register rhsReg = table->allocateTemp(rhs.ident, tail, true);
    saveTemp(table, tail);
    linkToTail(tail, new Branch(lhsReg, rhsReg, label, op));
    table->free(lhsReg, tail, false);
    table->free(rhsReg, tail, false);
}

void FuncDefNode::print() { printToFile(immediateFile, "FUNCTION %s:\n", name.ident.c_str()); }

static int linearScan(GenerateTable* table, std::vector<IRNode*>& nodes) {
    int size = 0;
    table->intervals.clear();
    for (int i = 0; i < (int)nodes.size(); ++i) {
        for (auto ident : nodes[i]->out) {
            if (table->intervals.find(ident) == table->intervals.end()) {
                table->intervals[ident] = VarInterval(ident, i, i + 1);
            } else {
                table->intervals[ident].end = i + 1;
            }
        }
    }
    std::vector<VarInterval> live;
    std::map<VarInterval, int, std::greater<VarInterval>> active;
    std::unordered_set<int> freeRegisters;
    for (int i = 0; i < NUM_OF_TEMP_REG; i++) {
        freeRegisters.insert(TEMP_REGISTERS[i]);
    }
    for (auto interval : table->intervals) {
        live.emplace_back(interval.second);
    }

    table->clearIdentReg();
    std::sort(live.begin(), live.end());
    for (auto i : live) {
        std::printf("%s %d---%d\n", i.ident.c_str(), i.start, i.end);
    }

    for (auto i : live) {
        // expire old intervals
        for (auto j = active.begin(); j != active.end();) {
            if (j->first.end >= i.start) {
                break;
            }
            freeRegisters.insert(j->second);
            table->insertIdentReg(j->first.ident, j->second);
            j = active.erase(j);
        }

        if (active.size() == NUM_OF_TEMP_REG) {
            // spill
            auto spill = active.begin();
            if (spill->first.end > i.end) {
                int reg = spill->second;
                table->insertIdentReg(spill->first.ident, -1);
                size += table->insert(spill->first.ident, SIZE_OF_INT, false);
                active.erase(spill);
                active[i] = reg;
            } else {
                table->insertIdentReg(i.ident, -1);
                size += table->insert(i.ident, SIZE_OF_INT, false);
            }
        } else {
            int reg = *freeRegisters.begin();
            freeRegisters.erase(reg);
            active[i] = reg;
        }
    }
    for (auto i : active) {
        table->insertIdentReg(i.first.ident, i.second);
    }
    table->printTest();
    return size;
}

void FuncDefNode::generate(GenerateTable* table, AssemblyNode*& tail) {
    table->clearStack();
    int prologueSize;

    // liveness analysis
    table->clearLabel();
    std::vector<IRNode*> nodes;
    int index = 0;
    for (IRNode* cur = this->next; cur != nullptr; cur = cur->next) {
        if (typeid(*cur) == typeid(FuncDefNode)) {
            break;
        } else if (typeid(*cur) == typeid(Label)) {
            table->insertLabel(dynamic_cast<Label*>(cur)->getName(), cur);
        }
        nodes.push_back(cur);
        cur->index = index++;
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto cur = nodes.rbegin(); cur != nodes.rend(); cur++) {
            changed |= (*cur)->livenessAnalysis(table);
        }
    }
    prologueSize = linearScan(table, nodes);

    // prologue
    table->curParam = 0;
    bool foundCall = false;
    for (IRNode* cur = this->next; cur != nullptr; cur = cur->next) {
        if (typeid(*cur) == typeid(FuncDefNode)) {
            break;
        }
        prologueSize += cur->prologue(table);
        foundCall |= typeid(*cur) == typeid(CallWithRet) || typeid(*cur) == typeid(Call);
    }
    if (foundCall) {
        prologueSize += SIZE_OF_INT;
        table->insert("_ra", SIZE_OF_INT);
    }
    prologueSize += SIZE_OF_INT;
    table->insert("_s0", SIZE_OF_INT);
    table->curParam = 0;

    prologueSize += table->getPreserved();
    linkToTail(tail, new LabelAssembly(name.ident));
    if (prologueSize > 0) {
        if (prologueSize > 2048) {
            throw std::runtime_error("TODO: prologue size too large");
        } else {
            linkToTail(tail, new BinaryImmAssembly(Register(2), Register(2), ImmAssembly(-prologueSize), "+"));
        }
    }
    if (foundCall) {
        linkToTail(tail, new Sw(Register(1), Register(2), table->getOffset("_ra")));
    }
    linkToTail(tail, new Sw(Register(8), Register(2), table->getOffset("_s0")));

    // save arguments
    for (int i = 0; i < std::min(table->curParam, 8); i++) {
        linkToTail(tail, new Sw(Register(i + 10), Register(2), table->getOffset(table->params[i])));
    }
}

static void freeArgs(GenerateTable* table, AssemblyNode*& tail) {
    table->clearPreservedUsed();
    for (int i : ARG_REGISTERS) {
        table->clear(Register(i), tail, false);
    }
}

void CallWithRet::print() { printToFile(immediateFile, "%s = CALL %s\n", lhs.ident.c_str(), name.c_str()); }

void CallWithRet::generate(GenerateTable* table, AssemblyNode*& tail) {
    saveTemp(table, tail);

    // save temp
    for (auto i : regToSave) {
        std::printf("SAVE %s on %s\n", i.first.c_str(), REGISTER_NAMES[i.second].c_str());
        linkToTail(tail, new Sw(Register(i.second), Register(2), table->getOffset(i.first)));
    }

    linkToTail(tail, new CallAssembly(name));

    // load temp
    for (auto i : regToSave) {
        std::printf("LOAD %s on %s\n", i.first.c_str(), REGISTER_NAMES[i.second].c_str());
        linkToTail(tail, new Lw(Register(i.second), Register(2), table->getOffset(i.first)));
    }

    Register lhsReg = table->allocateTemp(lhs.ident, tail, false);
    linkToTail(tail, new Mv(lhsReg, Register(10)));
    table->free(lhsReg, tail, true);
    freeArgs(table, tail);
}

void Call::print() { printToFile(immediateFile, "CALL %s\n", name.c_str()); }

void Call::generate(GenerateTable* table, AssemblyNode*& tail) {
    saveTemp(table, tail);

    // save temp
    for (auto i : regToSave) {
        linkToTail(tail, new Sw(Register(i.second), Register(2), table->getOffset(i.first)));
    }

    linkToTail(tail, new CallAssembly(name));

    // load temp
    for (auto i : regToSave) {
        linkToTail(tail, new Lw(Register(i.second), Register(2), table->getOffset(i.first)));
    }

    freeArgs(table, tail);
}

void Param::print() { printToFile(immediateFile, "PARAM %s\n", ident.ident.c_str()); }

void Param::generate(GenerateTable* table, AssemblyNode*& tail) {
    ++table->curParam;
    if (table->getIdentReg(ident.ident) != -1) {
        if (table->curParam < 8) {  // a0-a7
            linkToTail(tail, new Mv(Register(table->getIdentReg(ident.ident)), Register(table->curParam + 9)));
        } else {  // stack
            Register reg = table->allocateTemp(ident.ident, tail, false);
            linkToTail(tail, new Lw(reg, Register(2), table->getOffset(ident.ident)));
            linkToTail(tail, new Mv(table->allocateArg(ident.ident, tail), reg));
            table->free(reg, tail, false);
        }
    }
}

void Arg::print() { printToFile(immediateFile, "ARG %s\n", ident.ident.c_str()); }

void Arg::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register argReg = table->allocateArg(ident.ident, tail);
    if (argReg.index == 0) {
        return;
    }
    // Register identReg = table->allocateTemp(ident.ident, tail, true);
    // linkToTail(tail, new Mv(argReg, identReg));
    // table->free(identReg, tail, false);
}

static void epilogue(GenerateTable* table, AssemblyNode*& tail) {
    if (table->identExists("_s0")) {
        linkToTail(tail, new Lw(Register(8), Register(2), table->getOffset("_s0")));
    }
    if (table->identExists("_ra")) {
        linkToTail(tail, new Lw(Register(1), Register(2), table->getOffset("_ra")));
    }
    if (table->getStackOffset() + table->getPreserved() > 0) {
        linkToTail(tail, new BinaryImmAssembly(Register(2), Register(2),
                                               ImmAssembly(table->getStackOffset() + table->getPreserved()), "+"));
    }
}

void ReturnWithVal::print() { printToFile(immediateFile, "RETURN %s\n", ident.ident.c_str()); }

void ReturnWithVal::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register retReg = table->allocateTemp(ident.ident, tail, true);
    linkToTail(tail, new Mv(Register(10), retReg));
    saveTemp(table, tail);
    epilogue(table, tail);
    linkToTail(tail, new Ret());
    table->free(retReg, tail, false);
}

void Return::print() { printToFile(immediateFile, "RETURN\n"); }

void Return::generate(GenerateTable* table, AssemblyNode*& tail) {
    saveTemp(table, tail);
    epilogue(table, tail);
    linkToTail(tail, new Ret());
}

void VarDec::print() { printToFile(immediateFile, "DEC %s #%d\n", ident.ident.c_str(), size); }

void VarDec::generate(GenerateTable* table, AssemblyNode*& tail) {}

void GlobalVar::print() { printToFile(immediateFile, "GLOBAL %s:\n", ident.ident.c_str()); }

void GlobalVar::generate(GenerateTable* table, AssemblyNode*& tail) {
    table->insertGlobal(ident.ident);
    linkToTail(tail, new LabelAssembly(ident.ident));
}

void LoadGlobal::print() { printToFile(immediateFile, "%s = &%s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void LoadGlobal::generate(GenerateTable* table, AssemblyNode*& tail) {
    Register lhsReg = table->allocateTemp(lhs.ident, tail, false);
    linkToTail(tail, new La(lhsReg, rhs.ident));
    table->free(lhsReg, tail, true);
}

void Word::print() { printToFile(immediateFile, ".WORD #%d\n", imm.value); }

void Word::generate(GenerateTable* table, AssemblyNode*& tail) {
    linkToTail(tail, new WordAssembly(ImmAssembly(imm.value)));
}