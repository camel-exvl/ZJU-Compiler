#include "ast.h"

bool Type::operator==(const Type& other) const {
    if (kind_ != other.kind_) {
        return false;
    }

    if (kind_ == TypeKind::SIMPLE) {
        return val_.simple == other.val_.simple || val_.simple == SimpleKind::UNKNOWN ||
               other.val_.simple == SimpleKind::UNKNOWN;
    } else if (kind_ == TypeKind::ARRAY) {
        if (val_.array->type != other.val_.array->type || val_.array->size.size() != other.val_.array->size.size()) {
            return false;
        }
        for (size_t i = 0; i < val_.array->size.size(); ++i) {
            if (val_.array->size[i] != other.val_.array->size[i]) {
                return false;
            }
        }
        return true;
    } else if (kind_ == TypeKind::FUNC) {
        if (val_.func->ret != other.val_.func->ret || val_.func->params.size() != other.val_.func->params.size()) {
            return false;
        }
        for (size_t i = 0; i < val_.func->params.size(); ++i) {
            if (val_.func->params[i] != other.val_.func->params[i]) {
                return false;
            }
        }
        return true;
    }
    throw std::runtime_error("unknown type kind");
}

std::string Type::toString() {
    if (kind_ == TypeKind::SIMPLE) {
        switch (val_.simple) {
            case SimpleKind::INT:
                return "int";
            case SimpleKind::VOID:
                return "void";
            case SimpleKind::SCOPE:
                return "scope";
            case SimpleKind::UNKNOWN:
                return "unknown";
            default:
                return "error";
        }
    } else if (kind_ == TypeKind::ARRAY) {
        return "array";
    } else if (kind_ == TypeKind::FUNC) {
        return "function";
    } else {
        return "error";
    }
}

void Table::insert(const char* name, Type type) {
    if (!table_.count(name)) {
        table_[name] = std::stack<Type>();
    } else if (!table_[name].top().isScope()) {
        if (table_[name].top() == type) {
            yyerror(("redefinition of '" + type.toString() + " " + std::string(name) + "'").c_str());
        } else {
            yyerror(("conflicting declaration '" + type.toString() + " " + std::string(name) + "'").c_str());
        }
        return;
    }
    table_[name].push(Type(TypeKind::SIMPLE, {SimpleKind::SCOPE}));
    table_[name].push(type);
    undo_.push(name);
}

Type Table::lookup(const char* name) {
    if (!table_.count(name)) {
        yyerror(("'" + std::string(name) + "' was not declared in this scope").c_str());
        return Type(TypeKind::SIMPLE, {SimpleKind::UNKNOWN});
    }
    return table_[name].top();
}

void Table::exitScope() {
    if (undo_.empty()) {
        throw std::runtime_error("Table::exitScope: undo_ is empty");
    }
    std::string name = undo_.top();
    undo_.pop();
    while (!name.empty()) {
        // delete if necessary
        Type type = table_[name].top();
        if (type.getKind() == TypeKind::ARRAY) {
            delete type.getVal().array;
        } else if (type.getKind() == TypeKind::FUNC) {
            delete type.getVal().func;
        }

        table_[name].pop();
        table_[name].pop();
        name = undo_.top();
        undo_.pop();
    }
}

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

