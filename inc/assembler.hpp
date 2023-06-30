#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

struct Operand {
    int adressing_mode;
    int literal;
    string symbol;
    int reg;
};

struct Instruction {
    Operand operand;
    int gpr1;
    int gpr2;
    int csr;
};

enum AdressingMode {
    IMMED,
    SYMBOL,
    LIT_DIR,
    SYM_DIR,
    REGDIR,
    REGIND,
    REG_LIT,
    REG_SYM
};

struct Symbol {
    int value;
    bool is_global;
    bool is_defined;
    string section_name;

    Symbol() {}

    Symbol(bool is_global, bool is_defined) {
        this->is_global = is_global;
        this->is_defined = is_defined;
    }

    Symbol(int value, bool is_global, bool is_defined, string section_name)
        : Symbol(is_global, is_defined) {
        this->value = value;
        this->section_name = section_name;
    }
};

struct Relocation {
    unsigned int offset;
    int addend;
    string symbol;

    Relocation(int offset, int addend, string symbol) {
        this->offset = offset;
        this->addend = addend;
        this->symbol = symbol;
    }
};

class Section {
  public:
    vector<unsigned char> content;
    unordered_map<int, int> literal_pool;
    unordered_map<string, int> symbol_pool;
    vector<Relocation> relocations;
    int length = 0;
    string name;

    Section() {}
    Section(string name) : name(name) {}

    void add_bytes(vector<unsigned char> bytes) {
        for (auto byte : bytes) {
            content.push_back(byte);
        }
    }

    void write_int(int number) {
        for (int i = 0; i < sizeof(int); ++i) {
            unsigned char byte = (number >> (i * 8)) & 0xFF;
            content.push_back(byte);
        }
    }

    void write_int(int number, int location) {
        for (int i = 0; i < sizeof(int); ++i) {
            unsigned char byte = (number >> (i * 8)) & 0xFF;
            content[location] = byte;
            location++;
        }
    }

    void allocate_literals() {
        for (auto &it : literal_pool) {
            it.second = length;
            length += 4;
        }
    }

    void alocate_symbols() {
        for (auto &it : symbol_pool) {
            it.second = length;
            length += 4;
        }
    }

    void write_literals() {
        for (auto &it : literal_pool) {
            write_int(it.first, it.second);
        }
    }

    void register_literal(int literal) {
        if (literal_pool.find(literal) == literal_pool.end()) {
            literal_pool[literal] = -1;
        }
    }

    void register_symbol(string name) {
        if (symbol_pool.find(name) == symbol_pool.end()) {
            symbol_pool[name] = -1;
        }
    }
};

class Assembler {
  private:
    const unsigned char pc = 15;
    const unsigned char sp = 14;

    bool second_pass = false;
    unordered_map<string, Symbol> symbol_table;
    unordered_map<string, Section> sections;

    Section *current_section = nullptr;
    unsigned int lc = 0;

  public:
    void label(string name);
    void global(vector<string> symbols);
    void extern_directive(vector<string> symbols);
    void section(string name);
    void word(string symbol);
    void word(int literal);
    void skip(int bytes_to_skip);
    void end();

    void halt_instruction();
    void int_instruction();
    void iret_instruction();
    void ret_instruction();
    void call_instruction(Instruction instruction);
    void jmp_instruction(Instruction instruction);
    void beq_instruction(Instruction instruction);
    void bne_instruction(Instruction instruction);
    void bgt_instruction(Instruction instruction);
    void push_instruction(int reg);
    void pop_instruction(int reg);
    void xchg_instruction(Instruction instruction);
    void add_instruction(Instruction instruction);
    void sub_instruction(Instruction instruction);
    void mul_instruction(Instruction instruction);
    void div_instruction(Instruction instruction);
    void not_instruction(Instruction instruction);
    void and_instruction(Instruction instruction);
    void xor_instruction(Instruction instruction);
    void or_instruction(Instruction instruction);
    void shl_instruction(Instruction instruction);
    void shr_instruction(Instruction instruction);
    void ld_instruction(Instruction instruction);
    void st_instruction(Instruction instruction);
    void csrrd_instruction(Instruction instruction);
    void csrwr_instruction(Instruction instruction);

    void make_relocation(string name, int location);
    void make_relocations();
    void symbol_used(string symbol);
};

void print_section(Section &section);
void print_symbol_table(unordered_map<string, Symbol> &sym_tab);
bool fits_12_bits(int number);
