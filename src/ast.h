#ifndef _AST_H_
#define _AST_H_

#include <cstdio>
#include <vector>
#include <stack>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

/* Location type.  */
#if !defined YYLTYPE && !defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
};
#define YYLTYPE_IS_DECLARED 1
#define YYLTYPE_IS_TRIVIAL 1
#endif

class Type;
class Table;
class IntConst;

extern void error_handle(const char *s, YYLTYPE pos);
extern FILE *outputFile;

enum class SimpleKind { SCOPE, INT, VOID };
enum class TypeKind { UNKNOWN, SIMPLE, ARRAY, FUNC };

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
    Type() : kind_(TypeKind::UNKNOWN), val_({SimpleKind::VOID}) {}
    Type(TypeKind kind, TypeVal val_) : kind_(kind), val_(val_) {}
    Type(const Type &other);
    Type(Type &&other);
    ~Type();

    Type &operator=(Type &&other);
    bool operator==(const Type &other) const;
    bool operator!=(const Type &other) const { return !(*this == other); }
    bool isScope() const { return kind_ == TypeKind::SIMPLE && val_.simple == SimpleKind::SCOPE; }
    std::string toString(std::string name = "") const;

    TypeKind getKind() const { return kind_; }
    void setKind(TypeKind kind) { kind_ = kind; }
    TypeVal getVal() const { return val_; }
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

// for type check
class Table {
   public:
    Table() {}

    void insert(const char *name, const Type &type, YYLTYPE pos);
    Type lookup(const char *name, YYLTYPE pos);
    void enterScope();
    void exitScope();
    void setReturnType(Type *type) { return_type_ = type; }
    Type *getReturnType() { return return_type_; }

   private:
    std::unordered_map<std::string, std::list<Type>> table_;
    Type *return_type_;
};

// for translate
class SymbolTable {
   public:
    SymbolTable() {}

    std::string insert(std::string name);  // return the new name
    std::string lookup(std::string name);
    void insertArray(std::string name, std::vector<IntConst *> size) { array_table_[name] = size; }
    std::vector<IntConst *> lookupArray(std::string name);
    bool isArray(std::string name) { return array_table_.count(name); }
    void enterScope();
    void exitScope();
    std::string newTemp() { return "_t" + std::to_string(temp_count_++); }
    std::string newLabel() { return "_l" + std::to_string(label_count_++); }
    bool isGlobalLayer() { return layer_ == 1; }
    bool isGlobal(std::string name) { return global_table_.count(name); }

   private:
    std::unordered_map<std::string, std::list<std::string>> table_;
    std::unordered_set<std::string> global_table_;
    std::unordered_map<std::string, std::vector<IntConst *>> array_table_;
    int temp_count_ = 0;
    int label_count_ = 0;
    int layer_ = 0;
};

class BaseStmt {
   public:
    BaseStmt(YYLTYPE pos) : pos(pos) {}
    virtual ~BaseStmt() {}

    virtual Type typeCheck(Table *table) { throw std::runtime_error("typeCheck is not implemented for this class"); }
    virtual void translateStmt(SymbolTable *table, int indent) {
        throw std::runtime_error("translateStmt is not implemented for this class");
    }
    virtual void print(int indent = 0, bool last = false) = 0;
    static void printIndent(int indent = 0, bool last = false);
    YYLTYPE getPos() { return pos; }

   protected:
    YYLTYPE pos;
};

class Exp : public BaseStmt {
   public:
    Exp(YYLTYPE pos) : BaseStmt(pos) {}

    // if place is empty, the function will try to replace the value in place to decrease the number of temp variables
    virtual void translateExp(SymbolTable *table, std::string &place, int indent, bool ignoreReturn) {
        throw std::runtime_error("translateExp is not implemented for this class");
    }
    virtual void translateCond(SymbolTable *table, std::string trueLabel, std::string falseLabel, int indent);
};

class Ident : public Exp {
   public:
    Ident(YYLTYPE pos, const char *name) : Exp(pos), name_(name) {}

