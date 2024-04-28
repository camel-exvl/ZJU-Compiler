#include "ast.h"

extern int yylineno;
extern int yyparse();
extern void yyrestart(FILE*);

BaseStmt* root = nullptr;
bool errorFlag = false;
char* inputFilename;
FILE *inputFile, *outputFile;
Table* globalTable = new Table();
SymbolTable* symbolTable = new SymbolTable();

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <input file> [<output file>]\n", argv[0]);
        return 1;
    }

    inputFilename = argv[1];
    inputFile = fopen(argv[1], "r");
    if (!inputFile) {
        perror(argv[1]);
        return -1;
    }
    // extern int yydebug;
    // yydebug = 1;
    yylineno = 1;
    yyrestart(inputFile);
    yyparse();

    if (root) {
        // root->print();
        root->typeCheck(globalTable);
    }
    if (errorFlag) {
        fclose(inputFile);
        inputFile = nullptr;
        return 1;
    }

    // generate intermediate code
    outputFile = argc == 3 ? fopen(argv[2], "w") : stdout;
    if (!outputFile) {
        perror(argv[2]);
        return -1;
    }
    root->translateStmt(symbolTable, 0);
    fclose(inputFile);
    inputFile = nullptr;
    fclose(outputFile);
    outputFile = nullptr;

    delete root;
    return 0;
}
