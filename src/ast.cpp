#include "ast.h"

bool Type::operator==(const Type& other) const {
    if (kind_ == TypeKind::UNKNOWN || other.kind_ == TypeKind::UNKNOWN) {
        return true;
    }
    if (kind_ != other.kind_) {
        return false;
    }

    if (kind_ == TypeKind::SIMPLE) {
        return val_.simple == other.val_.simple;
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

std::string Type::toString(std::string name) {
    if (kind_ == TypeKind::UNKNOWN) {
        return "unknown";
    } else if (kind_ == TypeKind::SIMPLE) {
        switch (val_.simple) {
            case SimpleKind::INT:
                return "int";
            case SimpleKind::VOID:
                return "void";
            case SimpleKind::SCOPE:
                return "scope";
            default:
                return "error";
        }
    } else if (kind_ == TypeKind::ARRAY) {
        std::string ret = val_.array->type.toString();
        if (val_.array->size[0] == -1) {  // pointer
            if (val_.array->size.size() == 1) {
                ret += " *";
            } else {
                ret += " (*)";
            }
        }

        for (size_t i = val_.array->size[0] == -1; i < val_.array->size.size(); ++i) {
            ret += "[" + std::to_string(val_.array->size[i]) + "]";
        }
        return ret;
    } else if (kind_ == TypeKind::FUNC) {
        std::string ret = val_.func->ret.toString() + " " + name + "(";
        for (size_t i = 0; i < val_.func->params.size(); ++i) {
            ret += val_.func->params[i].toString();
            if (i != val_.func->params.size() - 1) {
                ret += ", ";
            }
        }
        ret += ")";
        return ret;
    } else {
        return "error";
    }
}

void Table::insert(const char* name, Type type, YYLTYPE pos) {
    if (!table_.count(name)) {
        table_[name] = std::list<Type>();
        table_[name].emplace_back(Type(TypeKind::SIMPLE, {SimpleKind::SCOPE}));
    } else if (!table_[name].back().isScope()) {
        std::string s = type.toString(name);
        if (type.getKind() != TypeKind::FUNC) {
            s += " " + std::string(name);
        }

        if (table_[name].back() == type) {
            error_handle(("redefinition of '\033[1m" + s + "\033[0m'").c_str(), pos);
        } else {
            error_handle(("conflicting declaration '\033[1m" + s + "\033[0m'").c_str(), pos);
        }
        return;
    }
    table_[name].emplace_back(type);
}

Type Table::lookup(const char* name, YYLTYPE pos) {
    if (!table_.count(name)) {
        error_handle(("'\033[1m" + std::string(name) + "\033[0m' was not declared in this scope").c_str(), pos);
        return Type(TypeKind::UNKNOWN, {SimpleKind::VOID});
    }
    for (auto it = table_[name].rbegin(); it != table_[name].rend(); ++it) {
        if (!it->isScope()) {
            return *it;
        }
    }
    error_handle(("'\033[1m" + std::string(name) + "\033[0m' was not declared in this scope").c_str(), pos);
    return Type(TypeKind::UNKNOWN, {SimpleKind::VOID});
}

void Table::enterScope() {
    for (auto& [name, list] : table_) {
        list.emplace_back(Type(TypeKind::SIMPLE, {SimpleKind::SCOPE}));
    }
}

void Table::exitScope() {
    std::vector<std::string> toDelete;
    for (auto& [name, list] : table_) {
        while (!list.empty() && !list.back().isScope()) {
            list.pop_back();
        }
        list.pop_back();
        if (list.empty()) {
            toDelete.emplace_back(name);
        }
    }
    for (auto name : toDelete) {
        table_.erase(name);
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
    table->insert("read", read, pos);
    Type write = Type(TypeKind::FUNC, TypeVal{.func = new FuncVal()});
    write.getVal().func->ret = Type(TypeKind::SIMPLE, {SimpleKind::VOID});
    write.getVal().func->params.emplace_back(Type(TypeKind::SIMPLE, {SimpleKind::INT}));
    table->insert("write", write, pos);

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

Type ArrayDef::typeCheck(Table* table) {
    Type type = Type(TypeKind::ARRAY, TypeVal{.array = new ArrayVal()});
    for (auto dim : dims_) {
        type.getVal().array->size.emplace_back(dim->getValue());
    }
    return type;
}

void arrayInitlistTypeCheck(std::vector<int>& size, int l, int r, InitVal* init) {
    if (!init->getVal()) {
        return;
    }
    if (!init->isList()) {
        error_handle("array must be initialized with a brace-enclosed initializer", init->getPos());
        return;
    }

    int maxNum = 1;
    for (int i = l; i <= r; ++i) {
        maxNum *= size[i];
    }
    int finishedNum = 0;
    for (auto val : static_cast<InitValList*>(init->getVal())->getInitVals()) {
        if (val->isList()) {
            if (finishedNum % size[r] != 0) {
                error_handle("array initializer must be aligned", val->getPos());
                return;
            }

            int edge = r;
            while (edge > l && finishedNum % size[edge] == 0) {
                --edge;
            }
            arrayInitlistTypeCheck(size, edge + 1, r, val);

            int mul = 1;
            for (int i = edge + 1; i <= r; ++i) {
                mul *= size[i];
            }
            finishedNum += mul;
        } else {
            ++finishedNum;
        }
        if (finishedNum > maxNum) {
            error_handle("excess elements in array initializer", val->getPos());
            break;
        }
    }
}

Type VarDef::typeCheck(Table* table) {
    if (array_def_) {
        Type type = array_def_->typeCheck(table);
        if (init_) {
            arrayInitlistTypeCheck(type.getVal().array->size, 0, type.getVal().array->size.size() - 1, init_);
        }
        return type;
    } else if (init_) {
        if (init_->isList()) {
            if (!init_->getVal()) {
                error_handle("empty scalar initializer", init_->getPos());
            } else if (static_cast<InitValList*>(init_->getVal())->getInitVals().size() > 1) {
                error_handle(
                    ("scalar object '\033[1m" + std::string(name_) + "\033[0m' requires one element in initializer")
                        .c_str(),
                    pos);
            } else {
                return static_cast<InitValList*>(init_->getVal())->getInitVals()[0]->typeCheck(table);
            }
            return Type(TypeKind::UNKNOWN, {SimpleKind::VOID});
        }
        Type type = init_->typeCheck(table);
        return type;
    } else {
        return Type(TypeKind::UNKNOWN, {SimpleKind::VOID});
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
            if (cur.getKind() == TypeKind::ARRAY && type == cur.getVal().array->type) {
                cur.getVal().array->type = type;
            } else {
                error_handle(("invalid conversion from '\033[1m" + type.toString() + "\033[0m' to '\033[1m" +
                              cur.toString() + "\033[0m'")
                                 .c_str(),
                             pos);
            }
        } else {
            cur = type;  // update unknown type to real type
        }
        table->insert(def->getName(), cur, def->getPos());
    }
    return Type(TypeKind::SIMPLE, {SimpleKind::VOID});
}

void VarDecl::print(int indent, bool last) {
    printIndent(indent, last);
    printf("VarDecl: '\033[1m");
    type_->print();
    printf("\033[0m'\n");
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
                type.getVal().array->size.emplace_back(dim->getValue());
                type.getVal().array->type = cur;
            }
        } else {
            type.getVal().array->size.emplace_back(-1);
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
    if (arr_param_) {
        Type type = ftype_->typeCheck(table);

        Type arr_type = arr_param_->typeCheck(table);
        if (type != arr_type.getVal().array->type) {
            error_handle(("invalid conversion from '\033[1m" + type.toString() + "\033[0m' to '\033[1m" +
                          arr_type.toString() + "\033[0m'")
                             .c_str(),
                         pos);
        }
        arr_type.getVal().array->type = type;
        return arr_type;
    }
    return ftype_->typeCheck(table);
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

Type Block::typeCheck(Table* table) {
    table->enterScope();
    for (auto stmt : stmts_) {
        stmt->typeCheck(table);
    }
    table->exitScope();
    return Type(TypeKind::SIMPLE, {SimpleKind::VOID});
}

Type Block::typeCheckWithoutScope(Table* table) {
    for (auto stmt : stmts_) {
        stmt->typeCheck(table);
    }
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
        for (auto fparam : fparams_->getFParams()) {
            type.getVal().func->params.emplace_back(fparam->typeCheck(table));
        }
    }
    table->insert(name_, type, pos);

    table->enterScope();
    if (fparams_) {
        for (auto fparam : fparams_->getFParams()) {
            table->insert(fparam->getName(), fparam->typeCheck(table), pos);
        }
    }

    table->setReturnType(&type.getVal().func->ret);
    body_->typeCheckWithoutScope(table);
    table->setReturnType(nullptr);

    table->exitScope();
    return type;
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

Type LVal::typeCheck(Table* table) {
    Type type = table->lookup(name_, pos);
    if (arr_) {
        if (type.getKind() != TypeKind::ARRAY) {
            error_handle(("invalid types '\033[1m" + type.toString() + "\033[0m' for array subscript").c_str(), pos);
            return Type(TypeKind::UNKNOWN, {SimpleKind::VOID});
        }

        for (size_t i = 0; i < arr_->getDims().size(); ++i) {
            Type dim = arr_->getDims()[i]->typeCheck(table);
            if (dim.getKind() != TypeKind::SIMPLE || dim.getVal().simple != SimpleKind::INT) {
                error_handle(("invalid types '\033[1m" + dim.toString() + "\033[0m' for array subscript").c_str(),
                             arr_->getDims()[i]->getPos());
            }
        }
        if (arr_->getDims().size() == type.getVal().array->size.size()) {
            return type.getVal().array->type;
        } else {
            Type ret = Type(TypeKind::ARRAY, TypeVal{.array = new ArrayVal()});
            ret.getVal().array->size.emplace_back(-1);
            for (size_t i = arr_->getDims().size() + 1; i < type.getVal().array->size.size(); ++i) {
                ret.getVal().array->size.emplace_back(type.getVal().array->size[i]);
            }
            ret.getVal().array->type = type.getVal().array->type;
            return ret;
        }
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
        if (expr.getKind() == TypeKind::ARRAY) {
            error_handle("invalid array assignment", pos);
        } else if (expr == Type(TypeKind::SIMPLE, {SimpleKind::VOID})) {
            error_handle("void value not ignored as it ought to be", pos);
        } else {
            error_handle(("invalid conversion from '\033[1m" + lval.toString() + "\033[0m' to '\033[1m" +
                          expr.toString() + "\033[0m'")
                             .c_str(),
                         pos);
        }
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
        error_handle(("invalid conversion from '\033[1m" + cond.toString() + "\033[1m' to '\033[1mint\033[0m'").c_str(),
                     pos);
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
        error_handle(("invalid conversion from '\033[1m" + cond.toString() + "\033[0m' to '\033[1mint\033[0m'").c_str(),
                     pos);
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

    if (!table->getReturnType()) {
        error_handle("expected unqualified-id before '\033[1mreturn\033[0m'", pos);
    } else if (*table->getReturnType() != ret) {
        if (*table->getReturnType() == Type(TypeKind::SIMPLE, {SimpleKind::VOID})) {
            error_handle("return-statement with a value, in function returning '\033[1mvoid\033[0m'", ret_->getPos());
        } else if (ret == Type(TypeKind::SIMPLE, {SimpleKind::VOID})) {
            error_handle(("return-statement with no value, in function returning '\033[1m" +
                          table->getReturnType()->toString() + "\033[0m'")
                             .c_str(),
                         pos);
        } else {
            error_handle(("invalid conversion from \033[1m" + ret.toString() + "\033[0m' to '\033[1m" +
                          table->getReturnType()->toString() + "\033[0m'")
                             .c_str(),
                         ret_ ? ret_->getPos() : pos);
        }
    }
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
    Type type = table->lookup(name_, pos);
    if (type.getKind() == TypeKind::UNKNOWN) {  // if not declared
        return Type(TypeKind::UNKNOWN, {SimpleKind::VOID});
    }

    if (params_) {
        if (type.getKind() != TypeKind::FUNC) {
            error_handle(("'\033[1m" + std::string(name_) + "\033[0m' cannot be used as a function").c_str(), pos);
            return Type(TypeKind::UNKNOWN, {SimpleKind::VOID});
        } else if (type.getVal().func->params.size() > params_->getParams().size()) {
            error_handle(("too few arguments to function '\033[1m" + std::string(name_) + "\033[0m'").c_str(), pos);
            return Type(TypeKind::UNKNOWN, {SimpleKind::VOID});
        } else if (type.getVal().func->params.size() < params_->getParams().size()) {
            error_handle(("too many arguments to function '\033[1m" + std::string(name_) + "\033[0m'").c_str(), pos);
            return Type(TypeKind::UNKNOWN, {SimpleKind::VOID});
        } else {
            for (size_t i = 0; i < params_->getParams().size(); ++i) {
                Type param = params_->getParams()[i]->typeCheck(table);
                if (type.getVal().func->params[i] != param) {
                    error_handle(("invalid conversion from '\033[1m" + param.toString() + "\033[0m' to '\033[1m" +
                                  type.getVal().func->params[i].toString() + "\033[0m'")
                                     .c_str(),
                                    params_->getParams()[i]->getPos());
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
        error_handle(("invalid conversion from '\033[1m" + type.toString() + "\033[0m' to '\033[1mint\033[0m'").c_str(),
                     pos);
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
        error_handle(("invalid operands of types '\033[1m" + lhs.toString() + "\033[0m' and '\033[1m" + rhs.toString() +
                      "\033[0m' to binary '\033[1moperator" + op_ + "\033[0m'")
                         .c_str(),
                     pos);
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
        error_handle(
            ("invalid conversion from '\033[1m" + rhs.toString() + "\033[0m' to '\033[1m" + lhs.toString() + "\033[0m'")
                .c_str(),
            pos);
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
        error_handle(("invalid conversion from '\033[1m" + lhs.toString() + "\033[0m' to '\033[1mint\033[0m'").c_str(),
                     pos);
    }
    if (rhs.getKind() != TypeKind::SIMPLE || rhs.getVal().simple != SimpleKind::INT) {
        error_handle(("invalid conversion from '\033[1m" + rhs.toString() + "\033[0m' to '\033[1mint\033[0m").c_str(),
                     pos);
    }
    return Type(TypeKind::SIMPLE, {SimpleKind::INT});
}

void LogicExp::print(int indent, bool last) {
    printIndent(indent, last);
    printf("LogicExp: %s\n", op_);
    lhs_->print(indent + 1, false);
    rhs_->print(indent + 1, true);
}