    Type typeCheck(Table *table) override { return table->lookup(name_, pos); }
    void translateExp(SymbolTable *table, std::string &place, int indent, bool ignoreReturn) override;
    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("Ident: %s\n", name_);
    }

   private:
    const char *name_;
};

class IntConst : public Exp {
   public:
    IntConst(YYLTYPE pos, int val) : Exp(pos), val_(val) {}

    Type typeCheck(Table *table) override { return Type(TypeKind::SIMPLE, {SimpleKind::INT}); }
    void translateExp(SymbolTable *table, std::string &place, int indent, bool ignoreReturn) override;
    int getValue() { return val_; }
    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("IntConst: %d\n", val_);
    }

   private:
    int val_;
};

class CompUnit : public BaseStmt {
   public:
    CompUnit(YYLTYPE pos, BaseStmt *stmt) : BaseStmt(pos) { stmts_.push_back(stmt); }
    ~CompUnit() {
        for (auto stmt : stmts_) {
            delete stmt;
        }
    }

    Type typeCheck(Table *table) override;
    void translateStmt(SymbolTable *table, int indent) override;
    void print(int indent = 0, bool last = false) override;
    void append(BaseStmt *stmt) { stmts_.push_back(stmt); }

   private:
    std::vector<BaseStmt *> stmts_;
};

class TypeDecl : public BaseStmt {
   public:
    TypeDecl(YYLTYPE pos, Type type) : BaseStmt(pos), type_(type) {}

    Type typeCheck(Table *table) override { return type_; }
    void print(int indent = 0, bool last = false) override { printf("%s", type_.toString().c_str()); }
    Type getType() { return type_; }

   private:
    Type type_;
};

class InitVal : public Exp {
   public:
    InitVal(YYLTYPE pos, Exp *val, bool is_list_) : Exp(pos), val_(val), is_list_(is_list_) {}
    ~InitVal() { delete val_; }

    Type typeCheck(Table *table) override {
        return val_ ? val_->typeCheck(table) : Type(TypeKind::UNKNOWN, {SimpleKind::VOID});
    }
    void print(int indent = 0, bool last = false) override;
    bool isList() { return is_list_; }
    Exp *getVal() { return val_; }

   private:
    Exp *val_;
    bool is_list_;
};

class InitValList : public Exp {
   public:
    InitValList(YYLTYPE pos) : Exp(pos) {}
    ~InitValList() {
        for (auto init_val : init_vals_) {
            delete init_val;
        }
    }

    void print(int indent = 0, bool last = false) override {
        for (auto init_val : init_vals_) {
            init_val->print(indent, init_val == init_vals_.back());
        }
    }
    void append(InitVal *init_val) { init_vals_.push_back(init_val); }
    void appendHead(InitVal *init_val) { init_vals_.insert(init_vals_.begin(), init_val); }
    std::vector<InitVal *> &getInitVals() { return init_vals_; }

   private:
    std::vector<InitVal *> init_vals_;
};

class ArrayDef : public BaseStmt {
   public:
    ArrayDef(YYLTYPE pos) : BaseStmt(pos) {}
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
    void append(YYLTYPE pos, int dim) { dims_.push_back(new IntConst(pos, dim)); }
    std::vector<IntConst *> getDims() { return dims_; }

   private:
    std::vector<IntConst *> dims_;
};

class VarDef : public BaseStmt {
   public:
    VarDef(YYLTYPE pos, const char *name, ArrayDef *array_def, InitVal *init)
        : BaseStmt(pos), name_(name), array_def_(array_def), init_(init) {}
    ~VarDef() {
        delete array_def_;
        delete init_;
    }

    Type typeCheck(Table *table) override;
    void translateStmt(SymbolTable *table, int indent) override;
    void print(int indent = 0, bool last = false) override;
    const char *getName() { return name_; }
    ArrayDef *getArrayDef() { return array_def_; }

   private:
    const char *name_;
    ArrayDef *array_def_;
    InitVal *init_;
};

class VarDefList : public BaseStmt {
   public:
    VarDefList(YYLTYPE pos) : BaseStmt(pos) {}
    ~VarDefList() {
        for (auto def : defs_) {
            delete def;
        }
    }