Type CompUnit::typeCheck(Table* table) {
    table->enterScope();
    // insert read and write function
    Type read = Type(TypeKind::FUNC, TypeVal{.func = new FuncVal()});
    read.getVal().func->ret = Type(TypeKind::SIMPLE, {SimpleKind::INT});
    table->insert("read", read);
    Type write = Type(TypeKind::FUNC, TypeVal{.func = new FuncVal()});
    write.getVal().func->ret = Type(TypeKind::SIMPLE, {SimpleKind::VOID});
    write.getVal().func->params.emplace_back(Type(TypeKind::SIMPLE, {SimpleKind::INT}));
    table->insert("write", write);

    for (auto stmt : stmts_) {
        stmt->typeCheck(table);
    }

    table->exitScope();
    return Type(TypeKind::SIMPLE, {SimpleKind::VOID});
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

Type InitValList::typeCheck(Table* table) {
    Type type = Type(TypeKind::ARRAY, TypeVal{.array = new ArrayVal()});
    for (auto init_val : init_vals_) {
        Type cur = init_val->typeCheck(table);
        if (cur.getKind() == TypeKind::ARRAY) {
            type.getVal().array->size.insert(type.getVal().array->size.end(), cur.getVal().array->size.begin(),
                                             cur.getVal().array->size.end());
            type.getVal().array->type = cur.getVal().array->type;
        } else if (cur.getKind() == TypeKind::SIMPLE) {
            type.getVal().array->size.emplace_back(1);
            type.getVal().array->type = cur;
        }
    }
    return type;
}

Type ArrayDef::typeCheck(Table* table) {
    Type type = Type(TypeKind::ARRAY, TypeVal{.array = new ArrayVal()});
    for (auto dim : dims_) {
        type.getVal().array->size.emplace_back(dim->getValue());
    }
    return type;
}

Type VarDef::typeCheck(Table* table) {
    if (array_def_) {
        Type type = array_def_->typeCheck(table);
        if (init_) {
            Type init_type = init_->typeCheck(table);
            if (type != init_type) {
                yyerror(
                    ("invalid conversion from '" + type.toString() + "' to '" + init_type.toString() + "'").c_str());
            }

            // delete init if necessary
            if (init_type.getKind() == TypeKind::ARRAY) {
                delete init_type.getVal().array;
            }
        }
        return type;
    } else if (init_) {
        Type type = init_->typeCheck(table);
        return type;
    } else {
        return Type(TypeKind::SIMPLE, {SimpleKind::UNKNOWN});
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

Type VarDecl::typeCheck(Table* table) {
    Type type = type_->typeCheck(table);
    for (auto def : def_list_->getDefs()) {
        Type cur = def->typeCheck(table);
        if (type != cur) {
            yyerror(("invalid conversion from '" + type.toString() + "' to '" + cur.toString() + "'").c_str());
        }
        table->insert(def->getName(), type);
    }
    return Type(TypeKind::SIMPLE, {SimpleKind::VOID});
}

void VarDecl::print(int indent, bool last) {
    printIndent(indent, last);
    printf("VarDecl: '");
    type_->print();
    printf("'\n");
    def_list_->print(indent + 1, true);
}

Type FuncFArrParam::typeCheck(Table* table) {
    Type type = Type(TypeKind::ARRAY, TypeVal{.array = new ArrayVal()});
    for (auto dim : dims_) {
        if (dim) {
            Type cur = dim->typeCheck(table);
            if (cur.getKind() == TypeKind::ARRAY) {
                type.getVal().array->size.insert(type.getVal().array->size.end(), cur.getVal().array->size.begin(),
                                                 cur.getVal().array->size.end());
                type.getVal().array->type = cur.getVal().array->type;
            } else if (cur.getKind() == TypeKind::SIMPLE) {
                type.getVal().array->size.emplace_back(1);
                type.getVal().array->type = cur;
            }
        } else {
            type.getVal().array->size.emplace_back(0);
        }
    }
    return type;
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

Type FuncFParam::typeCheck(Table* table) {
    Type type = ftype_->typeCheck(table);
    if (arr_param_) {
        type.setKind(TypeKind::ARRAY);
        Type arr_type = arr_param_->typeCheck(table);
        type.getVal().array->size = arr_type.getVal().array->size;
        if (type != arr_type.getVal().array->type) {
            yyerror(("invalid conversion from '" + type.toString() + "' to '" + arr_type.toString() + "'").c_str());
        }
    }
    table->insert(name_, type);
    return Type(TypeKind::SIMPLE, {SimpleKind::VOID});
}

void FuncFParam::print(int indent, bool last) {
    printIndent(indent, last);
    printf("FuncFParam: ");
    printf(" %s '", name_);
    ftype_->print();
    printf("'\n");
    if (arr_param_) {
        arr_param_->print(indent + 1, true);
    }
}

Type Block::typeCheck(Table* table) {
    table->enterScope();
    for (auto stmt : stmts_) {
        stmt->typeCheck(table);
    }
    table->exitScope();
    return Type(TypeKind::SIMPLE, {SimpleKind::VOID});
}

void Block::print(int indent, bool last) {
    printIndent(indent, last);
    printf("Block\n");
    for (auto stmt : stmts_) {
        stmt->print(indent + 1, stmt == stmts_.back());
    }
}

Type FuncDef::typeCheck(Table* table) {
    Type type = Type(TypeKind::FUNC, TypeVal{.func = new FuncVal()});
    type.getVal().func->ret = ftype_->typeCheck(table);
    if (fparams_) {
        table->enterScope();
        for (auto fparam : fparams_->getFParams()) {
            type.getVal().func->params.emplace_back(fparam->typeCheck(table));
        }
    }
    body_->typeCheck(table);
    if (fparams_) {
        table->exitScope();
    }
    return type;
}

void FuncDef::print(int indent, bool last) {
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

Type LVal::typeCheck(Table* table) {
    Type type = table->lookup(name_);
    if (arr_) {
        if (type.getKind() != TypeKind::ARRAY) {
            yyerror(("invalid types '" + type.toString() + "' for array subscript").c_str());
        } else if (arr_->getDims().size() != type.getVal().array->size.size()) {
            yyerror(("invalid types '" + type.toString() + "' for array subscript").c_str());
        }
        return type.getVal().array->type;
    }
    return type;
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

Type AssignStmt::typeCheck(Table* table) {
    Type lval = lhs_->typeCheck(table);
    Type expr = rhs_->typeCheck(table);
    if (lval != expr) {
        yyerror(("invalid conversion from '" + lval.toString() + "' to '" + expr.toString() + "'").c_str());
    }
    return Type(TypeKind::SIMPLE, {SimpleKind::VOID});
}

void AssignStmt::print(int indent, bool last) {
    printIndent(indent, last);
    printf("AssignStmt: =\n");
    lhs_->print(indent + 1, false);
    rhs_->print(indent + 1, true);
}

Type IfStmt::typeCheck(Table* table) {
    Type cond = cond_->typeCheck(table);
    if (cond.getKind() != TypeKind::SIMPLE || cond.getVal().simple != SimpleKind::INT) {
        yyerror(("invalid conversion from '" + cond.toString() + "' to 'int'").c_str());
    }
    then_->typeCheck(table);
    if (els_) {
        els_->typeCheck(table);
    }
    return Type(TypeKind::SIMPLE, {SimpleKind::VOID});
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

Type WhileStmt::typeCheck(Table* table) {
    Type cond = cond_->typeCheck(table);
    if (cond.getKind() != TypeKind::SIMPLE || cond.getVal().simple != SimpleKind::INT) {
        yyerror(("invalid conversion from '" + cond.toString() + "' to 'int'").c_str());
    }
    body_->typeCheck(table);
    return Type(TypeKind::SIMPLE, {SimpleKind::VOID});
}

void WhileStmt::print(int indent, bool last) {
    printIndent(indent, last);
    printf("WhileStmt\n");
    cond_->print(indent + 1, false);
    body_->print(indent + 1, true);
}

Type ReturnStmt::typeCheck(Table* table) {
    Type ret;
    if (ret_) {
        ret = ret_->typeCheck(table);
    } else {
        ret = Type(TypeKind::SIMPLE, {SimpleKind::VOID});
    }
    // TODO: check return type
    return Type(TypeKind::SIMPLE, {SimpleKind::VOID});
}

void ReturnStmt::print(int indent, bool last) {
    printIndent(indent, last);
    printf("ReturnStmt\n");
    if (ret_) {
        ret_->print(indent + 1, true);
    }
}

Type CallExp::typeCheck(Table* table) {
    Type type = table->lookup(name_);
    if (params_) {
        if (type.getKind() != TypeKind::FUNC) {
            yyerror(("'" + std::string(name_) + "' cannot be used as a function").c_str());
        } else if (type.getVal().func->params.size() != params_->getParams().size()) {
            yyerror(("no matching function for call to '" + std::string(name_) + "'").c_str());
        } else {
            for (size_t i = 0; i < params_->getParams().size(); ++i) {
                Type param = params_->getParams()[i]->typeCheck(table);
                if (type.getVal().func->params[i] != param) {
                    yyerror(("invalid conversion from '" + param.toString() + "' to '" +
                             type.getVal().func->params[i].toString() + "'")
                                .c_str());
                }
            }
        }
    }
    return type.getVal().func->ret;
}

void CallExp::print(int indent, bool last) {
    printIndent(indent, last);
    printf("Call: %s\n", name_);
    if (params_) {
        params_->print(indent + 1, true);
    }
}

Type UnaryExp::typeCheck(Table* table) {
    Type type = exp_->typeCheck(table);
    if (type.getKind() != TypeKind::SIMPLE || type.getVal().simple != SimpleKind::INT) {
        yyerror(("invalid conversion from '" + type.toString() + "' to 'int'").c_str());
    }
    return type;
}

void UnaryExp::print(int indent, bool last) {
    printIndent(indent, last);
    printf("UnaryExp: %s\n", op_);
    exp_->print(indent + 1, true);
}

Type BinaryExp::typeCheck(Table* table) {
    Type lhs = lhs_->typeCheck(table);
    Type rhs = rhs_->typeCheck(table);
    if (lhs != rhs) {
        yyerror(("invalid conversion from '" + rhs.toString() + "' to '" + lhs.toString() + "'").c_str());
    }
    return lhs;
}

void BinaryExp::print(int indent, bool last) {
    printIndent(indent, last);
    printf("BinaryExp: %s\n", op_);
    lhs_->print(indent + 1, false);
    rhs_->print(indent + 1, true);
}

Type RelExp::typeCheck(Table* table) {
    Type lhs = lhs_->typeCheck(table);
    Type rhs = rhs_->typeCheck(table);
    if (lhs != rhs) {
        yyerror(("invalid conversion from '" + rhs.toString() + "' to '" + lhs.toString() + "'").c_str());
    }
    return Type(TypeKind::SIMPLE, {SimpleKind::INT});
}

void RelExp::print(int indent, bool last) {
    printIndent(indent, last);
    printf("RelExp: %s\n", op_);
    lhs_->print(indent + 1, false);
    rhs_->print(indent + 1, true);
}

Type LogicExp::typeCheck(Table* table) {
    Type lhs = lhs_->typeCheck(table);
    Type rhs = rhs_->typeCheck(table);
    if (lhs.getKind() != TypeKind::SIMPLE || lhs.getVal().simple != SimpleKind::INT) {
        yyerror(("invalid conversion from '" + lhs.toString() + "' to 'int'").c_str());
    }
    if (rhs.getKind() != TypeKind::SIMPLE || rhs.getVal().simple != SimpleKind::INT) {
        yyerror(("invalid conversion from '" + rhs.toString() + "' to 'int'").c_str());
    }
    return Type(TypeKind::SIMPLE, {SimpleKind::INT});
}

void LogicExp::print(int indent, bool last) {
    printIndent(indent, last);
    printf("LogicExp: %s\n", op_);
    lhs_->print(indent + 1, false);
    rhs_->print(indent + 1, true);
}