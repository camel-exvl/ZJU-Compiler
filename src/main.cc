#include "ast.h"
#include "common.h"
#include <string.h>

extern int yylineno;
extern int yyparse();
extern void yyrestart(FILE *);

BaseStmt *root = nullptr;
bool errorFlag = false;
char *inputFilename;
FILE *inputFile, *outputFile, *immediateFile;
Table *globalTable = new Table();
SymbolTable *symbolTable = new SymbolTable();

int main(int argc, char **argv) {
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

    outputFile = argc == 3 ? fopen(argv[2], "w") : stdout;
    if (!outputFile) {
        perror(argv[2]);
        return -1;
    }
    immediateFile = argc == 3 ? fopen(strcat(argv[2], ".ir"), "w") : stdout;
    if (!immediateFile) {
        perror(argv[2]);
        return -1;
    }

    // generate intermediate code
    IRNode *irRoot = new IRNode(), *irTail = irRoot;
    root->translateStmt(symbolTable, irTail);
    for (IRNode *ir = irRoot->next; ir != nullptr; ir = ir->next) {
        ir->print();
    }

    // generate assembly code
    AssemblyNode *asmRoot = new AssemblyNode(), *asmTail = asmRoot;
    GenerateTable *table = new GenerateTable();
    for (IRNode *ir = irRoot->next; ir != nullptr; ir = ir->next) {
        ir->generate(table, asmTail);
    }
    fprintf(outputFile, "%s", TEXT.c_str());
    for (AssemblyNode *cur = asmRoot->next; cur != nullptr; cur = cur->next) {
        cur->print();
    }
    fprintf(outputFile, "%s", DATA.c_str());

    delete irRoot;
    delete asmRoot;

    fclose(inputFile);
    inputFile = nullptr;
    fclose(immediateFile);
    immediateFile = nullptr;
    fclose(outputFile);
    outputFile = nullptr;

    delete root;
    return 0;
}
