#ifndef _IR_H_
#define _IR_H_

#include "assembly.h"
#include "common.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <vector>
#include <stdexcept>

class IRNode;
class FuncDefNode;
class GenerateTable {
   public:
    int insert(std::string ident, int size, bool storeAddr = false);  // storeAddr is true for local array
    void insertStack(std::string ident) { spillParams.push_back(ident); }
    Register allocateTemp(std::string ident, AssemblyNode *&tail, bool needLoad);
    Register allocateArg(std::string ident, AssemblyNode *&tail);
    void free(Register reg, AssemblyNode *&tail, bool needStore);
    void clear(Register reg, AssemblyNode *&tail, bool needStore);

    void clearStack();
    void clearPreservedUsed() { preservedUsed = 0; }
    bool identExists(std::string ident) { return identMap.find(ident) != identMap.end(); }
    int getOffset(std::string ident) { return identMap[ident] + preservedOffset; }
    int getStackOffset() { return stackOffset; }
    void setPreserved(int size) { preservedOffset = std::max(preservedOffset, size); }
    int getPreserved() { return preservedOffset; }

    void insertGlobal(std::string ident) { globalSet.insert(ident); }

    void insertLabel(std::string name, IRNode *label) { labelMap[name] = label; }
    IRNode *getLabel(std::string name) { return labelMap[name]; }
    void clearLabel() { labelMap.clear(); }

    FuncDefNode *curFunction;

    int curParam;  // current function parameter count
    std::vector<std::string> spillParams;
    std::vector<std::string> params;

    int preservedOffsetCurCall = 0;

   private:
    void allocateReg(Register reg, int offset, AssemblyNode *&tail, bool isArray, bool needLoad);
    int stackOffset = 0;
    int preservedOffset = 0;                        // preserve for more args
    int preservedUsed = 0;                          // used for more args
    std::unordered_map<std::string, int> identMap;  // identifier -> offset(negative is array)
    std::unordered_set<std::string> globalSet;
    std::unordered_set<std::string> arraySet;
    std::string registers[32];
    int regUsed[32] = {0};  // low bit = 1 if register is used, high bit = 1 if register need sw
    int lastVictim = 0;     // last victim register index in TEMP_REGISTERS
    std::unordered_map<std::string, IRNode *> labelMap;
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
    virtual bool livenessAnalysis(GenerateTable *table) { return _livenessAnalysis(next); }
    virtual int prologue(GenerateTable *table) { return 0; }
    virtual void generate(GenerateTable *table, AssemblyNode *&tail) { throw "IRNode::generate() not implemented!"; }
    IRNode *next = nullptr;

    // FIXME: FOR DEBUG
    void printTest() {
        std::printf("%s\n", typeid(*this).name());
        // print use, def, in, out
        std::printf("use: ");
        for (auto &u : use) {
            std::printf("%s ", u.c_str());
        }
        std::printf("\ndef: ");
        for (auto &d : def) {
            std::printf("%s ", d.c_str());
        }
        std::printf("\nin: ");
        for (auto &i : in) {
            std::printf("%s ", i.c_str());
        }
        std::printf("\nout: ");
        for (auto &o : out) {
            std::printf("%s ", o.c_str());
        }
        std::printf("\n\n");
    }

   protected:
    std::unordered_set<std::string> use, def;
    std::unordered_set<std::string> in, out;
    bool _livenessAnalysis(IRNode *next);
    bool _livenessAnalysis(IRNode *first, IRNode *second);
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
    int prologue(GenerateTable *table) override { return table->insert(ident.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier ident;
    Immediate value;
};

class Assign : public IRNode {
   public:
    Assign(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {
        use.emplace(rhs.ident);
        def.emplace(lhs.ident);
    }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
};

class Binop : public IRNode {
   public:
    Binop(Identifier lhs, Identifier rhs1, Identifier rhs2, std::string op) : lhs(lhs), rhs1(rhs1), rhs2(rhs2), op(op) {
        use.emplace(rhs1.ident);
        use.emplace(rhs2.ident);
        def.emplace(lhs.ident);
    }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs1, rhs2;
    std::string op;
};

class BinopImm : public IRNode {
   public:
    BinopImm(Identifier lhs, Identifier rhs, Immediate imm, std::string op) : lhs(lhs), rhs(rhs), imm(imm), op(op) {
        use.emplace(rhs.ident);
        def.emplace(lhs.ident);
    }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
    Immediate imm;
    std::string op;
};

class Unop : public IRNode {
   public:
    Unop(Identifier lhs, Identifier rhs, std::string op) : lhs(lhs), rhs(rhs), op(op) {
        use.emplace(rhs.ident);
        def.emplace(lhs.ident);
    }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
    std::string op;
};

class Load : public IRNode {
   public:
    Load(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {
        use.emplace(rhs.ident);
        def.emplace(lhs.ident);
    }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(lhs.ident, SIZE_OF_INT); }
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
    bool livenessAnalysis(GenerateTable *table) override { return _livenessAnalysis(table->getLabel(label)); }
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
    bool livenessAnalysis(GenerateTable *table) override { return _livenessAnalysis(next, table->getLabel(label)); }
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

    Identifier getName() { return name; }

   private:
    Identifier name;
};

class CallWithRet : public IRNode {
   public:
    CallWithRet(Identifier lhs, std::string name, int argCount) : lhs(lhs), name(name), argCount(argCount) {
        def.emplace(lhs.ident);
    }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(lhs.ident, SIZE_OF_INT); }
    bool livenessAnalysis(GenerateTable *table) override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs;
    std::string name;
    int argCount;
};

class Call : public IRNode {
   public:
    Call(std::string name, int argCount) : name(name), argCount(argCount) {}
    void print() override;
    int prologue(GenerateTable *table) override {
        table->preservedOffsetCurCall = 0;
        return std::max(0, argCount - 8) * SIZE_OF_INT;
    }
    bool livenessAnalysis(GenerateTable *table) override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    std::string name;
    int argCount;
};

class Param : public IRNode {
   public:
    Param(Identifier ident) : ident(ident) { def.emplace(ident.ident); }
    void print() override;
    int prologue(GenerateTable *table) override {
        ++table->curParam;
        table->params.push_back(ident.ident);
        if (table->curParam <= 8) {
            return table->insert(ident.ident, SIZE_OF_INT);
        } else {
            table->insertStack(ident.ident);
            return 0;
        }
    }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier ident;
};

class Arg : public IRNode {
   public:
    Arg(Identifier ident) : ident(ident) { use.emplace(ident.ident); }
    void print() override;
    int prologue(GenerateTable *table) override {
        ++table->preservedOffsetCurCall;
        if (table->preservedOffsetCurCall > 8) {
            table->setPreserved((table->preservedOffsetCurCall - 8) * SIZE_OF_INT);
        }
        return 0;
    }
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
    VarDec(Identifier ident, Immediate size) : ident(ident), size(size) { def.emplace(ident.ident); }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(ident.ident, size.value, true); }
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
    LoadGlobal(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) { def.emplace(lhs.ident); }
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(lhs.ident, SIZE_OF_INT); }
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