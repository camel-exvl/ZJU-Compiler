#include "ast.h"

extern int yylineno;
extern int yyparse();
extern void yyrestart(FILE*);

BaseStmt* root = nullptr;
bool errorFlag = false;
char* filename;
Table* globalTable = new Table();

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror(argv[1]);
        return 1;
    }
    filename = argv[1];
    // extern int yydebug;
    // yydebug = 1;
    yylineno = 1;
    yyrestart(file);
    yyparse();
    if (root) {
        // root->print();
        root->typeCheck(globalTable);
        delete root;
    }
    if (errorFlag) {
        return -1;
    }
    return 0;
}
