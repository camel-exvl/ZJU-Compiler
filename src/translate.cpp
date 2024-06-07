#include "ast.h"
#include <string.h>
#include <cstdarg>

static void linkToTail(IRNode*& tail, IRNode* target) {
    if (tail) {
        tail->next = target;
    } else {
        throw std::runtime_error("tail is nullptr");
    }
    tail = target;
}

static void handlePointer(SymbolTable* table, std::string& name, IRNode*& tail) {
    if (name[0] == '*') {
        std::string temp = table->newTemp();
        linkToTail(tail, new Load(Identifier(temp), Identifier(name.substr(1))));
        name = temp;
    }
}

std::string SymbolTable::insert(std::string name) {
    std::string newName = name;
    // avoid conflict
    if (newName[0] == '_') {
        newName = '_' + newName;
    }
    if (newName.back() == '_' || (newName.back() >= '0' && newName.back() <= '9')) {
        newName += '_';
    }

    if (!table_.count(name)) {
        table_[name] = std::list<std::string>();
        table_[name].emplace_back("$");
    } else if (table_[name].back() != "$") {
        throw std::runtime_error("redefinition of symbol " + name);
    } else {
        newName += std::to_string(table_[name].size() / 2);
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

void Exp::translateCond(SymbolTable* table, std::string trueLabel, std::string falseLabel, IRNode*& tail) {
    std::string place = table->newTemp();
    translateExp(table, place, false, tail);
    std::string zero = table->newTemp();
    linkToTail(tail, new LoadImm(Identifier(zero), Immediate(0)));
    linkToTail(tail, new CondGoto(Identifier(place), Identifier(zero), "!=", trueLabel));
    linkToTail(tail, new Goto(falseLabel));
}

void Ident::translateExp(SymbolTable* table, std::string& place, bool ignoreReturn, IRNode*& tail) {
    if (place.empty()) {
        place = table->newTemp();
    }

    linkToTail(tail, new Assign(Identifier(place), Identifier(table->lookup(name_))));
}

void IntConst::translateExp(SymbolTable* table, std::string& place, bool ignoreReturn, IRNode*& tail) {
    if (place.empty()) {
        place = table->newTemp();
    }

    linkToTail(tail, new LoadImm(Identifier(place), Immediate(val_)));
}

void CompUnit::translateStmt(SymbolTable* table, IRNode*& tail) {
    table->enterScope();
    // insert read and write function
    table->insert("read");
    table->insert("write");

    // translate global variable
    for (auto stmt : stmts_) {
        if (typeid(*stmt) == typeid(VarDecl)) {
            stmt->translateStmt(table, tail);
        }
    }

    for (auto stmt : stmts_) {
        if (typeid(*stmt) != typeid(VarDecl)) {
            stmt->translateStmt(table, tail);
        }
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
                                   std::string numPlace, SymbolTable* table, IRNode*& tail) {
    int totalSize = 1;
    for (int i = l; i <= r; ++i) {
        totalSize *= size[i];
    }
    if (!init->getVal()) {
        if (!initPlace.empty()) {
            linkToTail(tail, new LoadImm(Identifier(numPlace), Immediate(0)));
        }
        for (int i = 0; i < totalSize; ++i) {
            if (initPlace.empty()) {
                linkToTail(tail, new Word(Immediate(0)));
            } else {
                linkToTail(tail, new Store(Identifier(initPlace), Identifier(numPlace)));
                linkToTail(tail,
                           new BinopImm(Identifier(initPlace), Identifier(initPlace), Immediate(SIZE_OF_INT), "+"));
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

            translateArrayInitlist(size, edge + 1, r, val, initPlace, numPlace, table, tail);

            int mul = 1;
            for (int i = edge + 1; i <= r; ++i) {
                mul *= size[i];
            }
            finishedNum += mul;
        } else {
            if (initPlace.empty()) {
                linkToTail(tail, new Word(Immediate(static_cast<IntConst*>(val->getVal())->getValue())));
            } else {
                val->getVal()->translateExp(table, numPlace, false, tail);
                linkToTail(tail, new Store(Identifier(initPlace), Identifier(numPlace)));
                linkToTail(tail,
                           new BinopImm(Identifier(initPlace), Identifier(initPlace), Immediate(SIZE_OF_INT), "+"));
            }
            ++finishedNum;
        }
    }

    // fill the rest with 0
    if (!initPlace.empty() && finishedNum < totalSize) {
        linkToTail(tail, new LoadImm(Identifier(numPlace), Immediate(0)));
    }
    for (int i = finishedNum; i < totalSize; ++i) {
        if (initPlace.empty()) {
            linkToTail(tail, new Word(Immediate(0)));
        } else {
            linkToTail(tail, new Store(Identifier(initPlace), Identifier(numPlace)));
            linkToTail(tail, new BinopImm(Identifier(initPlace), Identifier(initPlace), Immediate(SIZE_OF_INT), "+"));
        }
    }
}

void VarDef::translateStmt(SymbolTable* table, IRNode*& tail) {
    std::string name = table->lookup(name_);
    if (table->isGlobalLayer()) {
        linkToTail(tail, new GlobalVar(Identifier(name)));
    }

    if (array_def_) {
        std::vector<int> size;
        int totalSize = 1;
        for (auto dim : array_def_->getDims()) {
            size.emplace_back(dim->getValue());
            totalSize *= dim->getValue();
        }

        if (!table->isGlobalLayer()) {
            linkToTail(tail, new VarDec(Identifier(name), Immediate(totalSize * SIZE_OF_INT)));
        }
        if (init_) {
            std::string initPlace = "", zeroPlace = "";
            if (!table->isGlobalLayer()) {
                initPlace = table->newTemp();
                zeroPlace = table->newTemp();
                linkToTail(tail, new Assign(Identifier(initPlace), Identifier(name)));
            }
            translateArrayInitlist(size, 0, array_def_->getDims().size() - 1, init_, initPlace, zeroPlace, table, tail);
        } else {
            if (table->isGlobalLayer()) {
                for (int i = 0; i < totalSize; ++i) {
                    linkToTail(tail, new Word(Immediate(0)));
                }
            }
        }
    } else if (init_) {
        if (table->isGlobalLayer()) {
            linkToTail(tail, new Word(static_cast<IntConst*>(init_->getVal())->getValue()));
        } else {
            std::string place = name;
            init_->getVal()->translateExp(table, place, false, tail);
        }
    } else {
        if (table->isGlobalLayer()) {
            linkToTail(tail, new Word(Immediate(0)));
        } else {
            linkToTail(tail, new LoadImm(Identifier(name), Immediate(0)));
        }
    }
}

void VarDecl::translateStmt(SymbolTable* table, IRNode*& tail) {
    for (auto def : def_list_->getDefs()) {
        std::string name = table->insert(def->getName());
        if (def->getArrayDef()) {  // if it is an array
            table->insertArray(name, def->getArrayDef()->getDims());
        }

        def->translateStmt(table, tail);
    }
}

void Block::translateStmt(SymbolTable* table, IRNode*& tail) {
    table->enterScope();
    for (auto stmt : stmts_) {
        stmt->translateStmt(table, tail);
    }
    table->exitScope();
}

void Block::translateStmtWithoutScope(SymbolTable* table, IRNode*& tail) {
    for (auto stmt : stmts_) {
        stmt->translateStmt(table, tail);
    }
}

void FuncDef::translateStmt(SymbolTable* table, IRNode*& tail) {
    std::string functionName = table->insert(name_);
    table->enterScope();
    linkToTail(tail, new FuncDefNode(Identifier(functionName)));
    if (fparams_) {
        for (auto fparam : fparams_->getFParams()) {
            std::string name = table->insert(fparam->getName());
            if (fparam->getArrParam()) {  // if it is an array
                table->insertArray(name, fparam->getArrParam()->getDims());
            }
            linkToTail(tail, new Param(Identifier(name)));
        }
    }

    if (body_) {
        body_->translateStmtWithoutScope(table, tail);
    }

    // if the last statement is not return, add a return statement
    if (!body_ || typeid(*body_->getStmts().back()) != typeid(ReturnStmt)) {
        switch (ftype_->getType().getVal().simple) {
            case SimpleKind::VOID: {
                linkToTail(tail, new Return());
                break;
            }
            case SimpleKind::INT: {
                std::string zero = table->newTemp();
                linkToTail(tail, new LoadImm(Identifier(zero), Immediate(0)));
                linkToTail(tail, new ReturnWithVal(Identifier(zero)));
                break;
            }
            default: {
                throw std::runtime_error("unknown return type");
            }
        }
    }
    table->exitScope();
}

void LVal::translateExp(SymbolTable* table, std::string& place, bool ignoreReturn, IRNode*& tail) {
    std::string name = table->lookup(name_);
    if (arr_) {
        std::vector<IntConst*> size = table->lookupArray(name);
        std::string offset = table->newTemp();
        std::vector<Exp*> dims = arr_->getDims();
        int block = SIZE_OF_INT;  // size of int
        std::string blockPlace = table->newTemp();
        std::string curOffset = table->newTemp();

        if (table->isGlobal(name)) {
            linkToTail(tail, new LoadGlobal(Identifier(offset), Identifier(name)));
        } else {
            if (name[0] == '*') {
                linkToTail(tail, new Load(Identifier(offset), Identifier(name.substr(1))));
            } else {
                linkToTail(tail, new Assign(Identifier(offset), Identifier(name)));
            }
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
            dims[i]->translateExp(table, dimPlace, false, tail);
            linkToTail(tail, new LoadImm(Identifier(blockPlace), Immediate(block)));

            linkToTail(tail, new Binop(Identifier(curOffset), Identifier(dimPlace), Identifier(blockPlace), "*"));
            linkToTail(tail, new Binop(Identifier(offset), Identifier(offset), Identifier(curOffset), "+"));

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
        linkToTail(tail, new LoadGlobal(Identifier(globalPlace), Identifier(name)));
        if (table->isArray(name)) {
            name = globalPlace;
        } else {
            name = "*" + globalPlace;
        }
    }

    if (place.empty()) {
        place = name;
    } else {
        if (name[0] == '*') {
            linkToTail(tail, new Load(Identifier(place), Identifier(name.substr(1))));
        } else {
            linkToTail(tail, new Assign(Identifier(place), Identifier(name)));
        }
    }
}

void AssignStmt::translateStmt(SymbolTable* table, IRNode*& tail) {
    std::string lval = "";  // lval doesn't need to be a new temp
    lhs_->translateExp(table, lval, false, tail);
    if (lval[0] == '*') {
        std::string rval = table->newTemp();
        rhs_->translateExp(table, rval, false, tail);
        linkToTail(tail, new Store(Identifier(lval.substr(1)), Identifier(rval)));
    } else {
        rhs_->translateExp(table, lval, false, tail);
    }
}

void IfStmt::translateStmt(SymbolTable* table, IRNode*& tail) {
    std::string thenLabel = table->newLabel();
    std::string elseLabel = table->newLabel();  // elseLabel is equal to endLabel if no else
    cond_->translateCond(table, thenLabel, elseLabel, tail);
    linkToTail(tail, new Label(thenLabel));
    then_->translateStmt(table, tail);
    if (els_) {
        std::string endLabel = table->newLabel();
        linkToTail(tail, new Goto(endLabel));
        linkToTail(tail, new Label(elseLabel));
        els_->translateStmt(table, tail);
        linkToTail(tail, new Label(endLabel));
    } else {
        linkToTail(tail, new Label(elseLabel));
    }
}

void WhileStmt::translateStmt(SymbolTable* table, IRNode*& tail) {
    std::string condLabel = table->newLabel();
    std::string bodyLabel = table->newLabel();
    std::string endLabel = table->newLabel();
    linkToTail(tail, new Label(condLabel));
    cond_->translateCond(table, bodyLabel, endLabel, tail);
    linkToTail(tail, new Label(bodyLabel));
    body_->translateStmt(table, tail);
    linkToTail(tail, new Goto(condLabel));
    linkToTail(tail, new Label(endLabel));
}

void ReturnStmt::translateStmt(SymbolTable* table, IRNode*& tail) {
    if (ret_) {
        std::string retPlace = table->newTemp();
        ret_->translateExp(table, retPlace, false, tail);
        linkToTail(tail, new ReturnWithVal(Identifier(retPlace)));
    } else {
        linkToTail(tail, new Return());
    }
}

void CallExp::translateExp(SymbolTable* table, std::string& place, bool ignoreReturn, IRNode*& tail) {
    if (place.empty() && !ignoreReturn) {
        place = table->newTemp();
    }

    std::string function = table->lookup(name_);
    if (params_) {
        for (auto param : params_->getParams()) {
            std::string paramPlace = table->newTemp();
            param->translateExp(table, paramPlace, false, tail);
            linkToTail(tail, new Arg(Identifier(paramPlace)));
        }
    }
    if (ignoreReturn) {
        linkToTail(tail, new Call(function));
    } else {
        linkToTail(tail, new CallWithRet(Identifier(place), function));
    }
}

void UnaryExp::translateExp(SymbolTable* table, std::string& place, bool ignoreReturn, IRNode*& tail) {
    if (place.empty()) {
        place = table->newTemp();
    }

    std::string expPlace = table->newTemp();
    exp_->translateExp(table, expPlace, false, tail);
    linkToTail(tail, new Unop(Identifier(place), Identifier(expPlace), op_));
}

void UnaryExp::translateCond(SymbolTable* table, std::string trueLabel, std::string falseLabel, IRNode*& tail) {
    if (op_[0] == '!') {
        return exp_->translateCond(table, falseLabel, trueLabel, tail);
    } else {
        Exp::translateCond(table, trueLabel, falseLabel, tail);
    }
}

void BinaryExp::translateExp(SymbolTable* table, std::string& place, bool ignoreReturn, IRNode*& tail) {
    if (place.empty()) {
        place = table->newTemp();
    }

    std::string left = "";
    std::string right = "";
    lhs_->translateExp(table, left, false, tail);
    rhs_->translateExp(table, right, false, tail);
    handlePointer(table, left, tail);
    handlePointer(table, right, tail);
    linkToTail(tail, new Binop(Identifier(place), Identifier(left), Identifier(right), op_));
}

void RelExp::translateCond(SymbolTable* table, std::string trueLabel, std::string falseLabel, IRNode*& tail) {
    std::string left = "";
    std::string right = "";
    lhs_->translateExp(table, left, false, tail);
    if (left[0] == '*') {
        std::string temp = table->newTemp();
        linkToTail(tail, new Load(Identifier(temp), Identifier(left.substr(1))));
        left = temp;
    }
    rhs_->translateExp(table, right, false, tail);
    if (right[0] == '*') {
        std::string temp = table->newTemp();
        linkToTail(tail, new Load(Identifier(temp), Identifier(right.substr(1))));
        right = temp;
    }
    linkToTail(tail, new CondGoto(Identifier(left), Identifier(right), op_, trueLabel));
    linkToTail(tail, new Goto(falseLabel));
}

void LogicExp::translateCond(SymbolTable* table, std::string trueLabel, std::string falseLabel, IRNode*& tail) {
    std::string leftLabel = table->newLabel();
    switch (op_[0]) {
        case '&': {
            lhs_->translateCond(table, leftLabel, falseLabel, tail);
            break;
        }
        case '|': {
            lhs_->translateCond(table, trueLabel, leftLabel, tail);
            break;
        }
        default:
            throw std::runtime_error("unknown logic operator");
    }
    linkToTail(tail, new Label(leftLabel));
    rhs_->translateCond(table, trueLabel, falseLabel, tail);
}