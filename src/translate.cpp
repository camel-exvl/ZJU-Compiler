#include "ast.h"
#include <string.h>
#include <cstdarg>

static void printToFile(FILE* file, int indent, const char* format, ...) {
    // if format begin with "LABEL"
    if (strncmp(format, "LABEL", 5) == 0) {
        for (int i = 0; i < indent; ++i) {
            fprintf(file, "  ");
        }
    } else {
        for (int i = 0; i < indent; ++i) {
            fprintf(file, "    ");
        }
    }

    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
}

std::string SymbolTable::insert(std::string name) {
    std::string newName = name;
    if (!table_.count(name)) {
        table_[name] = std::list<std::string>();
        table_[name].emplace_back("$");
    } else if (table_[name].back() != "$") {
        throw std::runtime_error("redefinition of symbol " + name);
    } else {
        newName += std::to_string(table_[name].size() / 2);
        if (newName[0] == '_') {  // avoid conflict with user-defined variable
            newName = '_' + newName;
        }
    }
    table_[name].emplace_back(newName);
    if (isGlobalLayer()) {
        global_table_.emplace(newName);
    }
    return newName;
}

std::string SymbolTable::lookup(std::string name) {
    if (!table_.count(name)) {
        throw std::runtime_error("symbol " + name + " not found");
    }
    for (auto it = table_[name].rbegin(); it != table_[name].rend(); ++it) {
        if (*it != "$") {
            return *it;
        }
    }
    throw std::runtime_error("symbol " + name + " not found");
}

std::vector<IntConst*> SymbolTable::lookupArray(std::string name) {
    if (!array_table_.count(name)) {
        throw std::runtime_error("array " + name + " not found");
    }
    return array_table_[name];
}

void SymbolTable::enterScope() {
    for (auto& [name, list] : table_) {
        list.emplace_back("$");
    }
    ++layer_;
}

