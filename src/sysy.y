%locations
%define parse.error detailed
%define parse.lac full

%{
#include <cstdio>
void yyerror(const char *s);
extern int yylex(void);
#include "ast.h"
extern BaseStmt* root;
extern bool errorFlag;

extern int yylineno, yycolumn, yyleng;
extern char *yytext;
extern char* lineptr;
extern char* filename;

#include "sysy.tab.hh"
void error_handle(const char *s, YYLTYPE pos);
%}

%code requires {
#include "ast.h"
}

%union {
    CompUnit *comp;
    TypeDecl *type;
    VarDecl *varDecl;
    VarDefList *varDefList;
    VarDef *varDef;
    ArrayDef *arrayDef;
    InitVal *initVal;
    InitValList *initValList;
    FuncDef *funcDef;
    FuncFParams *funcFParams;
    FuncFParam *funcFParam;
    FuncFArrParam *funcFArrParam;
    Block *block;
    BaseStmt *baseStmt;
    Exp *exp;
    LVal *lval;
    LArrVal *lArrVal;
    IntConst *intConst;
    FuncRParams *funcRParams;
    int num;
    const char *str;
}

%token INT VOID
%token SEMICOLON COMMA ASSIGN LBRACKET RBRACKET LPAREN RPAREN LBRACE RBRACE
%token IF ELSE WHILE RETURN
%token PLUS MINUS MUL DIV MOD AND OR NOT
%token LT GT LE GE EQ NE
%token <num> INTCONST
%token <str> IDENT

%type <comp> CompUnit
%type <type> BType FuncType
%type <varDecl> Decl VarDecl
%type <varDefList> VarDefList
%type <varDef> VarDef
%type <arrayDef> ArrayDef
%type <initVal> InitValDef InitVal
%type <initValList> InitValList
%type <funcDef> FuncDef
%type <funcFParams> FuncFParams
%type <funcFParam> FuncFParam
%type <funcFArrParam> FuncFArrParam
%type <block> Block BlockItem
%type <baseStmt> Stmt
%type <exp> Exp Cond PrimaryExp UnaryExp LAndExp LOrExp MulExp AddExp RelExp EqExp
%type <lval> LVal
%type <lArrVal> LArrVal
%type <intConst> Number
%type <str> UnaryOp
%type <funcRParams> FuncRParams

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%start CompUnit

%%
CompUnit : Decl { $$ = new CompUnit($1); root = $$; }
            | FuncDef { $$ = new CompUnit($1); root = $$; }
            | CompUnit Decl { $1->append($2); $$ = $1; }
            | CompUnit FuncDef { $1->append($2); $$ = $1; }

Decl : VarDecl { $$ = $1;}
BType : INT { $$ = new TypeDecl(Type(TypeKind::SIMPLE, {SimpleKind::INT})); }

VarDecl : BType VarDefList SEMICOLON { $$ = new VarDecl($1, $2); }
            | FuncType VarDefList SEMICOLON { error_handle("syntax error, variable type is not a valid type", @1); YYERROR; }
VarDefList : VarDef { $$ = new VarDefList(); $$->append($1); }
            | VarDefList COMMA VarDef { $$ = $1; $$->append($3); }
VarDef : IDENT ArrayDef InitValDef { $$ = new VarDef($1, $2, $3); }
ArrayDef : { $$ = nullptr; }
            | ArrayDef LBRACKET INTCONST RBRACKET { if ($1 == nullptr) $$ = new ArrayDef(); else $$ = $1; $$->append($3); }
InitValDef : { $$ = nullptr; }
            | ASSIGN InitVal { $$ = $2; }
InitVal : Exp { $$ = new InitVal($1, false); }
            | LBRACE InitValList RBRACE { $$ = new InitVal($2, true); }
InitValList : { $$ = nullptr; }
            | InitVal { $$ = new InitValList(); $$->append($1); }
            | InitVal COMMA InitValList { $$ = $3; $$->appendHead($1); }

FuncDef : FuncType IDENT LPAREN FuncFParams RPAREN Block { $$ = new FuncDef($1, $2, $4, $6);}
            | BType IDENT LPAREN FuncFParams RPAREN Block { $$ = new FuncDef($1, $2, $4, $6); }
            | FuncType IDENT LPAREN RPAREN Block { $$ = new FuncDef($1, $2, nullptr, $5); }
            | BType IDENT LPAREN RPAREN Block { $$ = new FuncDef($1, $2, nullptr, $5); }
FuncType : VOID { $$ = new TypeDecl(Type(TypeKind::SIMPLE, {SimpleKind::VOID})); }
FuncFParams : FuncFParam { $$ = new FuncFParams(); $$->append($1); }
            | FuncFParams COMMA FuncFParam { $$ = $1; $$->append($3); }
FuncFParam : BType IDENT { $$ = new FuncFParam($1, $2, nullptr); }
            | BType IDENT LBRACKET RBRACKET { $$ = new FuncFParam($1, $2, new FuncFArrParam()); }
            | BType IDENT LBRACKET RBRACKET FuncFArrParam { $$ = new FuncFParam($1, $2, $5); }
            | FuncType { error_handle("syntax error, argument type is not a valid type", @1); YYERROR; }
FuncFArrParam : LBRACKET INTCONST RBRACKET { $$ = new FuncFArrParam(); $$->append($2); }
            | FuncFArrParam LBRACKET INTCONST RBRACKET { $$ = $1; $$->append($3); }

