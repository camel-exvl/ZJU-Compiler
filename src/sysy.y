%{
#include <stdio.h>
void yyerror(const char *s);
extern int yylex(void);
%}

%union {
    int num;
    char* str;
}

%token INT VOID
%token SEMICOLON COMMA ASSIGN LBRACKET RBRACKET LPAREN RPAREN LBRACE RBRACE
%token IF ELSE WHILE RETURN
%token PLUS MINUS MUL DIV MOD AND OR NOT
%token LT GT LE GE EQ NE
%token <num> INTCONST
%token <str> IDENT 

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%start CompUnit

%%
CompUnit : Decl | FuncDef;
CompUnit : Decl CompUnit | FuncDef CompUnit;

Decl : VarDecl;
BType : INT;

VarDecl : BType VarDefList SEMICOLON;
VarDefList : VarDef | VarDefList COMMA VarDef;
VarDef : IDENT ArrayDef InitValDef;
ArrayDef : | LBRACKET INTCONST RBRACKET ArrayDef;
InitValDef : | ASSIGN InitVal;
InitVal : Exp | LBRACE InitValList RBRACE;
InitValList : | InitVal | InitValList COMMA InitVal;

FuncDef : FuncType IDENT LPAREN FuncFParams RPAREN Block | BType IDENT LPAREN FuncFParams RPAREN Block;
FuncType : VOID;
FuncFParams : | FuncFParam | FuncFParams COMMA FuncFParam;
FuncFParam : BType IDENT | BType IDENT LBRACKET RBRACKET | BType IDENT LBRACKET RBRACKET FuncFArrParam;
FuncFArrParam : LBRACKET INTCONST RBRACKET | LBRACKET INTCONST RBRACKET FuncFArrParam;

Block : LBRACE BlockItem RBRACE;
BlockItem : | Decl BlockItem | Stmt BlockItem;
Stmt : LVal ASSIGN Exp SEMICOLON
    | SEMICOLON
    | Exp SEMICOLON
    | Block
    | IF LPAREN Cond RPAREN Stmt %prec LOWER_THAN_ELSE
    | IF LPAREN Cond RPAREN Stmt ELSE Stmt
    | WHILE LPAREN Cond RPAREN Stmt
    | RETURN SEMICOLON
    | RETURN Exp SEMICOLON;

Exp : AddExp;
Cond : LOrExp;
LVal : IDENT | IDENT LArrVal;
LArrVal : LBRACKET Exp RBRACKET | LArrVal LBRACKET Exp RBRACKET;
PrimaryExp : LPAREN Exp RPAREN | LVal | Number;
Number : INTCONST;
UnaryExp : PrimaryExp | IDENT LPAREN RPAREN | IDENT LPAREN FuncRParams RPAREN | UnaryOp UnaryExp;
UnaryOp : PLUS | MINUS | NOT;
FuncRParams : Exp | Exp COMMA FuncRParams;

MulExp : UnaryExp | MulExp MUL UnaryExp | MulExp DIV UnaryExp | MulExp MOD UnaryExp;
AddExp : MulExp | AddExp PLUS MulExp | AddExp MINUS MulExp;
RelExp : AddExp | RelExp LT AddExp | RelExp GT AddExp | RelExp LE AddExp | RelExp GE AddExp;
EqExp : RelExp | EqExp EQ RelExp | EqExp NE RelExp;
LAndExp : EqExp | LAndExp AND EqExp;
LOrExp : LAndExp | LOrExp OR LAndExp;
%%

void yyerror(const char *s) {
    printf("error: %s\n", s);
    exit(1);
}
