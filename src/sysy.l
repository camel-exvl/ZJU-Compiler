%option noinput
%option nounput
%option noyywrap
%option yylineno

%x COMMENT

%{
#include <string>
#include "sysy.tab.hh"
extern bool errorFlag;
extern void yyerror(const char *s);

int yycolumn = 1;
#define YY_USER_ACTION \
yylloc.first_line = yylloc.last_line = yylineno; \
yylloc.first_column = yycolumn; \
yylloc.last_column = yycolumn + yyleng - 1; \
yycolumn += yyleng;

char* lineptr = NULL;
size_t n = 0;
int consumed = 0, available = 0;
#define YY_INPUT(buf, result, max_size) {\
    if(available <= 0) {\
        consumed = 0;\
        available = getline(&lineptr, &n, yyin);\
        if (available < 0) {\
            if (ferror(yyin)) { perror("read error:"); }\
            available = 0;\
        }\
    }\
    result = available < max_size ? available : max_size;\
    strncpy(buf, lineptr + consumed, result);\
    consumed += result;\
    available -= result;\
}
%}

digit [0-9]

%%

"//".*          { }
"/*"                    { BEGIN COMMENT; }
<COMMENT>"*/"           { BEGIN INITIAL; }
<COMMENT>.|\n           { /* skip everything */ }
<COMMENT><<EOF>>        { yyerror("Unterminated comment"); }
[ \t]           { }
\n              { yycolumn = 1; }

","             { return COMMA; }
";"             { return SEMICOLON; }
"="             { return ASSIGN; }
"["             { return LBRACKET; }
"]"             { return RBRACKET; }
"("             { return LPAREN; }
")"             { return RPAREN; }
"{"             { return LBRACE; }
"}"             { return RBRACE; }

"+"             { return PLUS; }
"-"             { return MINUS; }
"*"             { return MUL; }
"/"             { return DIV; }
"%"             { return MOD; }

"&&"            { return AND; }
"||"            { return OR; }
"!"             { return NOT; }

"<"             { return LT; }
"<="            { return LE; }
">"             { return GT; }
">="            { return GE; }
"=="            { return EQ; }
"!="            { return NE; }

"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }
"return"        { return RETURN; }

"int"           { return INT; }
"void"          { return VOID; }

0[0-7]+         { yylval.num = strtol(yytext, NULL, 8); return INTCONST; }
0[xX][0-9a-fA-F]+ { yylval.num = strtol(yytext, NULL, 16); return INTCONST; }
{digit}+        { yylval.num = atoi(yytext); return INTCONST; }

[a-zA-Z_][a-zA-Z0-9_]* { yylval.str = strdup(yytext); return IDENT; }

.               { yyerror(("syntax error, unknown token '" + std::string(yytext) + "'").c_str()); }

%%
