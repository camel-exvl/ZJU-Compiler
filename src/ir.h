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

class GenerateTable {
   public:
    int insert(std::string ident, int size, bool storeAddr = false);  // storeAddr is true for local array
    void insertStack(std::string ident) { spillParams.push_back(ident); }
    Register allocateTemp(std::string ident, AssemblyNode *&tail);
    Register allocateArg(std::string ident, AssemblyNode *&tail);
    void free(Register reg, AssemblyNode *&tail, bool storeAddr);

    void clearStack();
    void clearPreservedUsed() { preservedUsed = 0; }
    bool identExists(std::string ident) { return identMap.find(ident) != identMap.end(); }
    int getOffset(std::string ident) { return identMap[ident]; }
    int getStackOffset() { return stackOffset; }
    void setPreserved(int size) { preservedOffset = std::max(preservedOffset, size); }

    void insertGlobal(std::string ident) { globalSet.insert(ident); }

    int curParam;  // current function parameter count
    std::vector<std::string> spillParams;
    std::vector<std::string> params;
   private:
    void allocateReg(Register reg, int offset, AssemblyNode *&tail);
    int stackOffset = 0;
    int preservedOffset = 0;                        // preserve for more args
    int preservedUsed = 0;                          // used for more args
    std::unordered_map<std::string, int> identMap;  // identifier -> offset(negative is array)
    std::unordered_set<std::string> globalSet;
    std::string registers[32];
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
    virtual void generate(GenerateTable *table, AssemblyNode *&tail) { throw "IRNode::generate() not implemented!"; }
    IRNode *next = nullptr;
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
    LoadImm(Identifier ident, Immediate value) : ident(ident), value(value) {}
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(ident.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier ident;
    Immediate value;
};

class Assign : public IRNode {
   public:
    Assign(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {}
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
};

class Binop : public IRNode {
   public:
    Binop(Identifier lhs, Identifier rhs1, Identifier rhs2, std::string op)
        : lhs(lhs), rhs1(rhs1), rhs2(rhs2), op(op) {}
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs1, rhs2;
    std::string op;
};

class BinopImm : public IRNode {
   public:
    BinopImm(Identifier lhs, Identifier rhs, Immediate imm, std::string op) : lhs(lhs), rhs(rhs), imm(imm), op(op) {}
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
    Unop(Identifier lhs, Identifier rhs, std::string op) : lhs(lhs), rhs(rhs), op(op) {}
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
    std::string op;
};

class Load : public IRNode {
   public:
    Load(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {}
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(lhs.ident, SIZE_OF_INT); }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier lhs, rhs;
};

class Store : public IRNode {
   public:
    Store(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {}
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

   private:
    std::string name;
};

class Goto : public IRNode {
   public:
    Goto(std::string label) : label(label) {}
    void print() override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    std::string label;
};

class CondGoto : public IRNode {
   public:
    CondGoto(Identifier lhs, Identifier rhs, std::string op, std::string label)
        : lhs(lhs), rhs(rhs), op(op), label(label) {}
    void print() override;
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
    CallWithRet(Identifier lhs, std::string name, int argCount) : lhs(lhs), name(name), argCount(argCount) {}
    void print() override;
    int prologue(GenerateTable *table) override { return table->insert(lhs.ident, SIZE_OF_INT); }
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
    int prologue(GenerateTable *table) override { return std::max(0, argCount - 8) * SIZE_OF_INT; }
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    std::string name;
    int argCount;
};

class Param : public IRNode {
   public:
    Param(Identifier ident) : ident(ident) {}
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
    Arg(Identifier ident) : ident(ident) {}
    void print() override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier ident;
};

class ReturnWithVal : public IRNode {
   public:
    ReturnWithVal(Identifier ident) : ident(ident) {}
    void print() override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;

   private:
    Identifier ident;
};

class Return : public IRNode {
   public:
    Return() {}
    void print() override;
    void generate(GenerateTable *table, AssemblyNode *&tail) override;
};

class VarDec : public IRNode {
   public:
    VarDec(Identifier ident, Immediate size) : ident(ident), size(size) {}
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
    LoadGlobal(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {}
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