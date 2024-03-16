#ifndef _AST_H_
#define _AST_H_

#include <cstdio>
#include <vector>

static std::vector<bool> lastFlags(8, false);

class BaseStmt {
   public:
    virtual void print(int indent = 0, bool last = false) = 0;
    static void printIndent(int indent = 0, bool last = false) {
        if (indent == 0) {
            return;
        }
        // resize lastFlags if necessary
        if (lastFlags.size() < (size_t)indent) {
            lastFlags.resize(lastFlags.size() << 1, false);
        }

        for (int i = 0; i < indent - 1; ++i) {
            if (lastFlags[i]) {
                printf("    ");
            } else {
                printf("│   ");
            }
        }
        if (last) {
            printf("└─");
            lastFlags[indent - 1] = true;
        } else {
            printf("├─");
            lastFlags[indent - 1] = false;
        }
    }
};

class Exp : public BaseStmt {};

class Ident : public Exp {
   public:
    Ident(const char *name) : name_(name) {}
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

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("CompUnit\n");
        for (auto stmt : stmts_) {
            stmt->print(indent + 1, stmt == stmts_.back());
        }
    }

    void append(BaseStmt *stmt) { stmts_.push_back(stmt); }

   private:
    std::vector<BaseStmt *> stmts_;
};

class Type : public BaseStmt {
   public:
    Type(const char *name) : name_(name) {}
    void print(int indent = 0, bool last = false) override { printf("%s", name_); }

   private:
    const char *name_;
};

class InitVal : public BaseStmt {
   public:
    InitVal(BaseStmt *val, bool is_list_) : val_(val), is_list_(is_list_) {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        if (is_list_) {
            printf("InitValList\n");
        } else {
            printf("InitVal\n");
        }
        if (val_) {
            val_->print(indent + 1, true);
        } else {
            printIndent(indent + 1, true);
            printf("{}\n");
        }
    }

   private:
    BaseStmt *val_;
    bool is_list_;
};

class InitValList : public BaseStmt {
   public:
    InitValList() {}

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

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        if (array_def_) {
            printf("VarDef Array: %s\n", name_);
            array_def_->print(indent + 1, !init_);
        } else {
            printf("VarDef: %s\n", name_);
        }
        if (init_) {
            init_->print(indent + 1, true);
        }
    }

   private:
    const char *name_;
    ArrayDef *array_def_;
    InitVal *init_;
};

class VarDefList : public BaseStmt {
   public:
    VarDefList() {}

    void print(int indent = 0, bool last = false) override {
        for (auto def : defs_) {
            def->print(indent, def == defs_.back());
        }
    }

    void append(VarDef *def) { defs_.push_back(def); }

   private:
    std::vector<VarDef *> defs_;
};

class VarDecl : public BaseStmt {
   public:
    VarDecl(Type *type, VarDefList *def_list) : type_(type), def_list_(def_list) {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("VarDecl: '");
        type_->print();
        printf("'\n");
        def_list_->print(indent + 1, true);
    }

   private:
    Type *type_;
    VarDefList *def_list_;
};

class FuncFArrParam : public BaseStmt {
   public:
    FuncFArrParam() { dims_.push_back(nullptr); }

    void print(int indent = 0, bool last = false) override {
        for (auto dim : dims_) {
            if (dim) {
                dim->print(indent + 1, dim == dims_.back());
            } else {
                printIndent(indent + 1, dim == dims_.back());
                printf("[]\n");
            }
        }
    }

    void append(int dim) { dims_.push_back(new IntConst(dim)); }

   private:
    std::vector<IntConst *> dims_;
};

class FuncFParam : public BaseStmt {
   public:
    FuncFParam(Type *ftype, const char *name, FuncFArrParam *arr_param_)
        : ftype_(ftype), name_(name), arr_param_(arr_param_) {}
    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("FuncFParam: ");
        printf(" %s '", name_);
        ftype_->print();
        printf("'\n");
        if (arr_param_) {
            arr_param_->print(indent + 1, true);
        }
    }

   private:
    Type *ftype_;
    const char *name_;
    FuncFArrParam *arr_param_;
};

class FuncFParams : public BaseStmt {
   public:
    FuncFParams() {}

    void print(int indent = 0, bool last = false) override {
        for (auto fparam : fparams_) {
            fparam->print(indent, last && fparam == fparams_.back());
        }
    }

    void append(FuncFParam *fparam) { fparams_.push_back(fparam); }

   private:
    std::vector<FuncFParam *> fparams_;
};

class Block : public BaseStmt {
   public:
    Block() {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("Block\n");
        for (auto stmt : stmts_) {
            stmt->print(indent + 1, stmt == stmts_.back());
        }
    }

    void append(BaseStmt *stmt) { stmts_.push_back(stmt); }

   private:
    std::vector<BaseStmt *> stmts_;
};

class FuncDef : public BaseStmt {
   public:
    FuncDef(Type *ftype, const char *name, FuncFParams *fparams, Block *body)
        : ftype_(ftype), name_(name), fparams_(fparams), body_(body) {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("FuncDef: %s\n", name_);
        printIndent(indent + 1, !(fparams_ || body_));
        printf("Return type: '");
        ftype_->print();
        printf("'\n");
        if (fparams_) {
            fparams_->print(indent + 1, !body_);
        }
        if (body_) {
            body_->print(indent + 1, true);
        }
    }

