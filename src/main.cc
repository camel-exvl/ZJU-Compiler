#include <cstdio>

extern int yylineno;
// extern int yydebug;
extern int yyparse();
extern void yyrestart(FILE*);

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
  // yydebug = 1;
  yylineno = 1;
  yyrestart(file);
  yyparse();
  return 0;
}
