#include "ir.h"
#include <string.h>
#include <cstdarg>
#include <cassert>

extern FILE *immediateFile;

static void printToFile(FILE *file, const char *format, ...) {
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

static void linkToTail(AssemblyNode *&tail, AssemblyNode *target) {
    if (tail) {
        tail->next = target;
    } else {
        throw std::runtime_error("tail is nullptr");
    }
    tail = target;
}

int GenerateTable::insertStack(std::string ident, int size) {
    if (identStackOffset.find(ident) != identStackOffset.end()) {
        return 0;
    }
    if (size > 0) {
        stackOffset += size;
        identStackOffset[ident] = stackOffset;
    } else {
        identStackOffset[ident] = size;
    }
    return size;
}

int GenerateTable::getStackOffset(std::string ident) {
    assert(identStackOffset.find(ident) != identStackOffset.end());
    return stackOffset - identStackOffset[ident];
}

Register GenerateTable::allocateReg(std::string ident, AssemblyNode *&tail, bool needLoad) {
    // if already allocated
    if (identReg.find(ident) != identReg.end()) {
        if (needLoad && (regState[identReg[ident]] & 1) == 0) {
            assert(identStackOffset.find(ident) != identStackOffset.end());
            if (arraySet.find(ident) != arraySet.end()) {
                linkToTail(tail, new BinaryImmAssembly(Register(identReg[ident]), Register(2),
                                                       ImmAssembly(getStackOffset(ident)), "+"));

            } else {
                linkToTail(tail, new Lw(Register(identReg[ident]), Register(2), getStackOffset(ident)));
            }
        }
        regState[identReg[ident]] |= 1;
        return Register(identReg[ident]);
    }

    // variable is spilled
    if (identStackOffset.find(ident) == identStackOffset.end()) {  // the ident is not used
        return Register(0);
    }
    // find in cache
    for (auto i = 0ull; i < TEMP_REGISTERS.size(); ++i) {
        if (tempReg[i] == ident) {
            return Register(TEMP_REGISTERS[i]);
        }
    }
    // allocate new register
    for (auto i = 0ull; i < TEMP_REGISTERS.size(); ++i) {
        if (tempReg[i].empty()) {
            int reg = TEMP_REGISTERS[i];
            regState[reg] |= 1;
            tempReg[i] = ident;
            if (arraySet.find(ident) != arraySet.end()) {
                linkToTail(tail,
                           new BinaryImmAssembly(Register(reg), Register(2), ImmAssembly(getStackOffset(ident)), "+"));
            } else {
                if (needLoad) {
                    linkToTail(tail, new Lw(Register(reg), Register(2), getStackOffset(ident)));
                }
            }
            return Register(reg);
        }
    }
    // spill
    for (auto i = 0ull; i < TEMP_REGISTERS.size(); ++i) {
        int reg = TEMP_REGISTERS[i];
        if ((regState[reg] & 1) == 0) {
            clear(Register(reg), tail);
            regState[reg] |= 1;
            tempReg[i] = ident;
            if (arraySet.find(ident) != arraySet.end()) {
                linkToTail(tail,
                           new BinaryImmAssembly(Register(reg), Register(2), ImmAssembly(getStackOffset(ident)), "+"));
            } else {
                if (needLoad) {
                    linkToTail(tail, new Lw(Register(reg), Register(2), getStackOffset(ident)));
                }
            }
            return Register(reg);
        }
    }
    throw std::runtime_error("No available register");
}

void GenerateTable::free(std::string ident, Register reg, AssemblyNode *&tail, bool needStore) {
    // only free temp registers
    if (std::find(TEMP_REGISTERS.begin(), TEMP_REGISTERS.end(), reg.index) != TEMP_REGISTERS.end()) {
        regState[reg.index] &= ~1;
        regState[reg.index] |= needStore ? 0b10 : 0;
    }
}

void GenerateTable::clear(Register reg, AssemblyNode *&tail) {
    if (std::find(TEMP_REGISTERS.begin(), TEMP_REGISTERS.end(), reg.index) != TEMP_REGISTERS.end()) {
        int tempIndex = std::find(TEMP_REGISTERS.begin(), TEMP_REGISTERS.end(), reg.index) - TEMP_REGISTERS.begin();
        std::string ident = tempReg[tempIndex];
        if (ident.empty()) {
            return;
        }
        if (regState[reg.index] & 0b10) {
            assert(identStackOffset.find(ident) != identStackOffset.end());
            linkToTail(tail, new Sw(reg, Register(2), getStackOffset(ident)));
        }
        regState[reg.index] = 0;
        tempReg[tempIndex] = "";
    }
}

bool IRNode::_livenessAnalysis(IRNode *next, IRNode *second) {
    std::unordered_set<std::string> newOut = next ? next->in : std::unordered_set<std::string>();
    if (second) {
        std::set_union(newOut.begin(), newOut.end(), second->in.begin(), second->in.end(),
                       std::inserter(newOut, newOut.begin()));
    }
    std::unordered_set<std::string> newIn;
    for (auto i : newOut) {
        if (def.find(i) == def.end()) {
            newIn.insert(i);
        }
    }
    // std::set_difference(newOut.begin(), newOut.end(), def.begin(), def.end(), std::inserter(newIn, newIn.begin()));
    std::set_union(use.begin(), use.end(), newIn.begin(), newIn.end(), std::inserter(newIn, newIn.begin()));

    if (newIn != in || newOut != out) {
        in = newIn;
        out = newOut;
        return true;
    }
    return false;
}

void LoadImm::print() { printToFile(immediateFile, "%s = #%d\n", ident.ident.c_str(), value.value); }

void LoadImm::generate(GenerateTable *table, AssemblyNode *&tail) {
    Register reg = table->allocateReg(ident.ident, tail, false);
    linkToTail(tail, new Li(reg, ImmAssembly(value.value)));
    table->free(ident.ident, reg, tail, true);
}

void Assign::print() { printToFile(immediateFile, "%s = %s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Assign::generate(GenerateTable *table, AssemblyNode *&tail) {
    // 此处均需先分配需要load的变量，再分配无需load的变量。否则在lhs和rhs相同时会导致rhs没有load
    Register rhsReg = table->allocateReg(rhs.ident, tail, true);
    Register lhsReg = table->allocateReg(lhs.ident, tail, false);
    linkToTail(tail, new Mv(lhsReg, rhsReg));
    table->free(lhs.ident, lhsReg, tail, true);
    table->free(rhs.ident, rhsReg, tail, false);
}

void Binop::print() {
    printToFile(immediateFile, "%s = %s %s %s\n", lhs.ident.c_str(), rhs1.ident.c_str(), op.c_str(),
                rhs2.ident.c_str());
}

void Binop::generate(GenerateTable *table, AssemblyNode *&tail) {
    Register rhs1Reg = table->allocateReg(rhs1.ident, tail, true);
    Register rhs2Reg = table->allocateReg(rhs2.ident, tail, true);
    Register lhsReg = table->allocateReg(lhs.ident, tail, false);
    linkToTail(tail, new BinaryAssembly(lhsReg, rhs1Reg, rhs2Reg, op));
    table->free(lhs.ident, lhsReg, tail, true);
    table->free(rhs1.ident, rhs1Reg, tail, false);
    table->free(rhs2.ident, rhs2Reg, tail, false);
}

void BinopImm::print() {
    printToFile(immediateFile, "%s = %s %s #%d\n", lhs.ident.c_str(), rhs.ident.c_str(), op.c_str(), imm.value);
}

void BinopImm::generate(GenerateTable *table, AssemblyNode *&tail) {
    Register rhsReg = table->allocateReg(rhs.ident, tail, true);
    Register lhsReg = table->allocateReg(lhs.ident, tail, false);
    linkToTail(tail, new BinaryImmAssembly(lhsReg, rhsReg, ImmAssembly(imm.value), op));
    table->free(lhs.ident, lhsReg, tail, true);
    table->free(rhs.ident, rhsReg, tail, false);
}

void Unop::print() { printToFile(immediateFile, "%s = %s%s\n", lhs.ident.c_str(), op.c_str(), rhs.ident.c_str()); }

void Unop::generate(GenerateTable *table, AssemblyNode *&tail) {
    Register rhsReg = table->allocateReg(rhs.ident, tail, true);
    Register lhsReg = table->allocateReg(lhs.ident, tail, false);
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
    table->free(lhs.ident, lhsReg, tail, true);
    table->free(rhs.ident, rhsReg, tail, false);
}

void Load::print() { printToFile(immediateFile, "%s = *%s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Load::generate(GenerateTable *table, AssemblyNode *&tail) {
    Register rhsReg = table->allocateReg(rhs.ident, tail, true);
    Register lhsReg = table->allocateReg(lhs.ident, tail, false);
    linkToTail(tail, new Lw(lhsReg, rhsReg));
    table->free(lhs.ident, lhsReg, tail, true);
    table->free(rhs.ident, rhsReg, tail, false);
}

void Store::print() { printToFile(immediateFile, "*%s = %s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void Store::generate(GenerateTable *table, AssemblyNode *&tail) {
    Register lhsReg = table->allocateReg(lhs.ident, tail, true);
    Register rhsReg = table->allocateReg(rhs.ident, tail, true);
    linkToTail(tail, new Sw(rhsReg, lhsReg));
    table->free(lhs.ident, lhsReg, tail, false);
    table->free(rhs.ident, rhsReg, tail, false);
}

static void saveTemp(GenerateTable *table, AssemblyNode *&tail) {
    for (int i : TEMP_REGISTERS) {
        table->clear(Register(i), tail);
    }
}

void Label::print() { printToFile(immediateFile, "LABEL %s:\n", name.c_str()); }

void Label::generate(GenerateTable *table, AssemblyNode *&tail) {
    saveTemp(table, tail);
    linkToTail(tail, new LabelAssembly(name));
}

void Goto::print() { printToFile(immediateFile, "GOTO %s\n", label.c_str()); }

void Goto::generate(GenerateTable *table, AssemblyNode *&tail) {
    saveTemp(table, tail);
    linkToTail(tail, new J(label));
}

void CondGoto::print() {
    printToFile(immediateFile, "IF %s %s %s GOTO %s\n", lhs.ident.c_str(), op.c_str(), rhs.ident.c_str(),
                label.c_str());
}

void CondGoto::generate(GenerateTable *table, AssemblyNode *&tail) {
    Register lhsReg = table->allocateReg(lhs.ident, tail, true);
    Register rhsReg = table->allocateReg(rhs.ident, tail, true);
    saveTemp(table, tail);
    linkToTail(tail, new Branch(lhsReg, rhsReg, label, op));
    table->free(lhs.ident, lhsReg, tail, false);
    table->free(rhs.ident, rhsReg, tail, false);
}

void FuncDefNode::print() { printToFile(immediateFile, "FUNCTION %s:\n", name.ident.c_str()); }

static void livenessAnalysisFunc(GenerateTable *table, std::vector<IRNode *> &nodes, FuncDefNode *func) {
    table->labelMap.clear();
    int index = 0;
    for (IRNode *cur = func->next; cur != nullptr; cur = cur->next) {
        if (typeid(*cur) == typeid(FuncDefNode)) {
            break;
        }
        if (typeid(*cur) == typeid(Label)) {
            table->labelMap[static_cast<Label *>(cur)->getName()] = cur;
        }
        cur->index = index++;
        nodes.push_back(cur);
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = nodes.size() - 1; i >= 0; i--) {
            changed |= nodes[i]->livenessAnalysis(table);
        }
    }
}

static void linearScan(GenerateTable *table, std::vector<IRNode *> &nodes, FuncDefNode *func) {
    table->varIntervals.clear();
    for (auto i = 0ull; i < nodes.size(); ++i) {
        for (auto ident : nodes[i]->out) {
            if (table->varIntervals.find(ident) == table->varIntervals.end()) {
                table->varIntervals[ident] = VarInterval(ident, i, i + 1);
            } else {
                table->varIntervals[ident].end = i + 1;
            }
        }
    }

    table->live.clear();
    std::map<VarInterval, int, std::greater<VarInterval>> active;
    std::unordered_set<int> freeRegisters;
    for (auto i : SAVED_REGISTERS) {
        freeRegisters.insert(i);
    }
    for (auto interval : table->varIntervals) {
        table->live.emplace_back(interval.second);
    }
    std::sort(table->live.begin(), table->live.end());

    // allocate registers
    for (auto i : table->live) {
        // if already allocated(such as function arguments)
        if (table->identReg.find(i.ident) != table->identReg.end()) {
            continue;
        }

        // expire old intervals
        for (auto j = active.begin(); j != active.end();) {
            if (j->first.end >= i.start) {
                break;
            }
            freeRegisters.insert(j->second);
            table->identReg[j->first.ident] = j->second;
            j = active.erase(j);
        }

        if (freeRegisters.empty()) {
            // spill
            auto spill = active.begin();
            if (spill->first.end > i.end) {
                int reg = spill->second;
                table->insertStack(spill->first.ident, SIZE_OF_INT);
                active.erase(spill);
                active[i] = reg;
            } else {
                table->insertStack(i.ident, SIZE_OF_INT);
            }
        } else {
            int reg = *freeRegisters.begin();
            freeRegisters.erase(reg);
            active[i] = reg;
        }
    }
    for (auto i : active) {
        table->identReg[i.first.ident] = i.second;
    }
}

void FuncDefNode::generate(GenerateTable *table, AssemblyNode *&tail) {
    // init
    table->identStackOffset.clear();
    table->identReg.clear();
    table->arraySet.clear();
    table->stackOffset = 0;
    table->curParamCount = 0;
    table->curArgCount = 0;
    table->curStackPreserve = 0;
    table->regState = std::vector<short>(NUM_OF_REG, 0);
    table->tempReg = std::vector<std::string>(TEMP_REGISTERS.size(), "");

    // prologue
    for (IRNode *cur = this->next; cur != nullptr; cur = cur->next) {
        if (typeid(*cur) == typeid(FuncDefNode)) {
            break;
        }
        cur->prologue(table);
    }
    table->insertStack("_ra", SIZE_OF_INT);

    // liveness analysis
    std::vector<IRNode *> nodes;
    livenessAnalysisFunc(table, nodes, this);
    linearScan(table, nodes, this);

    // set size for stack of saved registers
    std::unordered_set<int> savedRegs;
    for (auto i : table->identReg) {
        if (std::find(SAVED_REGISTERS.begin(), SAVED_REGISTERS.end(), i.second) != SAVED_REGISTERS.end() &&
            savedRegs.find(i.second) == savedRegs.end()) {
            savedRegs.emplace(i.second);
            table->insertStack("_" + REGISTER_NAMES[i.second], SIZE_OF_INT);
        }
    }

    // set call node's save context size
    for (auto cur : nodes) {
        if (typeid(*cur) == typeid(Call) || typeid(*cur) == typeid(CallWithRet)) {
            static_cast<CallNode *>(cur)->saveContextSize(table);
        }
    }

    // generate assembly
    table->stackOffset += table->curStackPreserve;
    linkToTail(tail, new LabelAssembly(name.ident));
    if (table->stackOffset > 2048) {
        throw std::runtime_error("TODO: prologue size too large");
    } else {
        linkToTail(tail, new BinaryImmAssembly(Register(2), Register(2), ImmAssembly(-table->stackOffset), "+"));
    }
    // sw
    linkToTail(tail, new Sw(Register(1), Register(2), table->getStackOffset("_ra")));
    for (auto i : savedRegs) {
        linkToTail(tail, new Sw(Register(i), Register(2), table->getStackOffset("_" + REGISTER_NAMES[i])));
    }
}

int CallNode::saveContextSize(GenerateTable *table) {
    int size = 0;
    for (auto i : table->live) {
        if (i.ident == lhs.ident) {
            continue;
        }
        if (i.start < index && i.end > index) {
            if (table->identReg.find(i.ident) != table->identReg.end()) {
                savedIdent.emplace_back(i.ident);
                size += table->insertStack(i.ident, SIZE_OF_INT);
            }
        }
    }
    return size;
}

void CallNode::saveContext(GenerateTable *table, AssemblyNode *&tail) {
    for (auto i : savedIdent) {
        if (i == lhs.ident) {
            continue;
        }
        Register reg = table->allocateReg(i, tail, true);
        linkToTail(tail, new Sw(reg, Register(2), table->getStackOffset(i)));
        table->free(i, reg, tail, false);
    }
}

void CallNode::loadContext(GenerateTable *table, AssemblyNode *&tail) {
    for (auto i : savedIdent) {
        if (i == lhs.ident) {
            continue;
        }
        Register reg = table->allocateReg(i, tail, false);
        linkToTail(tail, new Lw(reg, Register(2), table->getStackOffset(i)));
        table->free(i, reg, tail, true);
    }
}

void CallWithRet::print() { printToFile(immediateFile, "%s = CALL %s\n", lhs.ident.c_str(), name.c_str()); }

void CallWithRet::generate(GenerateTable *table, AssemblyNode *&tail) {
    if (table->curArgCount == 0) {
        saveContext(table, tail);
    }
    saveTemp(table, tail);
    linkToTail(tail, new CallAssembly(name));
    Register lhsReg = table->allocateReg(lhs.ident, tail, false);
    linkToTail(tail, new Mv(lhsReg, Register(10)));
    table->free(lhs.ident, lhsReg, tail, true);
    table->curArgCount = 0;
    loadContext(table, tail);
}

void Call::print() { printToFile(immediateFile, "CALL %s\n", name.c_str()); }

void Call::generate(GenerateTable *table, AssemblyNode *&tail) {
    if (table->curArgCount == 0) {
        saveContext(table, tail);
    }
    saveTemp(table, tail);
    linkToTail(tail, new CallAssembly(name));
    loadContext(table, tail);
    table->curArgCount = 0;
}

void Param::print() { printToFile(immediateFile, "PARAM %s\n", ident.ident.c_str()); }

int Param::prologue(GenerateTable *table) {
    ++table->curParamCount;
    if (table->curParamCount <= 8) {
        table->identReg[ident.ident] = ARG_REGISTERS[table->curParamCount - 1];
        table->regState[ARG_REGISTERS[table->curParamCount - 1]] |= 1;
    } else {
        table->insertStack(ident.ident, -(table->curParamCount - 9) * 4);
    }
    return 0;
}

void Param::generate(GenerateTable *table, AssemblyNode *&tail) {}

void Arg::print() { printToFile(immediateFile, "ARG %s\n", ident.ident.c_str()); }

int Arg::prologue(GenerateTable *table) {
    ++table->curArgCount;
    if (table->curArgCount > 8) {
        table->curStackPreserve = std::max(table->curStackPreserve, (table->curArgCount - 8) * SIZE_OF_INT);
    }
    return 0;
}

void Arg::generate(GenerateTable *table, AssemblyNode *&tail) {
    ++table->curArgCount;
    if (table->curArgCount == 1) {
        // save context
        for (auto cur = this->next; cur != nullptr; cur = cur->next) {
            if (typeid(*cur) == typeid(Call) || typeid(*cur) == typeid(CallWithRet)) {
                static_cast<CallNode *>(cur)->saveContext(table, tail);
                break;
            }
        }
    }
    if (table->curArgCount <= 8) {
        Register argReg = table->allocateReg(ident.ident, tail, true);
        linkToTail(tail, new Mv(Register(table->curArgCount + 9), argReg));
    } else {
        Register argReg = table->allocateReg(ident.ident, tail, true);
        linkToTail(tail, new Sw(argReg, Register(2), (table->curArgCount - 9) * SIZE_OF_INT));
    }
}

static void epilogue(GenerateTable *table, AssemblyNode *&tail) {
    std::unordered_set<int> savedRegs;
    for (auto i : table->identReg) {
        if (std::find(SAVED_REGISTERS.begin(), SAVED_REGISTERS.end(), i.second) != SAVED_REGISTERS.end() &&
            savedRegs.find(i.second) == savedRegs.end()) {
            savedRegs.emplace(i.second);
            linkToTail(tail,
                       new Lw(Register(i.second), Register(2), table->getStackOffset("_" + REGISTER_NAMES[i.second])));
        }
    }
    linkToTail(tail, new Lw(Register(1), Register(2), table->getStackOffset("_ra")));  // ra
    linkToTail(tail, new BinaryImmAssembly(Register(2), Register(2), ImmAssembly(table->stackOffset), "+"));
}

void ReturnWithVal::print() { printToFile(immediateFile, "RETURN %s\n", ident.ident.c_str()); }

void ReturnWithVal::generate(GenerateTable *table, AssemblyNode *&tail) {
    Register retReg = table->allocateReg(ident.ident, tail, true);
    linkToTail(tail, new Mv(Register(10), retReg));
    saveTemp(table, tail);
    epilogue(table, tail);
    linkToTail(tail, new Ret());
}

void Return::print() { printToFile(immediateFile, "RETURN\n"); }

void Return::generate(GenerateTable *table, AssemblyNode *&tail) {
    saveTemp(table, tail);
    epilogue(table, tail);
    linkToTail(tail, new Ret());
}

void VarDec::print() { printToFile(immediateFile, "DEC %s #%d\n", ident.ident.c_str(), size); }

void VarDec::generate(GenerateTable *table, AssemblyNode *&tail) {}

void GlobalVar::print() { printToFile(immediateFile, "GLOBAL %s:\n", ident.ident.c_str()); }

void GlobalVar::generate(GenerateTable *table, AssemblyNode *&tail) {
    linkToTail(tail, new LabelAssembly(ident.ident));
}

void LoadGlobal::print() { printToFile(immediateFile, "%s = &%s\n", lhs.ident.c_str(), rhs.ident.c_str()); }

void LoadGlobal::generate(GenerateTable *table, AssemblyNode *&tail) {
    Register lhsReg = table->allocateReg(lhs.ident, tail, false);
    linkToTail(tail, new La(lhsReg, rhs.ident));
    table->free(lhs.ident, lhsReg, tail, true);
}

void Word::print() { printToFile(immediateFile, ".WORD #%d\n", imm.value); }

void Word::generate(GenerateTable *table, AssemblyNode *&tail) {
    linkToTail(tail, new WordAssembly(ImmAssembly(imm.value)));
}