    void print(int indent = 0, bool last = false) override {
        for (auto def : defs_) {
            def->print(indent, def == defs_.back());
        }
    }
    void append(VarDef *def) { defs_.push_back(def); }
    std::vector<VarDef *> &getDefs() { return defs_; }

   private:
    std::vector<VarDef *> defs_;
};

class VarDecl : public BaseStmt {
   public:
    VarDecl(YYLTYPE pos, TypeDecl *type, VarDefList *def_list) : BaseStmt(pos), type_(type), def_list_(def_list) {}
    ~VarDecl() {
        delete type_;
        delete def_list_;
    }

    Type typeCheck(Table *table) override;
    void translateStmt(SymbolTable *table, int indent) override;
    void print(int indent = 0, bool last = false) override;

   private:
    TypeDecl *type_;
    VarDefList *def_list_;
};

class FuncFArrParam : public BaseStmt {
   public:
    FuncFArrParam(YYLTYPE pos) : BaseStmt(pos) { dims_.push_back(nullptr); }
    ~FuncFArrParam() {
        for (auto dim : dims_) {
            delete dim;
        }
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;
    void append(YYLTYPE pos, int dim) { dims_.push_back(new IntConst(pos, dim)); }
    std::vector<IntConst *> &getDims() { return dims_; }

   private:
    std::vector<IntConst *> dims_;
};

class FuncFParam : public BaseStmt {
   public:
    FuncFParam(YYLTYPE pos, TypeDecl *ftype, const char *name, FuncFArrParam *arr_param_)
        : BaseStmt(pos), ftype_(ftype), name_(name), arr_param_(arr_param_) {}
    ~FuncFParam() {
        delete ftype_;
        delete arr_param_;
    }

    Type typeCheck(Table *table) override;
    void print(int indent = 0, bool last = false) override;
    const char *getName() { return name_; }
    FuncFArrParam *getArrParam() { return arr_param_; }

   private:
    TypeDecl *ftype_;
    const char *name_;
    FuncFArrParam *arr_param_;
};

class FuncFParams : public BaseStmt {
   public:
    FuncFParams(YYLTYPE pos) : BaseStmt(pos) {}
    ~FuncFParams() {
        for (auto fparam : fparams_) {
            delete fparam;
        }
    }

    void print(int indent = 0, bool last = false) override {
        for (auto fparam : fparams_) {
            fparam->print(indent, last && fparam == fparams_.back());
        }
    }
    void append(FuncFParam *fparam) { fparams_.push_back(fparam); }
    std::vector<FuncFParam *> &getFParams() { return fparams_; }

   private:
    std::vector<FuncFParam *> fparams_;
};

class Block : public BaseStmt {
   public:
    Block(YYLTYPE pos) : BaseStmt(pos) {}
    ~Block() {
        for (auto stmt : stmts_) {
            delete stmt;
        }
    }

    Type typeCheck(Table *table) override;
    Type typeCheckWithoutScope(Table *table);
    void translateStmt(SymbolTable *table, int indent) override;
    void translateStmtWithoutScope(SymbolTable *table, int indent);

    void print(int indent = 0, bool last = false) override;
    void append(BaseStmt *stmt) { stmts_.push_back(stmt); }
    std::vector<BaseStmt *> &getStmts() { return stmts_; }

   private:
    std::vector<BaseStmt *> stmts_;
};

class FuncDef : public BaseStmt {
   public:
    FuncDef(YYLTYPE pos, TypeDecl *ftype, const char *name, FuncFParams *fparams, Block *body)
        : BaseStmt(pos), ftype_(ftype), name_(name), fparams_(fparams), body_(body) {}
    ~FuncDef() {
        delete ftype_;
        delete fparams_;
        delete body_;
    }

    Type typeCheck(Table *table) override;
    void translateStmt(SymbolTable *table, int indent) override;
    void print(int indent = 0, bool last = false) override;

   private:
    TypeDecl *ftype_;
    const char *name_;
    FuncFParams *fparams_;
    Block *body_;
};

class LArrVal : public BaseStmt {
   public:
    LArrVal(YYLTYPE pos) : BaseStmt(pos) {}
    ~LArrVal() {
        for (auto exp : dims_) {
            delete exp;
        }
    }

