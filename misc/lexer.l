%{
    #include "parser.tab.hpp"
    extern int yylex();
    int line_num = 0;
%}

%option noyywrap
%x COMMENT

%%
#.*                         { line_num++; }
\/\*                        { BEGIN(COMMENT); }
<COMMENT>"*/"               { BEGIN(INITIAL); }
<COMMENT>\n                 { line_num++; }
<COMMENT>.                  { }


[ \t]                       { }

halt                        { return HALT; }
int                         { return INT; }
iret                        { return IRET; }
call                        { return CALL; }
ret                         { return RET; }
jmp                         { return JMP; }
beq                         { return BEQ; }
bne                         { return BNE; }
bgt                         { return BGT; }
push                        { return PUSH; }
pop                         { return POP; }
xchg                        { return XCHG; }
add                         { return ADD; }
sub                         { return SUB; }
mul                         { return MUL; }
div                         { return DIV; }
not                         { return NOT; }
and                         { return AND; }
or                          { return OR; }
xor                         { return XOR; }
shl                         { return SHL; }
shr                         { return SHR; }
ld                          { return LD; }
st                          { return ST; }
csrrd                       { return CSRRD; }
csrwr                       { return CSRWR; }
\.word                      { return WORD; }
\.skip                      { return SKIP; }
\.global                    { return GLOBAL; }
\.extern                    { return EXTERN; }
\.section                   { return SECTION; }
\.end                       { return END; yy_accept; }
\,                          { return COMMA; }
\$                          { return DOLLAR; }
\[                          { return LBRACKETS; }
\]                          { return RBRACKETS; }
\+                          { return PLUS; }
\n                          { line_num++; return ENDL; }

%status                     { yylval.ival = 0; return CSR; }
%handler                    { yylval.ival = 1; return CSR; }
%cause                      { yylval.ival = 2; return CSR; }
%r([0-9]|1[0-5])            { 
                              int len = strlen(yytext);
                              char* substring = static_cast<char*>(malloc(len - 2));
                              strcpy(substring, yytext + 2);
                              
                              yylval.ival = atoi(substring);
                              return GPR;
                            }
%(sp|pc)                    {
                              const char* sp = "%sp";
                              yylval.ival = strcmp(yytext, sp) == 0 ? 14 : 15;
                              
                              return GPR;
                            }
[0-9]+                      { yylval.ival = atoi(yytext); return NUMBER; }
0x[0-9a-fA-F]+              { yylval.ival = strtol(yytext, NULL, 16); return NUMBER; }
[a-zA-Z0-9_]+:              {                              
                              yytext[strlen(yytext) - 1] = '\0';
                              yylval.sval = yytext;
                              return LABEL;
                            } 
[a-zA-Z0-9_]+               { yylval.sval = yytext; return STRING; }
%%