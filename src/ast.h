#ifndef _AST_H_
#define _AST_H_

#include <cstdio>
#include <vector>
#include <stack>
#include <list>
#include <string>
#include <unordered_map>
#include <stdexcept>

class Type;
class Table;

extern void yyerror(const char *s);
extern Table *globalTable;

enum class SimpleKind { SCOPE, UNKNOWN, INT, VOID };
enum class TypeKind { SIMPLE, ARRAY, FUNC };

struct ARRAYVAL;
struct FUNCVAL;
typedef union TYPEVAL {
    SimpleKind simple;
    struct ARRAYVAL *array;
    struct FUNCVAL *func;
} TypeVal;

static std::vector<bool> lastFlags(8, false);

class Type {
   public:
    Type() : kind_(TypeKind::SIMPLE), val_({SimpleKind::UNKNOWN}) {}
    Type(TypeKind kind, TypeVal val_) : kind_(kind), val_(val_) {}

    bool operator==(const Type &other) const;
    bool operator!=(const Type &other) const { return !(*this == other); }
    bool isScope() const { return kind_ == TypeKind::SIMPLE && val_.simple == SimpleKind::SCOPE; }
    std::string toString();

    TypeKind getKind() { return kind_; }
    void setKind(TypeKind kind) { kind_ = kind; }
    TypeVal getVal() { return val_; }
    void setVal(TypeVal val) { val_ = val; }

   private:
    TypeKind kind_;
    TypeVal val_;
};

typedef struct ARRAYVAL {
    std::vector<int> size;
    Type type;
} ArrayVal;

typedef struct FUNCVAL {
    std::vector<Type> params;
    Type ret;
} FuncVal;

class Table {
   public:
    Table() {}

    void insert(const char *name, Type type);
    Type lookup(const char *name);
    void enterScope() {
        undo_.push(std::string());
    }
    void exitScope();

   private:
    std::unordered_map<std::string, std::stack<Type>> table_;
    std::stack<std::string> undo_;
};

class BaseStmt {
   public:
    virtual ~BaseStmt() {}
    virtual Type typeCheck(Table *table) = 0;
    virtual void print(int indent = 0, bool last = false) = 0;
    static void printIndent(int indent = 0, bool last = false);
};

class Exp : public BaseStmt {};

class Ident : public Exp {
   public:
    Ident(const char *name) : name_(name) {}

    Type typeCheck(Table *table) override { return table->lookup(name_); }
    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("Ident: %s\n", name_);
    }

   private:
    const char *name_;
};

class IntConst : public Exp {
   public:
    IntConst(int val) : val_(val) {}

    Type typeCheck(Table *table) override { return Type(TypeKind::SIMPLE, {SimpleKind::INT}); }
    int getValue() { return val_; }
    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("IntConst: %d\n", val_);
    }

   private:
    int val_;
};

class ExprStmt : public BaseStmt {
   public:
    ExprStmt(Exp *expr) : expr_(expr) {}
    ~ExprStmt() { delete expr_; }

    Type typeCheck(Table *table) override { return expr_->typeCheck(table); }
    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        expr_->print(indent + 1, true);
    }

   private:
    Exp *expr_;
};

class CompUnit : public BaseStmt {
   public:
    CompUnit(BaseStmt *stmt) { stmts_.push_back(stmt); }
    ~CompUnit() {
        for (auto stmt : stmts_) {
            delete stmt;
        }
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;
    void append(BaseStmt *stmt) { stmts_.push_back(stmt); }

   private:
    std::vector<BaseStmt *> stmts_;
};

class TypeDecl : public BaseStmt {
   public:
    TypeDecl(Type type) : type_(type) {}

    Type typeCheck(Table *table) override { return type_; }
    void print(int indent = 0, bool last = false) override { printf("%s", type_.toString().c_str()); }

   private:
    Type type_;
};

class InitVal : public BaseStmt {
   public:
    InitVal(BaseStmt *val, bool is_list_) : val_(val), is_list_(is_list_) {}
    ~InitVal() { delete val_; }

    Type typeCheck(Table *table) override {
        return val_ ? val_->typeCheck(table) : Type(TypeKind::SIMPLE, {SimpleKind::UNKNOWN});
    }
    void print(int indent = 0, bool last = false) override;