    void print(int indent = 0, bool last = false) override {
        for (auto exp : dims_) {
            exp->print(indent, exp == dims_.back());
        }
    }
    void append(Exp *dim) { dims_.push_back(dim); }
    std::vector<Exp *> &getDims() { return dims_; }

   private:
    std::vector<Exp *> dims_;
};

class LVal : public Exp {
   public:
    LVal(YYLTYPE pos, const char *name) : Exp(pos), name_(name), arr_(nullptr) {}
    LVal(YYLTYPE pos, const char *name, LArrVal *arr) : Exp(pos), name_(name), arr_(arr) {}
    ~LVal() { delete arr_; }

    Type typeCheck(Table *table) override;
    void translateExp(SymbolTable *table, std::string &place, int indent, bool ignoreReturn) override;
    void print(int indent = 0, bool last = false) override;

   private:
    const char *name_;
    LArrVal *arr_;
};

class AssignStmt : public BaseStmt {
   public:
    AssignStmt(YYLTYPE pos, LVal *lhs, Exp *rhs) : BaseStmt(pos), lhs_(lhs), rhs_(rhs) {}
    ~AssignStmt() {
        delete lhs_;
        delete rhs_;
    }

    Type typeCheck(Table *table) override;
    void translateStmt(SymbolTable *table, int indent) override;
    void print(int indent = 0, bool last = false) override;

   private:
    LVal *lhs_;
    Exp *rhs_;
};

class EmptyStmt : public BaseStmt {
   public:
    EmptyStmt(YYLTYPE pos) : BaseStmt(pos) {}

    Type typeCheck(Table *table) override { return Type(TypeKind::SIMPLE, {SimpleKind::VOID}); }
    void translateStmt(SymbolTable *table, int indent) override {}
    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("EmptyStmt\n");
    }
};

class ExpStmt : public BaseStmt {
   public:
    ExpStmt(YYLTYPE pos, Exp *expr) : BaseStmt(pos), expr_(expr) {}
    ~ExpStmt() { delete expr_; }

    Type typeCheck(Table *table) override {
        expr_->typeCheck(table);
        return Type(TypeKind::SIMPLE, {SimpleKind::VOID});
    }
    void translateStmt(SymbolTable *table, int indent) override {
        std::string place = "";
        expr_->translateExp(table, place, indent, true);
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
    IfStmt(YYLTYPE pos, Exp *cond, BaseStmt *then) : BaseStmt(pos), cond_(cond), then_(then), els_(nullptr) {}
    IfStmt(YYLTYPE pos, Exp *cond, BaseStmt *then, BaseStmt *els)
        : BaseStmt(pos), cond_(cond), then_(then), els_(els) {}
    ~IfStmt() {
        delete cond_;
        delete then_;
        delete els_;
    }

    Type typeCheck(Table *table) override;
    void translateStmt(SymbolTable *table, int indent) override;
    void print(int indent = 0, bool last = false) override;

   private:
    Exp *cond_;
    BaseStmt *then_;
    BaseStmt *els_;
};

class WhileStmt : public BaseStmt {
   public:
    WhileStmt(YYLTYPE pos, Exp *cond, BaseStmt *body) : BaseStmt(pos), cond_(cond), body_(body) {}
    ~WhileStmt() {
        delete cond_;
        delete body_;
    }

    Type typeCheck(Table *table) override;
    void translateStmt(SymbolTable *table, int indent) override;
    void print(int indent = 0, bool last = false) override;

   private:
    Exp *cond_;
    BaseStmt *body_;
};

class ReturnStmt : public BaseStmt {
   public:
    ReturnStmt(YYLTYPE pos) : BaseStmt(pos), ret_(nullptr) {}
    ReturnStmt(YYLTYPE pos, Exp *ret) : BaseStmt(pos), ret_(ret) {}
    ~ReturnStmt() { delete ret_; }

    Type typeCheck(Table *table) override;
    void translateStmt(SymbolTable *table, int indent) override;
    void print(int indent = 0, bool last = false) override;

