#include "ast.h"

void BaseStmt::printIndent(int indent, bool last) {
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

void CompUnit::print(int indent, bool last) {
    printIndent(indent, last);
    printf("CompUnit\n");
    for (auto stmt : stmts_) {
        stmt->print(indent + 1, stmt == stmts_.back());
    }
}

void InitVal::print(int indent, bool last) {
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

void VarDef::print(int indent, bool last) {
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

void VarDecl::print(int indent, bool last) {
    printIndent(indent, last);
    printf("VarDecl: '\033[1m");
    type_->print();
    printf("\033[0m'\n");
    def_list_->print(indent + 1, true);
}

void FuncFArrParam::print(int indent, bool last) {
    for (auto dim : dims_) {
        if (dim) {
            dim->print(indent + 1, dim == dims_.back());
        } else {
            printIndent(indent + 1, dim == dims_.back());
            printf("[]\n");
        }
    }
}

void FuncFParam::print(int indent, bool last) {
    printIndent(indent, last);
    printf("FuncFParam: ");
    printf(" %s '\033[1m", name_);
    ftype_->print();
    printf("\033[0m'\n");
    if (arr_param_) {
        arr_param_->print(indent + 1, true);
    }
}

void Block::print(int indent, bool last) {
    printIndent(indent, last);
    printf("Block\n");
    for (auto stmt : stmts_) {
        stmt->print(indent + 1, stmt == stmts_.back());
    }
}

void FuncDef::print(int indent, bool last) {
    printIndent(indent, last);
    printf("FuncDef: %s\n", name_);
    printIndent(indent + 1, !(fparams_ || body_));
    printf("Return type: '\033[1m");
    ftype_->print();
    printf("\033[0m'\n");
    if (fparams_) {
        fparams_->print(indent + 1, !body_);
    }
    if (body_) {
        body_->print(indent + 1, true);
    }
}

void LVal::print(int indent, bool last) {
    printIndent(indent, last);
    if (arr_) {
        printf("LVal Array: %s\n", name_);
        arr_->print(indent + 1);
    } else {
        printf("LVal: %s\n", name_);
    }
}

void AssignStmt::print(int indent, bool last) {
    printIndent(indent, last);
    printf("AssignStmt: =\n");
    lhs_->print(indent + 1, false);
    rhs_->print(indent + 1, true);
}

void IfStmt::print(int indent, bool last) {
    printIndent(indent, last);
    printf("IfStmt\n");
    cond_->print(indent + 1, false);
    then_->print(indent + 1, !els_);
    if (els_) {
        els_->print(indent + 1, true);
    }
}

void WhileStmt::print(int indent, bool last) {
    printIndent(indent, last);
    printf("WhileStmt\n");
    cond_->print(indent + 1, false);
    body_->print(indent + 1, true);
}

void ReturnStmt::print(int indent, bool last) {
    printIndent(indent, last);
    printf("ReturnStmt\n");
    if (ret_) {
        ret_->print(indent + 1, true);
    }
}

void CallExp::print(int indent, bool last) {
    printIndent(indent, last);
    printf("Call: %s\n", name_);
    if (params_) {
        params_->print(indent + 1, true);
    }
}

void UnaryExp::print(int indent, bool last) {
    printIndent(indent, last);
    printf("UnaryExp: %s\n", op_);
    exp_->print(indent + 1, true);
}

void BinaryExp::print(int indent, bool last) {
    printIndent(indent, last);
    printf("BinaryExp: %s\n", op_);
    lhs_->print(indent + 1, false);
    rhs_->print(indent + 1, true);
}

void RelExp::print(int indent, bool last) {
    printIndent(indent, last);
    printf("RelExp: %s\n", op_);
    lhs_->print(indent + 1, false);
    rhs_->print(indent + 1, true);
}

void LogicExp::print(int indent, bool last) {
    printIndent(indent, last);
    printf("LogicExp: %s\n", op_);
    lhs_->print(indent + 1, false);
    rhs_->print(indent + 1, true);
}