   private:
    BaseStmt *val_;
    bool is_list_;
};

class InitValList : public BaseStmt {
   public:
    InitValList() {}
    ~InitValList() {
        for (auto init_val : init_vals_) {
            delete init_val;
        }
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override {
        for (auto init_val : init_vals_) {
            init_val->print(indent, init_val == init_vals_.back());
        }
    }
    void append(InitVal *init_val) { init_vals_.push_back(init_val); }
    void appendHead(InitVal *init_val) { init_vals_.insert(init_vals_.begin(), init_val); }

   private:
    std::vector<InitVal *> init_vals_;
};

class ArrayDef : public BaseStmt {
   public:
    ArrayDef() {}
    ~ArrayDef() {
        for (auto dim : dims_) {
            delete dim;
        }
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override {
        for (auto dim : dims_) {
            dim->print(indent, last && dim == dims_.back());
        }
    }
    void append(int dim) { dims_.push_back(new IntConst(dim)); }

   private:
    std::vector<IntConst *> dims_;
};

class VarDef : public BaseStmt {
   public:
    VarDef(const char *name, ArrayDef *array_def, InitVal *init) : name_(name), array_def_(array_def), init_(init) {}
    ~VarDef() {
        delete array_def_;
        delete init_;
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;
    const char *getName() { return name_; }

   private:
    const char *name_;
    ArrayDef *array_def_;
    InitVal *init_;
};

class VarDefList : public BaseStmt {
   public:
    VarDefList() {}
    ~VarDefList() {
        for (auto def : defs_) {
            delete def;
        }
    }

    Type typeCheck(Table *table) override { throw std::runtime_error("VarDefList typeCheck is never called"); }
    void print(int indent = 0, bool last = false) override {
        for (auto def : defs_) {
            def->print(indent, def == defs_.back());
        }
    }
    void append(VarDef *def) { defs_.push_back(def); }
    std::vector<VarDef *> getDefs() { return defs_; }

   private:
    std::vector<VarDef *> defs_;
};

class VarDecl : public BaseStmt {
   public:
    VarDecl(TypeDecl *type, VarDefList *def_list) : type_(type), def_list_(def_list) {}
    ~VarDecl() {
        delete type_;
        delete def_list_;
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    TypeDecl *type_;
    VarDefList *def_list_;
};

class FuncFArrParam : public BaseStmt {
   public:
    FuncFArrParam() { dims_.push_back(nullptr); }
    ~FuncFArrParam() {
        for (auto dim : dims_) {
            delete dim;
        }
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;
    void append(int dim) { dims_.push_back(new IntConst(dim)); }

   private:
    std::vector<IntConst *> dims_;
};

class FuncFParam : public BaseStmt {
   public:
    FuncFParam(TypeDecl *ftype, const char *name, FuncFArrParam *arr_param_)
        : ftype_(ftype), name_(name), arr_param_(arr_param_) {}
    ~FuncFParam() {
        delete ftype_;
        delete arr_param_;
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    TypeDecl *ftype_;
    const char *name_;
    FuncFArrParam *arr_param_;
};

class FuncFParams : public BaseStmt {
   public:
    FuncFParams() {}
    ~FuncFParams() {
        for (auto fparam : fparams_) {
            delete fparam;
        }
    }

    Type typeCheck(Table *table) override { throw std::runtime_error("FuncFParams typeCheck is never called"); }
    void print(int indent = 0, bool last = false) override {
        for (auto fparam : fparams_) {
            fparam->print(indent, last && fparam == fparams_.back());
        }
    }
    void append(FuncFParam *fparam) { fparams_.push_back(fparam); }
    std::vector<FuncFParam *> getFParams() { return fparams_; }

   private:
    std::vector<FuncFParam *> fparams_;
};

class Block : public BaseStmt {
   public:
    Block() {}
    ~Block() {
        for (auto stmt : stmts_) {
            delete stmt;
        }
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;
    void append(BaseStmt *stmt) { stmts_.push_back(stmt); }

   private:
    std::vector<BaseStmt *> stmts_;
};

class FuncDef : public BaseStmt {
   public:
    FuncDef(TypeDecl *ftype, const char *name, FuncFParams *fparams, Block *body)
        : ftype_(ftype), name_(name), fparams_(fparams), body_(body) {}
    ~FuncDef() {
        delete ftype_;
        delete fparams_;
        delete body_;
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    TypeDecl *ftype_;
    const char *name_;
    FuncFParams *fparams_;
    Block *body_;
};

class LArrVal : public BaseStmt {
   public:
    LArrVal() {}
    ~LArrVal() {
        for (auto exp : dims_) {
            delete exp;
        }
    }

    Type typeCheck(Table *table) override { throw std::runtime_error("LArrVal typeCheck is never called"); }
    void print(int indent = 0, bool last = false) override {
        for (auto exp : dims_) {
            exp->print(indent, exp == dims_.back());
        }
    }
    void append(Exp *dim) { dims_.push_back(dim); }
    std::vector<Exp *> getDims() { return dims_; }

