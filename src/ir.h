#ifndef IR_H
#define IR_H

#include <string>

extern FILE *immediateFile;

class IRNode {
   public:
    IRNode() {}
    virtual ~IRNode() {
        if (next != this) {
            delete next;
        }
    }

    virtual void print() { throw "IRNode::print() not implemented!"; }
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

   private:
    Identifier ident;
    Immediate value;
};

class Assign : public IRNode {
   public:
    Assign(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {}
    void print() override;

   private:
    Identifier lhs, rhs;
};

class Binop : public IRNode {
   public:
    Binop(Identifier lhs, Identifier rhs1, Identifier rhs2, std::string op)
        : lhs(lhs), rhs1(rhs1), rhs2(rhs2), op(op) {}
    void print() override;

   private:
    Identifier lhs, rhs1, rhs2;
    std::string op;
};

class BinopImm : public IRNode {
   public:
    BinopImm(Identifier lhs, Identifier rhs, Immediate imm, std::string op) : lhs(lhs), rhs(rhs), imm(imm), op(op) {}
    void print() override;

   private:
    Identifier lhs, rhs;
    Immediate imm;
    std::string op;
};

class Unop : public IRNode {
   public:
    Unop(Identifier lhs, Identifier rhs, std::string op) : lhs(lhs), rhs(rhs), op(op) {}
    void print() override;

   private:
    Identifier lhs, rhs;
    std::string op;
};

class Load : public IRNode {
   public:
    Load(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {}
    void print() override;

   private:
    Identifier lhs, rhs;
};

class Store : public IRNode {
   public:
    Store(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {}
    void print() override;

   private:
    Identifier lhs, rhs;
};

class Label : public IRNode {
   public:
    Label(std::string name) : name(name) {}
    void print() override;

   private:
    std::string name;
};

class Goto : public IRNode {
   public:
    Goto(std::string label) : label(label) {}
    void print() override;

   private:
    std::string label;
};

class CondGoto : public IRNode {
   public:
    CondGoto(Identifier lhs, Identifier rhs, std::string op, std::string label)
        : lhs(lhs), rhs(rhs), op(op), label(label) {}
    void print() override;

   private:
    Identifier lhs, rhs;
    std::string op, label;
};

class FuncDefNode : public IRNode {
   public:
    FuncDefNode(Identifier name) : name(name) {}
    void print() override;

   private:
    Identifier name;
};

class CallWithRet : public IRNode {
   public:
    CallWithRet(Identifier lhs, std::string name) : lhs(lhs), name(name) {}
    void print() override;

   private:
    Identifier lhs;
    std::string name;
};

class Call : public IRNode {
   public:
    Call(std::string name) : name(name) {}
    void print() override;

   private:
    std::string name;
};

class Param : public IRNode {
   public:
    Param(Identifier ident) : ident(ident) {}
    void print() override;

   private:
    Identifier ident;
};

class Arg : public IRNode {
   public:
    Arg(Identifier ident) : ident(ident) {}
    void print() override;

   private:
    Identifier ident;
};

class ReturnWithVal : public IRNode {
   public:
    ReturnWithVal(Identifier ident) : ident(ident) {}
    void print() override;

   private:
    Identifier ident;
};

class Return : public IRNode {
   public:
    Return() {}
    void print() override;
};

class VarDec : public IRNode {
   public:
    VarDec(Identifier ident, Immediate size) : ident(ident), size(size) {}
    void print() override;

   private:
    Identifier ident;
    Immediate size;
};

class GlobalVar : public IRNode {
   public:
    GlobalVar(Identifier ident) : ident(ident) {}
    void print() override;

   private:
    Identifier ident;
};

class LoadGlobal : public IRNode {
   public:
    LoadGlobal(Identifier lhs, Identifier rhs) : lhs(lhs), rhs(rhs) {}
    void print() override;

   private:
    Identifier lhs, rhs;
};

class Word : public IRNode {
   public:
    Word(Immediate imm) : imm(imm) {}
    void print() override;

   private:
    Immediate imm;
};

#endif