   private:
    Type *ftype_;
    const char *name_;
    FuncFParams *fparams_;
    Block *body_;
};

class LArrVal : public BaseStmt {
   public:
    LArrVal() {}

    void print(int indent = 0, bool last = false) override {
        for (auto exp : dims_) {
            exp->print(indent, exp == dims_.back());
        }
    }

    void append(Exp *dim) { dims_.push_back(dim); }

   private:
    std::vector<Exp *> dims_;
};

class LVal : public Exp {
   public:
    LVal(const char *name) : name_(name), arr_(nullptr) {}
    LVal(const char *name, LArrVal *arr) : name_(name), arr_(arr) {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        if (arr_) {
            printf("LVal Array: %s\n", name_);
            arr_->print(indent + 1);
        } else {
            printf("LVal: %s\n", name_);
        }
    }

   private:
    const char *name_;
    LArrVal *arr_;
};

class AssignStmt : public BaseStmt {
   public:
    AssignStmt(LVal *lhs, Exp *rhs) : lhs_(lhs), rhs_(rhs) {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("AssignStmt: =\n");
        lhs_->print(indent + 1, false);
        rhs_->print(indent + 1, true);
    }

   private:
    LVal *lhs_;
    Exp *rhs_;
};

class EmptyStmt : public BaseStmt {
   public:
    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("EmptyStmt\n");
    }
};

class ExpStmt : public BaseStmt {
   public:
    ExpStmt(Exp *expr) : expr_(expr) {}

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

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("IfStmt\n");
        cond_->print(indent + 1, false);
        then_->print(indent + 1, !els_);
        if (els_) {
            els_->print(indent + 1, true);
        }
    }

   private:
    Exp *cond_;
    BaseStmt *then_;
    BaseStmt *els_;
};

class WhileStmt : public BaseStmt {
   public:
    WhileStmt(Exp *cond, BaseStmt *body) : cond_(cond), body_(body) {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("WhileStmt\n");
        cond_->print(indent + 1, false);
        body_->print(indent + 1, true);
    }

   private:
    Exp *cond_;
    BaseStmt *body_;
};

class ReturnStmt : public BaseStmt {
   public:
    ReturnStmt() : ret_(nullptr) {}
    ReturnStmt(Exp *ret) : ret_(ret) {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("ReturnStmt\n");
        if (ret_) {
            ret_->print(indent + 1, true);
        }
    }

   private:
    Exp *ret_;
};

class PrimaryExp : public Exp {
   public:
    PrimaryExp(Exp *exp) : exp_(exp) {}

    void print(int indent = 0, bool last = false) override { exp_->print(indent, last); }

   private:
    Exp *exp_;
};

class FuncRParams : public BaseStmt {
   public:
    FuncRParams() {}

    void print(int indent = 0, bool last = false) override {
        for (auto param : params_) {
            param->print(indent, param == params_.back());
        }
    }

    void append(Exp *param) { params_.push_back(param); }

   private:
    std::vector<Exp *> params_;
};

class CallExp : public Exp {
   public:
    CallExp(const char *name) : name_(name), params_(nullptr) {}
    CallExp(const char *name, FuncRParams *params) : name_(name), params_(params) {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("Call: %s\n", name_);
        if (params_) {
            params_->print(indent + 1, true);
        }
    }

   private:
    const char *name_;
    FuncRParams *params_;
};

class UnaryExp : public Exp {
   public:
    UnaryExp(const char *&op, Exp *exp) : op_(op), exp_(exp) {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("UnaryExp: %s\n", op_);
        exp_->print(indent + 1, true);
    }

   private:
    const char *op_;
    Exp *exp_;
};

class BinaryExp : public Exp {
   public:
    BinaryExp(Exp *lhs, Exp *rhs, const char *op) : lhs_(lhs), rhs_(rhs), op_(op) {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("BinaryExp: %s\n", op_);
        lhs_->print(indent + 1, false);
        rhs_->print(indent + 1, true);
    }

   private:
    Exp *lhs_, *rhs_;
    const char *op_;
};

class RelExp : public Exp {
   public:
    RelExp(Exp *lhs, Exp *rhs, const char *op) : lhs_(lhs), rhs_(rhs), op_(op) {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("RelExp: %s\n", op_);
        lhs_->print(indent + 1, false);
        rhs_->print(indent + 1, true);
    }

   private:
    Exp *lhs_, *rhs_;
    const char *op_;
};

class LogicExp : public Exp {
   public:
    LogicExp(Exp *lhs, Exp *rhs, const char *op) : lhs_(lhs), rhs_(rhs), op_(op) {}

    void print(int indent = 0, bool last = false) override {
        printIndent(indent, last);
        printf("LogicExp: %s\n", op_);
        lhs_->print(indent + 1, false);
        rhs_->print(indent + 1, true);
    }

   private:
    Exp *lhs_, *rhs_;
    const char *op_;
};

#endif