   private:
    std::vector<Exp *> dims_;
};

class LVal : public Exp {
   public:
    LVal(const char *name) : name_(name), arr_(nullptr) {}
    LVal(const char *name, LArrVal *arr) : name_(name), arr_(arr) {}
    ~LVal() { delete arr_; }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    const char *name_;
    LArrVal *arr_;
};

class AssignStmt : public BaseStmt {
   public:
    AssignStmt(LVal *lhs, Exp *rhs) : lhs_(lhs), rhs_(rhs) {}
    ~AssignStmt() {
        delete lhs_;
        delete rhs_;
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    LVal *lhs_;
    Exp *rhs_;
};

class EmptyStmt : public BaseStmt {
   public:
    Type typeCheck(Table *table) override { return Type(TypeKind::SIMPLE, {SimpleKind::VOID}); }
    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("EmptyStmt\n");
    }
};

class ExpStmt : public BaseStmt {
   public:
    ExpStmt(Exp *expr) : expr_(expr) {}
    ~ExpStmt() { delete expr_; }

    Type typeCheck(Table *table) override {
        expr_->typeCheck(table);
        return Type(TypeKind::SIMPLE, {SimpleKind::VOID});
    }
    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("ExpStmt\n");
        expr_->print(indent + 1, true);
    }

   private:
    Exp *expr_;
};

class IfStmt : public BaseStmt {
   public:
    IfStmt(Exp *cond, BaseStmt *then) : cond_(cond), then_(then), els_(nullptr) {}
    IfStmt(Exp *cond, BaseStmt *then, BaseStmt *els) : cond_(cond), then_(then), els_(els) {}
    ~IfStmt() {
        delete cond_;
        delete then_;
        delete els_;
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    Exp *cond_;
    BaseStmt *then_;
    BaseStmt *els_;
};

class WhileStmt : public BaseStmt {
   public:
    WhileStmt(Exp *cond, BaseStmt *body) : cond_(cond), body_(body) {}
    ~WhileStmt() {
        delete cond_;
        delete body_;
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    Exp *cond_;
    BaseStmt *body_;
};

class ReturnStmt : public BaseStmt {
   public:
    ReturnStmt() : ret_(nullptr) {}
    ReturnStmt(Exp *ret) : ret_(ret) {}
    ~ReturnStmt() { delete ret_; }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    Exp *ret_;
};

class PrimaryExp : public Exp {
   public:
    PrimaryExp(Exp *exp) : exp_(exp) {}
    ~PrimaryExp() { delete exp_; }

    Type typeCheck(Table *table) override { return exp_->typeCheck(table); }
    void print(int indent = 0, bool last = false) override { exp_->print(indent, last); }

   private:
    Exp *exp_;
};

class FuncRParams : public BaseStmt {
   public:
    FuncRParams() {}
    ~FuncRParams() {
        for (auto param : params_) {
            delete param;
        }
    }

    Type typeCheck(Table *table) override { throw std::runtime_error("FuncRParams typeCheck is never called"); }
    void print(int indent = 0, bool last = false) override {
        for (auto param : params_) {
            param->print(indent, param == params_.back());
        }
    }
    void append(Exp *param) { params_.push_back(param); }
    std::vector<Exp *> getParams() { return params_; }

   private:
    std::vector<Exp *> params_;
};

class CallExp : public Exp {
   public:
    CallExp(const char *name) : name_(name), params_(nullptr) {}
    CallExp(const char *name, FuncRParams *params) : name_(name), params_(params) {}
    ~CallExp() { delete params_; }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    const char *name_;
    FuncRParams *params_;
};

class UnaryExp : public Exp {
   public:
    UnaryExp(const char *&op, Exp *exp) : op_(op), exp_(exp) {}
    ~UnaryExp() { delete exp_; }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    const char *op_;
    Exp *exp_;
};

class BinaryExp : public Exp {
   public:
    BinaryExp(Exp *lhs, Exp *rhs, const char *op) : lhs_(lhs), rhs_(rhs), op_(op) {}
    ~BinaryExp() {
        delete lhs_;
        delete rhs_;
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    Exp *lhs_, *rhs_;
    const char *op_;
};

class RelExp : public Exp {
   public:
    RelExp(Exp *lhs, Exp *rhs, const char *op) : lhs_(lhs), rhs_(rhs), op_(op) {}
    ~RelExp() {
        delete lhs_;
        delete rhs_;
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    Exp *lhs_, *rhs_;
    const char *op_;
};

class LogicExp : public Exp {
   public:
    LogicExp(Exp *lhs, Exp *rhs, const char *op) : lhs_(lhs), rhs_(rhs), op_(op) {}
    ~LogicExp() {
        delete lhs_;
        delete rhs_;
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;

   private:
    Exp *lhs_, *rhs_;
    const char *op_;
};

#endif