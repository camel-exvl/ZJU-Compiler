#ifndef _ASSEMBLY_H_
#define _ASSEMBLY_H_

#include "common.h"
#include <string>
#include <stdexcept>

class AssemblyNode {
   public:
    AssemblyNode() {}
    virtual ~AssemblyNode() {
        if (next != this) {
            delete next;
        }
    }

    virtual void print() { throw "AssemblyNode::print() not implemented!"; }
    AssemblyNode *next = nullptr;
};

class Register {
   public:
    Register(int index) : index(index), name(REGISTER_NAMES[index]) {}

    int index;
    std::string name;
};

class ImmAssembly {
   public:
    ImmAssembly(int value) : value(value) {}
    int value;
};

class IdentAssembly {
   public:
    IdentAssembly(std::string ident) : ident(ident) {}
    std::string ident;
};

class BinaryAssembly : public AssemblyNode {
   public:
    BinaryAssembly(Register lhs, Register rhs1, Register rhs2, std::string op)
        : lhs(lhs), rhs1(rhs1), rhs2(rhs2), op(op) {}
    void print() override;

   private:
    Register lhs, rhs1, rhs2;
    std::string op;
};

class BinaryImmAssembly : public AssemblyNode {
   public:
    BinaryImmAssembly(Register lhs, Register rhs, ImmAssembly imm, std::string op)
        : lhs(lhs), rhs(rhs), imm(imm), op(op) {}
    void print() override;

   private:
    Register lhs, rhs;
    ImmAssembly imm;
    std::string op;
};

class Mv : public AssemblyNode {
   public:
    Mv(Register lhs, Register rhs) : lhs(lhs), rhs(rhs) {}
    void print() override;

   private:
    Register lhs, rhs;
};

class Li : public AssemblyNode {
   public:
    Li(Register lhs, ImmAssembly imm) : lhs(lhs), imm(imm) {}
    void print() override;

   private:
    Register lhs;
    ImmAssembly imm;
};

class LabelAssembly : public AssemblyNode {
   public:
    LabelAssembly(IdentAssembly label) : label(label) {}
    void print() override;

   private:
    IdentAssembly label;
};

class J : public AssemblyNode {
   public:
    J(IdentAssembly label) : label(label) {}
    void print() override;

   private:
    IdentAssembly label;
};

class CallAssembly : public AssemblyNode {
   public:
    CallAssembly(IdentAssembly label) : label(label) {}
    void print() override;

   private:
    IdentAssembly label;
};

class Ret : public AssemblyNode {
   public:
    Ret() {}
    void print() override;
};

class Lw : public AssemblyNode {
   public:
    Lw(Register lhs, Register rhs) : lhs(lhs), rhs(rhs), offset(0) {}
    Lw(Register lhs, Register rhs, ImmAssembly offset) : lhs(lhs), rhs(rhs), offset(offset) {}
    void print() override;

   private:
    Register lhs, rhs;
    ImmAssembly offset;
};

class Sw : public AssemblyNode {
   public:
    Sw(Register lhs, Register rhs) : lhs(lhs), rhs(rhs), offset(0) {}
    Sw(Register lhs, Register rhs, ImmAssembly offset) : lhs(lhs), rhs(rhs), offset(offset) {}
    void print() override;

   private:
    Register lhs, rhs;
    ImmAssembly offset;
};

class Branch : public AssemblyNode {
   public:
    Branch(Register lhs, Register rhs, IdentAssembly label, std::string op)
        : lhs(lhs), rhs(rhs), label(label), op(op) {}
    void print() override;

   private:
    Register lhs, rhs;
    IdentAssembly label;
    std::string op;
};

class La : public AssemblyNode {
   public:
    La(Register lhs, IdentAssembly ident) : lhs(lhs), ident(ident) {}
    void print() override;

   private:
    Register lhs;
    IdentAssembly ident;
};

class WordAssembly : public AssemblyNode {
   public:
    WordAssembly(ImmAssembly val) : val(val) {}
    void print() override;

   private:
    ImmAssembly val;
};

#endif