   private:
    Exp *ret_;
};

class PrimaryExp : public Exp {
   public:
    PrimaryExp(YYLTYPE pos, Exp *exp) : Exp(pos), exp_(exp) {}
    ~PrimaryExp() { delete exp_; }

    Type typeCheck(Table *table) override { return exp_->typeCheck(table); }
    void translateExp(SymbolTable *table, std::string &place, int indent, bool ignoreReturn) override {
        exp_->translateExp(table, place, indent, ignoreReturn);
    }
    void print(int indent = 0, bool last = false) override { exp_->print(indent, last); }

   private:
    Exp *exp_;
};

class FuncRParams : public BaseStmt {
   public:
    FuncRParams(YYLTYPE pos) : BaseStmt(pos) {}
    ~FuncRParams() {
        for (auto param : params_) {
            delete param;
        }
    }

    void print(int indent = 0, bool last = false) override {
        for (auto param : params_) {
            param->print(indent, param == params_.back());
        }
    }
    void append(Exp *param) { params_.push_back(param); }
    void appendHead(Exp *param) { params_.insert(params_.begin(), param); }
    std::vector<Exp *> &getParams() { return params_; }

   private:
    std::vector<Exp *> params_;
};

class CallExp : public Exp {
   public:
    CallExp(YYLTYPE pos, const char *name) : Exp(pos), name_(name), params_(nullptr) {}
    CallExp(YYLTYPE pos, const char *name, FuncRParams *params) : Exp(pos), name_(name), params_(params) {}
    ~CallExp() { delete params_; }

    Type typeCheck(Table *table) override;
    void translateExp(SymbolTable *table, std::string &place, int indent, bool ignoreReturn) override;
    void print(int indent = 0, bool last = false) override;

   private:
    const char *name_;
    FuncRParams *params_;
};

class UnaryExp : public Exp {
   public:
    UnaryExp(YYLTYPE pos, const char *&op, Exp *exp) : Exp(pos), op_(op), exp_(exp) {}
    ~UnaryExp() { delete exp_; }

    Type typeCheck(Table *table) override;
    void translateExp(SymbolTable *table, std::string &place, int indent, bool ignoreReturn) override;
    void translateCond(SymbolTable *table, std::string trueLabel, std::string falseLabel, int indent) override;
    void print(int indent = 0, bool last = false) override;

   private:
    const char *op_;
    Exp *exp_;
};

class BinaryExp : public Exp {
   public:
    BinaryExp(YYLTYPE pos, Exp *lhs, Exp *rhs, const char *op) : Exp(pos), lhs_(lhs), rhs_(rhs), op_(op) {}
    ~BinaryExp() {
        delete lhs_;
        delete rhs_;
    }

    Type typeCheck(Table *table) override;
    void translateExp(SymbolTable *table, std::string &place, int indent, bool ignoreReturn) override;
    void print(int indent = 0, bool last = false) override;

   private:
    Exp *lhs_, *rhs_;
    const char *op_;
};

class RelExp : public Exp {
   public:
    RelExp(YYLTYPE pos, Exp *lhs, Exp *rhs, const char *op) : Exp(pos), lhs_(lhs), rhs_(rhs), op_(op) {}
    ~RelExp() {
        delete lhs_;
        delete rhs_;
    }

    Type typeCheck(Table *table) override;
    void translateCond(SymbolTable *table, std::string trueLabel, std::string falseLabel, int indent) override;
    void print(int indent = 0, bool last = false) override;

   private:
    Exp *lhs_, *rhs_;
    const char *op_;
};

class LogicExp : public Exp {
   public:
    LogicExp(YYLTYPE pos, Exp *lhs, Exp *rhs, const char *op) : Exp(pos), lhs_(lhs), rhs_(rhs), op_(op) {}
    ~LogicExp() {
        delete lhs_;
        delete rhs_;
    }

    Type typeCheck(Table *table) override;
    void translateCond(SymbolTable *table, std::string trueLabel, std::string falseLabel, int indent) override;
    void print(int indent = 0, bool last = false) override;

   private:
    Exp *lhs_, *rhs_;
    const char *op_;
};

#endif