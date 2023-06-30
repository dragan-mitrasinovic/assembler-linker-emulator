%{
  #include <iostream>
  #include <string>
  #include <vector>
  #include "assembler.hpp"
  using namespace std;
  
  extern int line_num;
  extern int yylex();
  void yyerror(const char* s);

  Assembler assembler;
  vector<string> symbol_list;
  Instruction ins;
  Operand op;
%}

%union {
  char* sval;
  int ival;
}

%token ENDL COMMA DOLLAR LBRACKETS RBRACKETS PLUS
%token GLOBAL EXTERN SECTION WORD SKIP END
%token HALT INT IRET CALL RET JMP BEQ BNE BGT
%token PUSH POP XCHG ADD SUB MUL DIV NOT AND OR
%token XOR SHL SHR LD ST CSRRD CSRWR

%token <sval> LABEL
%token <sval> STRING
%token <ival> NUMBER
%token <ival> GPR
%token <ival> CSR

%%
Program: 
    Program Line
  | Line

Line:
    Instruction ENDL
  | Directive ENDL
  | LABEL { assembler.label($1); } ENDL
  | LABEL { assembler.label($1); } Instruction ENDL
  | LABEL { assembler.label($1); } Directive ENDL
  | ENDL

Directive:
    GLOBAL SymbolList { assembler.global(symbol_list); symbol_list.clear(); }
  | EXTERN SymbolList { assembler.extern_directive(symbol_list); symbol_list.clear(); }
  | SECTION STRING { assembler.section($2); }
  | WORD SymbolOrLiteralList
  | SKIP NUMBER { assembler.skip($2); }
  | END { assembler.end(); }

SymbolList:
    STRING { symbol_list.push_back($1); }
  | SymbolList COMMA STRING { symbol_list.push_back($3); }

SymbolOrLiteralList:
    STRING { assembler.word($1); }
  | NUMBER { assembler.word($1); }
  | SymbolOrLiteralList COMMA STRING { assembler.word($3); }
  | SymbolOrLiteralList COMMA NUMBER { assembler.word($3); }

Instruction:
    HALT { assembler.halt_instruction(); }
  | INT { assembler.int_instruction(); }
  | IRET { assembler.iret_instruction(); }
  | CALL OperandJump { ins.operand = op; assembler.call_instruction(ins); }
  | RET {assembler.ret_instruction(); }
  | JMP OperandJump { ins.operand = op; assembler.jmp_instruction(ins); }
  | BEQ GPR COMMA GPR COMMA OperandJump { ins.operand = op; ins.gpr1 = $2; ins.gpr2 = $4; assembler.beq_instruction(ins); }
  | BNE GPR COMMA GPR COMMA OperandJump { ins.operand = op; ins.gpr1 = $2; ins.gpr2 = $4; assembler.bne_instruction(ins); }
  | BGT GPR COMMA GPR COMMA OperandJump { ins.operand = op; ins.gpr1 = $2; ins.gpr2 = $4; assembler.bgt_instruction(ins); }
  | PUSH GPR { assembler.push_instruction($2); }
  | POP GPR { assembler.pop_instruction($2); }
  | XCHG GPR COMMA GPR { ins.gpr1 = $2; ins.gpr2 = $4; assembler.xchg_instruction(ins); }
  | ADD GPR COMMA GPR { ins.gpr1 = $2; ins.gpr2 = $4; assembler.add_instruction(ins); }
  | SUB GPR COMMA GPR { ins.gpr1 = $2; ins.gpr2 = $4; assembler.sub_instruction(ins); }
  | MUL GPR COMMA GPR { ins.gpr1 = $2; ins.gpr2 = $4; assembler.mul_instruction(ins); }
  | DIV GPR COMMA GPR { ins.gpr1 = $2; ins.gpr2 = $4; assembler.div_instruction(ins); }
  | NOT GPR { ins.gpr1 = $2; assembler.not_instruction(ins); }
  | AND GPR COMMA GPR { ins.gpr1 = $2; ins.gpr2 = $4; assembler.and_instruction(ins); }
  | OR GPR COMMA GPR { ins.gpr1 = $2; ins.gpr2 = $4; assembler.or_instruction(ins); }
  | XOR GPR COMMA GPR { ins.gpr1 = $2; ins.gpr2 = $4; assembler.xor_instruction(ins); }
  | SHL GPR COMMA GPR { ins.gpr1 = $2; ins.gpr2 = $4; assembler.shl_instruction(ins); }
  | SHR GPR COMMA GPR { ins.gpr1 = $2; ins.gpr2 = $4; assembler.shr_instruction(ins); }
  | LD OperandData COMMA GPR { ins.gpr1 = $4; ins.operand = op; assembler.ld_instruction(ins); }
  | ST GPR COMMA OperandData { ins.gpr1 = $2; ins.operand = op; assembler.st_instruction(ins); }
  | CSRRD CSR COMMA GPR { ins.csr = $2; ins.gpr1 = $4; assembler.csrrd_instruction(ins); }
  | CSRWR GPR COMMA CSR { ins.gpr1 = $2; ins.csr = $4; assembler.csrwr_instruction(ins); };

OperandData:
    DOLLAR NUMBER { op.adressing_mode = IMMED; op.literal = $2; }
  | DOLLAR STRING { op.adressing_mode = SYMBOL; op.symbol = $2; }
  | NUMBER { op.adressing_mode = LIT_DIR; op.literal = $1; }
  | STRING { op.adressing_mode = SYM_DIR; op.symbol = $1; }
  | GPR { op.adressing_mode = REGDIR; op.reg = $1; }
  | LBRACKETS GPR RBRACKETS { op.adressing_mode = REGIND; op.reg = $2; }
  | LBRACKETS GPR PLUS NUMBER RBRACKETS { op.adressing_mode = REG_LIT; op.reg = $2; op.literal = $4; }
  | LBRACKETS GPR PLUS STRING RBRACKETS { op.adressing_mode = REG_SYM; op.reg = $2; op.symbol = $4; }

OperandJump:
    NUMBER { op.adressing_mode = LIT_DIR; op.literal = $1; }
  | STRING { op.adressing_mode = SYM_DIR; op.symbol = $1; }
%%

void yyerror(const char *s) {
  cout << "Parsing error at line: " << line_num << endl;
  exit(-1);
}