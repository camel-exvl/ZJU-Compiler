%option noinput
%option nounput
%option noyywrap
%option yylineno

%x COMMENT

%{
#include "sysy.tab.hh"
%}

digit [0-9]
blank [ \t\n]

%%

"//".*          { }
"/*"                    { BEGIN COMMENT; }
<COMMENT>"*/"           { BEGIN INITIAL; }
<COMMENT>.|\n           { /* skip everything */ }
<COMMENT><<EOF>>        { printf("Unterminated comment at line %d\n", yylineno); exit(1); }
{blank}         { }

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

.               { printf("Unrecognized character at line %d: %s\n", yylineno, yytext); exit(1);}

%%