void SymbolTable::exitScope() {
    std::vector<std::string> toDelete;
    for (auto& [name, list] : table_) {
        if (!list.empty() && list.back() != "$") {
            std::string newName = list.back();
            if (array_table_.count(newName)) {
                array_table_.erase(newName);
            }
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
    --layer_;
}
void Exp::translateCond(SymbolTable* table, std::string trueLabel, std::string falseLabel, int indent) {
    std::string place = table->newTemp();
    translateExp(table, place, indent);
    std::string zero = table->newTemp();
    printToFile(outputFile, indent, "%s = #0\n", zero.c_str());
    printToFile(outputFile, indent, "IF %s != %s GOTO %s\n", place.c_str(), zero.c_str(), trueLabel.c_str());
    printToFile(outputFile, indent, "GOTO %s\n", falseLabel.c_str());
}

void Ident::translateExp(SymbolTable* table, std::string& place, int indent) {
    printToFile(outputFile, indent, "%s = %s\n", place.c_str(), table->lookup(name_).c_str());
}

void IntConst::translateExp(SymbolTable* table, std::string& place, int indent) {
    printToFile(outputFile, indent, "%s = #%d\n", place.c_str(), val_);
}

void CompUnit::translateStmt(SymbolTable* table, int indent) {
    table->enterScope();
    // insert read and write function
    table->insert("read");
    table->insert("write");

    for (auto stmt : stmts_) {
        stmt->translateStmt(table, indent);
    }

    table->exitScope();
}

/*
1. 记录待处理的 n 维数组各维度的总长 len_1,len_2,⋯ ,len_n​. 比如 int[2][3][4]
各维度的长度分别为 2, 3 和 4.
2. 依次处理初始化列表内的元素, 元素的形式无非就两种可能: 整数, 或者另一个初始化列表.
3. 遇到整数时, 从当前待处理的维度中的最后一维 (第 n 维) 开始填充数据.
4. 遇到初始化列表时:
    当前已经填充完毕的元素的个数必须是 len_n​ 的整数倍,
否则这个初始化列表没有对齐数组维度的边界, 你可以认为这种情况属于语义错误. 检查当前对齐到了哪一个边界,
然后将当前初始化列表视作这个边界所对应的最长维度的数组的初始化列表, 并递归处理. 比如: 对于 int[2][3][4] 和初始化列表 {1,
2, 3, 4, {5}}, 内层的初始化列表 {5} 对应的数组是 int[4]. 对于 int[2][3][4] 和初始化列表 {1, 2, 3, 4, 1, 2, 3, 4, 1, 2,
3, 4, {5}}, 内层的初始化列表 {5} 对应的数组是 int[3][4]. 对于 int[2][3][4] 和初始化列表 {{5}}, 内层的初始化列表 {5}
之前没出现任何整数元素, 这种情况其对应的数组是 int[3][4].
*/
static void translateArrayInitlist(std::vector<int>& size, int l, int r, InitVal* init, std::string initPlace,
                                   std::string numPlace, SymbolTable* table, int indent) {
    int totalSize = 1;
    for (int i = l; i <= r; ++i) {
        totalSize *= size[i];
    }
    if (!init->getVal()) {
        if (!initPlace.empty()) {
            printToFile(outputFile, indent, "%s = #0\n", numPlace.c_str());
        }
        for (int i = 0; i < totalSize; ++i) {
            if (initPlace.empty()) {
                printToFile(outputFile, indent, ".WORD #0\n");
            } else {
                printToFile(outputFile, indent, "*%s = %s\n", initPlace.c_str(), numPlace.c_str());
                printToFile(outputFile, indent, "%s = %s + #4\n", initPlace.c_str(), initPlace.c_str());
            }
        }
        return;
    }

    int finishedNum = 0;
    for (auto val : static_cast<InitValList*>(init->getVal())->getInitVals()) {
        if (val->isList()) {
            int edge = r;
            while (edge > l && finishedNum % size[edge] == 0) {
                --edge;
            }

            translateArrayInitlist(size, edge + 1, r, val, initPlace, numPlace, table, indent);

            int mul = 1;
            for (int i = edge + 1; i <= r; ++i) {
                mul *= size[i];
            }
            finishedNum += mul;
        } else {
            if (initPlace.empty()) {
                printToFile(outputFile, indent, ".WORD #%d\n", static_cast<IntConst*>(val->getVal())->getValue());
            } else {
                val->getVal()->translateExp(table, numPlace, indent);
                printToFile(outputFile, indent, "*%s = %s\n", initPlace.c_str(), numPlace.c_str());
                printToFile(outputFile, indent, "%s = %s + #4\n", initPlace.c_str(), initPlace.c_str());
            }
            ++finishedNum;
        }
    }

    // fill the rest with 0
    if (!initPlace.empty() && finishedNum < totalSize) {
        printToFile(outputFile, indent, "%s = #%d\n", numPlace.c_str(), 0);
    }
    for (int i = finishedNum; i < totalSize; ++i) {
        if (initPlace.empty()) {
            printToFile(outputFile, indent, ".WORD #0\n");
        } else {
            printToFile(outputFile, indent, "*%s = %s\n", initPlace.c_str(), numPlace.c_str());
            printToFile(outputFile, indent, "%s = %s + #4\n", initPlace.c_str(), initPlace.c_str());
        }
    }
}

void VarDef::translateStmt(SymbolTable* table, int indent) {
    std::string name = table->lookup(name_);
    if (table->isGlobalLayer()) {
        printToFile(outputFile, indent, "GLOBAL %s:\n", name.c_str());
    }

    if (array_def_) {
        std::vector<int> size;
        int totalSize = 1;
        for (auto dim : array_def_->getDims()) {
            size.emplace_back(dim->getValue());
            totalSize *= dim->getValue();
        }

        if (!table->isGlobalLayer()) {
            printToFile(outputFile, indent, "DEC %s #%d\n", name.c_str(), totalSize * 4);
        }
        if (init_) {
            std::string initPlace = "", zeroPlace = "";
            if (!table->isGlobalLayer()) {
                initPlace = table->newTemp();
                zeroPlace = table->newTemp();
                printToFile(outputFile, indent, "%s = %s\n", initPlace.c_str(), name.c_str());
            }
            translateArrayInitlist(size, 0, array_def_->getDims().size() - 1, init_, initPlace, zeroPlace, table,
                                   indent);
        } else {
            if (table->isGlobalLayer()) {
                for (int i = 0; i < totalSize; ++i) {
                    printToFile(outputFile, indent, ".WORD #0\n");
                }
            }
        }
    } else if (init_) {
        if (table->isGlobalLayer()) {
            printToFile(outputFile, indent, ".WORD #%d\n", static_cast<IntConst*>(init_->getVal())->getValue());
        } else {
            std::string place = name;
            init_->getVal()->translateExp(table, place, indent);
        }
    } else {
        if (table->isGlobalLayer()) {
            printToFile(outputFile, indent, ".WORD #0\n");
        } else {
            printToFile(outputFile, indent, "%s = #0\n", name.c_str());
        }
    }
}

void VarDecl::translateStmt(SymbolTable* table, int indent) {
    for (auto def : def_list_->getDefs()) {
        std::string name = table->insert(def->getName());
        if (def->getArrayDef()) {  // if it is an array
            table->insertArray(name, def->getArrayDef()->getDims());
        }

        def->translateStmt(table, indent);
    }
}

void Block::translateStmt(SymbolTable* table, int indent) {
    table->enterScope();
    for (auto stmt : stmts_) {
        stmt->translateStmt(table, indent);
    }
    table->exitScope();
}

void Block::translateStmtWithoutScope(SymbolTable* table, int indent) {
    for (auto stmt : stmts_) {
        stmt->translateStmt(table, indent);
    }
}

void FuncDef::translateStmt(SymbolTable* table, int indent) {
    std::string functionName = table->insert(name_);
    table->enterScope();
    printToFile(outputFile, indent, "FUNCTION %s:\n", functionName.c_str());
    if (fparams_) {
        for (auto fparam : fparams_->getFParams()) {
            std::string name = table->insert(fparam->getName());
            if (fparam->getArrParam()) {  // if it is an array
                table->insertArray(name, fparam->getArrParam()->getDims());
            }
            printToFile(outputFile, indent + 1, "PARAM %s\n", name.c_str());
        }
    }
    body_->translateStmtWithoutScope(table, indent + 1);
    table->exitScope();
}

void LVal::translateExp(SymbolTable* table, std::string& place, int indent, bool replace) {
    std::string name = table->lookup(name_);
    if (arr_) {
        std::vector<IntConst*> size = table->lookupArray(name);
        std::string offset = table->newTemp();
        std::vector<Exp*> dims = arr_->getDims();
        int block = 4;  // size of int
        std::string blockPlace = table->newTemp();
        std::string curOffset = table->newTemp();

        if (table->isGlobal(name)) {
            printToFile(outputFile, indent, "%s = &%s\n", offset.c_str(), name.c_str());
        } else {
            printToFile(outputFile, indent, "%s = %s\n", offset.c_str(), name.c_str());
        }

        // align
        for (auto i = size.size() - 1; i > dims.size() - 1; --i) {
            if (!size[i]) {
                throw std::runtime_error("array " + name + " not fully initialized");
            }
            block *= size[i]->getValue();
        }

        for (int i = dims.size() - 1; i >= 0; --i) {
            std::string dimPlace = table->newTemp();
            dims[i]->translateExp(table, dimPlace, indent);
            printToFile(outputFile, indent, "%s = #%d\n", blockPlace.c_str(), block);

            printToFile(outputFile, indent, "%s = %s * %s\n", curOffset.c_str(), dimPlace.c_str(), blockPlace.c_str());
            printToFile(outputFile, indent, "%s = %s + %s\n", offset.c_str(), offset.c_str(), curOffset.c_str());

            if (size[i]) {  // if !size[i] then it is a parameter
                block *= size[i]->getValue();
            }
        }
        if (size.size() > dims.size()) {  // pointer
            name = offset;
        } else {
            name = "*" + offset;
        }
    } else if (table->isGlobal(name)) {
        std::string globalPlace = table->newTemp();
        printToFile(outputFile, indent, "%s = &%s\n", globalPlace.c_str(), name.c_str());
        if (table->isArray(name)) {
            name = globalPlace;
        } else {
            name = "*" + globalPlace;
        }
    }

    if (replace) {
        place = name;
    } else {
        printToFile(outputFile, indent, "%s = %s\n", place.c_str(), name.c_str());
    }
}

void AssignStmt::translateStmt(SymbolTable* table, int indent) {
    std::string lval = table->newTemp();
    lhs_->translateExp(table, lval, indent, true);
    std::string rval = table->newTemp();
    rhs_->translateExp(table, rval, indent);
    printToFile(outputFile, indent, "%s = %s\n", lval.c_str(), rval.c_str());
}

void IfStmt::translateStmt(SymbolTable* table, int indent) {
    std::string thenLabel = table->newLabel();
    std::string elseLabel = table->newLabel();  // elseLabel is equal to endLabel if no else
    cond_->translateCond(table, thenLabel, elseLabel, indent);
    printToFile(outputFile, indent, "LABEL %s:\n", thenLabel.c_str());
    then_->translateStmt(table, indent);
    if (els_) {
        std::string endLabel = table->newLabel();
        printToFile(outputFile, indent, "GOTO %s\n", endLabel.c_str());
        printToFile(outputFile, indent, "LABEL %s:\n", elseLabel.c_str());
        els_->translateStmt(table, indent);
        printToFile(outputFile, indent, "LABEL %s:\n", endLabel.c_str());
    } else {
        printToFile(outputFile, indent, "LABEL %s:\n", elseLabel.c_str());
    }
}

void WhileStmt::translateStmt(SymbolTable* table, int indent) {
    std::string condLabel = table->newLabel();
    std::string bodyLabel = table->newLabel();
    std::string endLabel = table->newLabel();
    printToFile(outputFile, indent, "LABEL %s:\n", condLabel.c_str());
    cond_->translateCond(table, bodyLabel, endLabel, indent);
    printToFile(outputFile, indent, "LABEL %s:\n", bodyLabel.c_str());
    body_->translateStmt(table, indent);
    printToFile(outputFile, indent, "GOTO %s\n", condLabel.c_str());
    printToFile(outputFile, indent, "LABEL %s:\n", endLabel.c_str());
}

void ReturnStmt::translateStmt(SymbolTable* table, int indent) {
    if (ret_) {
        std::string retPlace = table->newTemp();
        ret_->translateExp(table, retPlace, indent);
        printToFile(outputFile, indent, "RETURN %s\n", retPlace.c_str());
    } else {
        printToFile(outputFile, indent, "RETURN\n");
    }
}

void CallExp::translateExp(SymbolTable* table, std::string& place, int indent) {
    std::string function = table->lookup(name_);
    if (params_) {
        for (auto param : params_->getParams()) {
            std::string paramPlace = table->newTemp();
            param->translateExp(table, paramPlace, indent);
            printToFile(outputFile, indent, "ARG %s\n", paramPlace.c_str());
        }
    }
    if (place.empty()) {
        printToFile(outputFile, indent, "CALL %s\n", function.c_str());
    } else {
        printToFile(outputFile, indent, "%s = CALL %s\n", place.c_str(), function.c_str());
    }
}

void UnaryExp::translateExp(SymbolTable* table, std::string& place, int indent) {
    std::string expPlace = table->newTemp();
    exp_->translateExp(table, expPlace, indent);
    printToFile(outputFile, indent, "%s = %s%s\n", place.c_str(), op_, expPlace.c_str());
}

void UnaryExp::translateCond(SymbolTable* table, std::string trueLabel, std::string falseLabel, int indent) {
    if (op_[0] == '!') {
        return exp_->translateCond(table, falseLabel, trueLabel, indent);
    } else {
        Exp::translateCond(table, trueLabel, falseLabel, indent);
    }
}

void BinaryExp::translateExp(SymbolTable* table, std::string& place, int indent) {
    std::string left = table->newTemp();
    std::string right = table->newTemp();
    lhs_->translateExp(table, left, indent);
    rhs_->translateExp(table, right, indent);
    printToFile(outputFile, indent, "%s = %s %s %s\n", place.c_str(), left.c_str(), op_, right.c_str());
}

void RelExp::translateCond(SymbolTable* table, std::string trueLabel, std::string falseLabel, int indent) {
    std::string left = table->newTemp();
    std::string right = table->newTemp();
    lhs_->translateExp(table, left, indent);
    rhs_->translateExp(table, right, indent);
    printToFile(outputFile, indent, "IF %s %s %s GOTO %s\n", left.c_str(), op_, right.c_str(), trueLabel.c_str());
    printToFile(outputFile, indent, "GOTO %s\n", falseLabel.c_str());
}

void LogicExp::translateCond(SymbolTable* table, std::string trueLabel, std::string falseLabel, int indent) {
    std::string leftLabel = table->newLabel();
    switch (op_[0]) {
        case '&': {
            lhs_->translateCond(table, leftLabel, falseLabel, indent);
            break;
        }
        case '|': {
            lhs_->translateCond(table, trueLabel, leftLabel, indent);
            break;
        }
        default:
            throw std::runtime_error("unknown logic operator");
    }
    printToFile(outputFile, indent, "LABEL %s:\n", leftLabel.c_str());
    rhs_->translateCond(table, trueLabel, falseLabel, indent);
}