Block : LBRACE BlockItem RBRACE { $$ = $2; }
BlockItem : { $$ = nullptr; }
            | BlockItem Decl { if ($1 == nullptr) $$ = new Block(); else $$ = $1; $$->append($2); }
            | BlockItem Stmt { if ($1 == nullptr) $$ = new Block(); else $$ = $1; $$->append($2); }
Stmt : LVal ASSIGN Exp SEMICOLON { $$ = new AssignStmt($1, $3); }
        | SEMICOLON { $$ = new EmptyStmt(); }
        | Exp SEMICOLON { $$ = new ExpStmt($1); }
        | Block { $$ = $1; }
        | IF LPAREN Cond RPAREN Stmt %prec LOWER_THAN_ELSE { $$ = new IfStmt($3, $5); }
        | IF LPAREN Cond RPAREN Stmt ELSE Stmt { $$ = new IfStmt($3, $5, $7); }
        | WHILE LPAREN Cond RPAREN Stmt { $$ = new WhileStmt($3, $5); }
        | RETURN SEMICOLON { $$ = new ReturnStmt(); }
        | RETURN Exp SEMICOLON { $$ = new ReturnStmt($2); }
        | INTCONST ASSIGN Exp SEMICOLON { error_handle("syntax error, rvalue cannot be assigned to", @1); YYERROR; }
        | error SEMICOLON { $$ = nullptr; }

Exp : AddExp { $$ = $1; }
Cond : LOrExp { $$ = $1; }
LVal : IDENT { $$ = new LVal($1);}
        | IDENT LArrVal { $$ = new LVal($1, $2); }
LArrVal : LBRACKET Exp RBRACKET { $$ = new LArrVal(); $$->append($2); }
        | LArrVal LBRACKET Exp RBRACKET { $$ = $1; $$->append($3); }
PrimaryExp : LPAREN Exp RPAREN { $$ = new PrimaryExp($2); }
        | LVal { $$ = $1;}
        | Number { $$ = $1; }
Number : INTCONST { $$ = new IntConst($1); }
UnaryExp : PrimaryExp { $$ = $1; }
        | IDENT LPAREN RPAREN { $$ = new CallExp($1);}
        | IDENT LPAREN FuncRParams RPAREN { $$ = new CallExp($1, $3);}
        | UnaryOp UnaryExp { $$ = new UnaryExp($1, $2); }
UnaryOp : PLUS { $$ = "+"; }
        | MINUS { $$ = "-"; }
        | NOT { $$ = "!"; }
FuncRParams : Exp { $$ = new FuncRParams(); $$->append($1); }
        | Exp COMMA FuncRParams { $$ = $3; $$->append($1); }

MulExp : UnaryExp { $$ = $1; }
        | MulExp MUL UnaryExp { $$ = new BinaryExp($1, $3, "*"); }
        | MulExp DIV UnaryExp { $$ = new BinaryExp($1, $3, "/");}
        | MulExp MOD UnaryExp { $$ = new BinaryExp($1, $3, "%"); }
AddExp : MulExp { $$ = $1; }
        | AddExp PLUS MulExp { $$ = new BinaryExp($1, $3, "+"); }
        | AddExp MINUS MulExp { $$ = new BinaryExp($1, $3, "-"); }
RelExp : AddExp { $$ = $1; }
        | RelExp LT AddExp { $$ = new RelExp($1, $3, "<"); }
        | RelExp GT AddExp { $$ = new RelExp($1, $3, ">"); }
        | RelExp LE AddExp { $$ = new RelExp($1, $3, "<="); }
        | RelExp GE AddExp { $$ = new RelExp($1, $3, ">="); }
EqExp : RelExp { $$ = $1; }
        | EqExp EQ RelExp { $$ = new RelExp($1, $3, "=="); }
        | EqExp NE RelExp { $$ = new RelExp($1, $3, "!="); }
LAndExp : EqExp { $$ = $1; }
        | LAndExp AND EqExp { $$ = new LogicExp($1, $3, "&&"); }
LOrExp : LAndExp { $$ = $1; }
        | LOrExp OR LAndExp { $$ = new LogicExp($1, $3, "||"); }
%%

void error_handle(const char *s, YYLTYPE pos) {
    yyleng = pos.last_column - pos.first_column + 1; 
    yycolumn = pos.last_column + 1;
    yyerror(s);
}

void yyerror(const char *s) {
    fprintf(stderr, "\x1b[1m%s:%d:%d:\x1b[0m \x1b[1;31merror: \x1b[0m%s\n", filename, yylineno, yycolumn - 1, s);
    int spaceLen = fprintf(stderr, "  %d | ", yylineno);

    // print the line
    fprintf(stderr, "%.*s", yycolumn - yyleng - 1, lineptr);
    fprintf(stderr, "\x1b[1;31m%.*s\x1b[0m", yyleng, lineptr + yycolumn - yyleng - 1);
    fprintf(stderr, "%s", lineptr + yycolumn - 1);

    fprintf(stderr, "%*c| \x1b[1;31m%*c", spaceLen - 2, ' ', yycolumn - yyleng, '^');
    for (int i = 1; i < yyleng; i++) {
        fprintf(stderr, "~");
    }
    fprintf(stderr, "\x1b[0m\n");
    errorFlag = true;
}