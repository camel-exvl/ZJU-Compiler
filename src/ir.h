#ifndef _IR_H_
#define _IR_H_

#include "assembly.h"
#include "common.h"
#include <string>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <algorithm>

class IRNode;

class VarInterval {
   public:
    VarInterval() = default;
    VarInterval(std::string ident, int start, int end) : ident(ident), start(start), end(end) {}
    bool operator<(const VarInterval &other) const { return start < other.start; }
    bool operator>(const VarInterval &other) const {
        return std::tie(end, start, ident) > std::tie(other.end, other.start, other.ident);
    }

    std::string ident;
    int start;
    int end;
};

class GenerateTable {
   public:
    int insertStack(std::string ident, int size);
    Register allocateReg(std::string ident, AssemblyNode *&tail, bool needLoad);
    void free(std::string ident, Register reg, AssemblyNode *&tail, bool needStore);

    int curArgCount = 0;
    int stackOffset = 0;
    std::unordered_map<std::string, int> identStackOffset;  // ident -> stack offset
    std::unordered_map<std::string, int> identReg;          // ident -> register index
    std::unordered_set<std::string> arraySet;               // arrays
    std::vector<bool> regState = std::vector<bool>(
        NUM_OF_REG, false);  // register index -> is clean for saved registers, is used for temp registers
    std::unordered_map<std::string, IRNode *> labelMap;         // label -> IRNode
    std::unordered_map<std::string, VarInterval> varIntervals;  // ident -> VarInterval
};

class IRNode {
   public:
    IRNode() {}
    virtual ~IRNode() {
        if (next != this) {
            delete next;
        }
    }

    virtual void print() { throw "IRNode::print() not implemented!"; }
    virtual int prologue(GenerateTable *table) { return 0; }
    virtual bool livenessAnalysis(GenerateTable *table) { return _livenessAnalysis(next); }
    virtual void generate(GenerateTable *table, AssemblyNode *&tail) { throw "IRNode::generate() not implemented!"; }

    IRNode *next = nullptr;
    int index;

    std::unordered_set<std::string> use, def;
    std::unordered_set<std::string> in, out;

   protected:
    virtual bool _livenessAnalysis(IRNode *next, IRNode *second = nullptr);
};

class Immediate {
   public:
    Immediate(int value) : value(value) {}
    int value;
};

class Identifier {
   public:
    Identifier(std::string ident) : ident(ident) {}
    std::string ident;
};

class LoadImm : public IRNode {
   public:
    LoadImm(Identifier ident, Immediate value) : ident(ident), value(value) { def.emplace(ident.ident); }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insertStack(ident.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier ident;
    Immediate value;
};

class Assign : public IRNode {
   public:
    Assign(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {
        def.emplace(lhs.ident);
        use.emplace(rhs.ident);
    }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insertStack(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
};

class Binop : public IRNode {
   public:
    Binop(Identifier lhs, Identifier rhs1, Identifier rhs2, std::string op) : lhs(lhs), rhs1(rhs1), rhs2(rhs2), op(op) {
        def.emplace(lhs.ident);
        use.emplace(rhs1.ident);
        use.emplace(rhs2.ident);
    }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insertStack(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs1, rhs2;
    std::string op;
};

class BinopImm : public IRNode {
   public:
    BinopImm(Identifier lhs, Identifier rhs, Immediate imm, std::string op) : lhs(lhs), rhs(rhs), imm(imm), op(op) {
        def.emplace(lhs.ident);
        use.emplace(rhs.ident);
    }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insertStack(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
    Immediate imm;
    std::string op;
};

class Unop : public IRNode {
   public:
    Unop(Identifier lhs, Identifier rhs, std::string op) : lhs(lhs), rhs(rhs), op(op) {
        def.emplace(lhs.ident);
        use.emplace(rhs.ident);
    }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insertStack(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
    std::string op;
};

class Load : public IRNode {
   public:
    Load(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {
        def.emplace(lhs.ident);
        use.emplace(rhs.ident);
    }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insertStack(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
};

class Store : public IRNode {
   public:
    Store(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {
        use.emplace(lhs.ident);
        use.emplace(rhs.ident);
    }
    void print() override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
};

class Label : public IRNode {
   public:
    Label(std::string name) : name(name) {}
    void print() override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

    std::string getName() { return name; }

   private:
    std::string name;
};

class Goto : public IRNode {
   public:
    Goto(std::string label) : label(label) {}
    void print() override;
    bool livenessAnalysis(GenerateTable *table) override { return _livenessAnalysis(table->labelMap[label]); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    std::string label;
};

class CondGoto : public IRNode {
   public:
    CondGoto(Identifier lhs, Identifier rhs, std::string op, std::string label)
        : lhs(lhs), rhs(rhs), op(op), label(label) {
        use.emplace(lhs.ident);
        use.emplace(rhs.ident);
    }
    void print() override;
    bool livenessAnalysis(GenerateTable *table) override { return _livenessAnalysis(next, table->labelMap[label]); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
    std::string op, label;
};

class FuncDefNode : public IRNode {
   public:
    FuncDefNode(Identifier name) : name(name) {}
    void print() override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier name;
};

class CallWithRet : public IRNode {
   public:
    CallWithRet(Identifier lhs, std::string name) : lhs(lhs), name(name) { def.emplace(lhs.ident); }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insertStack(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs;
    std::string name;
};

class Call : public IRNode {
   public:
    Call(std::string name) : name(name) {}
    void print() override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    std::string name;
};

class Param : public IRNode {
   public:
    Param(Identifier ident) : ident(ident) { def.emplace(ident.ident); }
    void print() override;
    int prologue(GenerateTable *table) override {
        ++table->curArgCount;
        if (table->curArgCount <= 8) {
            table->identReg[ident.ident] = ARG_REGISTERS[table->curArgCount - 1];
            return 0;
        } else {
            // TODO:
        }
        return 0;
    }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier ident;
};

class Arg : public IRNode {
   public:
    Arg(Identifier ident) : ident(ident) { use.emplace(ident.ident); }
    void print() override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier ident;
};

class ReturnWithVal : public IRNode {
   public:
    ReturnWithVal(Identifier ident) : ident(ident) { use.emplace(ident.ident); }
    void print() override;
    bool livenessAnalysis(GenerateTable *table) override { return _livenessAnalysis(nullptr); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier ident;
};

class Return : public IRNode {
   public:
    Return() {}
    void print() override;
    bool livenessAnalysis(GenerateTable *table) override { return _livenessAnalysis(nullptr); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;
};

class VarDec : public IRNode {
   public:
    VarDec(Identifier ident, Immediate size) : ident(ident), size(size) {}
    void print() override;
    int prologue(GenerateTable *table) override {
        table->arraySet.emplace(ident.ident);
        return table->insertStack(ident.ident, size.value);
    }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier ident;
    Immediate size;
};

class GlobalVar : public IRNode {
   public:
    GlobalVar(Identifier ident) : ident(ident) {}
    void print() override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier ident;
};

class LoadGlobal : public IRNode {
   public:
    LoadGlobal(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {}
    void print() override;
    int prologue(GenerateTable *table) override { return table->insertStack(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
};

class Word : public IRNode {
   public:
    Word(Immediate imm) : imm(imm) {}
    void print() override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Immediate imm;
};

#endif