#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "assembler.hpp"
#include "parser.tab.hpp"

extern int yyparse();
extern FILE *yyin;
ofstream output_file;

using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 4) {
        cout << "Expected 3 arguments, got " << argc - 1 << "!" << endl;
        return -1;
    }

    output_file = ofstream(argv[2]);

    FILE *input_file = fopen(argv[3], "r");
    if (!input_file) {
        cout << "Failed to open file " << argv[3] << endl;
        return -1;
    }

    yyin = input_file;
    yyparse();
    fclose(input_file);

    input_file = fopen(argv[3], "r");
    if (!input_file) {
        cout << "Failed to open file " << argv[3] << endl;
        return -1;
    }
    yyin = input_file;
    yyparse();

    fclose(input_file);
    output_file.close();
    return 0;
}

void Assembler::label(string name) {
    if (second_pass)
        return;

    if (symbol_table.find(name) == symbol_table.end()) {
        symbol_table[name] =
            *new Symbol(lc, false, true, current_section->name);

    } else {
        if (symbol_table[name].is_defined) {
            cout << "Symbol " << name << " defined twice!" << endl;
            exit(-1);
        }
        symbol_table[name].is_defined = true;
        symbol_table[name].section_name = current_section->name;
        symbol_table[name].value = lc;
    }
}

void Assembler::global(vector<string> symbols) {
    if (second_pass) {
        for (string name : symbols) {
            if (!symbol_table[name].is_defined) {
                cout << "Symbol " << name << " not defined!" << endl;
                exit(-1);
            }
        }

    } else {
        for (string name : symbols) {
            if (symbol_table.find(name) == symbol_table.end()) {
                symbol_table[name] = *new Symbol(true, false);
            } else {
                symbol_table[name].is_global = true;
            }
        }
    }
}

void Assembler::extern_directive(vector<string> symbols) {}

void Assembler::section(string name) {
    if (second_pass) {
        if (current_section) {
            current_section->content.resize(current_section->length);
            current_section->write_literals();
            make_relocations();
            print_section(*current_section);
        }
        current_section = &sections[name];

    } else {
        if (current_section) {
            current_section->length = lc;
            current_section->allocate_literals();
            current_section->alocate_symbols();
            sections[current_section->name] = *current_section;
        }
        current_section = new Section(name);
    }
    lc = 0;
}

void Assembler::word(string symbol) {
    if (second_pass) {
        make_relocation(symbol, lc);
        current_section->write_int(0);

    } else {
        if (symbol_table.find(symbol) == symbol_table.end()) {
            symbol_table[symbol] = *new Symbol(false, false);
        }
    }
    lc += 4;
}

void Assembler::word(int literal) {
    if (second_pass) {
        current_section->write_int(literal);
    }
    lc += 4;
}

void Assembler::skip(int bytes_to_skip) {
    if (second_pass) {
        current_section->content.resize(lc + bytes_to_skip);
    }
    lc += bytes_to_skip;
}

void Assembler::end() {
    if (second_pass) {
        if (current_section) {
            current_section->content.resize(current_section->length);
            current_section->write_literals();
            make_relocations();
            print_section(*current_section);
        }
        print_symbol_table(symbol_table);

    } else {
        second_pass = true;
        if (current_section) {
            current_section->length = lc;
            current_section->allocate_literals();
            current_section->alocate_symbols();
            sections[current_section->name] = *current_section;
        }
        current_section = nullptr;
        lc = 0;
    }
}

void Assembler::make_relocation(string name, int location) {
    Symbol sym = symbol_table[name];
    Relocation *rel;
    if (sym.is_global || !sym.is_defined) {
        rel = new Relocation(location, 0, name);
    } else {
        rel = new Relocation(location, sym.value, sym.section_name);
    }
    current_section->relocations.push_back(*rel);
}

void Assembler::make_relocations() {
    for (auto it : current_section->symbol_pool) {
        make_relocation(it.first, it.second);
    }
}

void Assembler::symbol_used(string symbol) {
    if (symbol_table.find(symbol) == symbol_table.end()) {
        symbol_table[symbol] = *new Symbol(false, false);
    }
    current_section->register_symbol(symbol);
}

void Assembler::halt_instruction() {
    if (second_pass) {
        current_section->add_bytes({0x00, 0x00, 0x00, 0x00});
    }
    lc += 4;
}

void Assembler::int_instruction() {
    if (second_pass) {
        current_section->add_bytes({0x10, 0x00, 0x00, 0x00});
    }
    lc += 4;
}

void Assembler::iret_instruction() {
    if (second_pass) {
        // status = 0
        unsigned char sp = 14;

        unsigned char second_byte = sp & 0x0F;
        current_section->add_bytes({0x97, second_byte, 0x00, 0x04});
    }
    lc += 4;
    pop_instruction(15);
}

void Assembler::ret_instruction() { pop_instruction(15); }

void Assembler::call_instruction(Instruction instruction) {
    Operand op = instruction.operand;
    unsigned char second_byte;
    unsigned char third_byte;
    unsigned char forth_byte;

    if (second_pass) {
        switch (op.adressing_mode) {
        case LIT_DIR:
            if (fits_12_bits(op.literal)) {
                third_byte = (op.literal >> 8) & 0x0F;
                forth_byte = op.literal & 0xFF;
                current_section->add_bytes(
                    {0x20, 0x00, third_byte, forth_byte});

            } else {
                int offset_to_literal =
                    current_section->literal_pool[op.literal] - lc - 4;
                second_byte = (pc << 4) & 0xF0;
                third_byte = (offset_to_literal >> 8) & 0x0F;
                forth_byte = offset_to_literal & 0xFF;
                current_section->add_bytes(
                    {0x21, second_byte, third_byte, forth_byte});
            }
            lc += 4;
            break;
        case SYM_DIR:
            int offset_to_symbol =
                current_section->symbol_pool[op.symbol] - lc - 4;
            second_byte = (pc << 4) & 0xF0;
            third_byte = (offset_to_symbol >> 8) & 0x0F;
            forth_byte = offset_to_symbol & 0xFF;
            current_section->add_bytes(
                {0x21, second_byte, third_byte, forth_byte});
            lc += 4;
            break;
        }
    } else {
        switch (op.adressing_mode) {
        case LIT_DIR:
            if (!fits_12_bits(op.literal)) {
                current_section->register_literal(op.literal);
            }
            lc += 4;
            break;
        case SYM_DIR:
            symbol_used(op.symbol);
            lc += 4;
            break;
        }
    }
}

void Assembler::jmp_instruction(Instruction instruction) {
    Operand op = instruction.operand;
    unsigned char second_byte;
    unsigned char third_byte;
    unsigned char forth_byte;

    if (second_pass) {
        switch (op.adressing_mode) {
        case LIT_DIR:
            if (fits_12_bits(op.literal)) {
                third_byte = (op.literal >> 8) & 0x0F;
                forth_byte = op.literal & 0xFF;
                current_section->add_bytes(
                    {0x30, 0x00, third_byte, forth_byte});

            } else {
                int offset_to_literal =
                    current_section->literal_pool[op.literal] - lc - 4;
                second_byte = (pc << 4) & 0xF0;
                third_byte = (offset_to_literal >> 8) & 0x0F;
                forth_byte = offset_to_literal & 0xFF;
                current_section->add_bytes(
                    {0x38, second_byte, third_byte, forth_byte});
            }
            lc += 4;
            break;
        case SYM_DIR:
            int offset_to_symbol =
                current_section->symbol_pool[op.symbol] - lc - 4;
            second_byte = (pc << 4) & 0xF0;
            third_byte = (offset_to_symbol >> 8) & 0x0F;
            forth_byte = offset_to_symbol & 0xFF;
            current_section->add_bytes(
                {0x38, second_byte, third_byte, forth_byte});
            lc += 4;
            break;
        }
    } else {
        switch (op.adressing_mode) {
        case LIT_DIR:
            if (!fits_12_bits(op.literal)) {
                current_section->register_literal(op.literal);
            }
            lc += 4;
            break;
        case SYM_DIR:
            symbol_used(op.symbol);
            lc += 4;
            break;
        }
    }
}

void Assembler::beq_instruction(Instruction instruction) {
    Operand op = instruction.operand;
    unsigned char second_byte;
    unsigned char third_byte;
    unsigned char forth_byte;

    if (second_pass) {
        switch (op.adressing_mode) {
        case LIT_DIR:
            if (fits_12_bits(op.literal)) {
                second_byte = (instruction.gpr1) & 0x0F;
                third_byte =
                    ((instruction.gpr2 << 4) & 0xF0) | (op.literal >> 8) & 0x0F;
                forth_byte = op.literal & 0xFF;
                current_section->add_bytes(
                    {0x31, 0x00, third_byte, forth_byte});

            } else {
                int offset_to_literal =
                    current_section->literal_pool[op.literal] - lc - 4;
                second_byte = ((pc << 4) & 0xF0) | ((instruction.gpr1) & 0x0F);
                third_byte = ((instruction.gpr2 << 4) & 0xF0) |
                             (offset_to_literal >> 8) & 0x0F;
                forth_byte = offset_to_literal & 0xFF;
                current_section->add_bytes(
                    {0x39, second_byte, third_byte, forth_byte});
            }
            lc += 4;
            break;
        case SYM_DIR:
            int offset_to_symbol =
                current_section->symbol_pool[op.symbol] - lc - 4;
            second_byte = ((pc << 4) & 0xF0) | ((instruction.gpr1) & 0x0F);
            third_byte = ((instruction.gpr2 << 4) & 0xF0) |
                         (offset_to_symbol >> 8) & 0x0F;
            forth_byte = offset_to_symbol & 0xFF;
            current_section->add_bytes(
                {0x39, second_byte, third_byte, forth_byte});
            lc += 4;
            break;
        }
    } else {
        switch (op.adressing_mode) {
        case LIT_DIR:
            if (!fits_12_bits(op.literal)) {
                current_section->register_literal(op.literal);
            }
            lc += 4;
            break;
        case SYM_DIR:
            symbol_used(op.symbol);
            lc += 4;
            break;
        }
    }
}

void Assembler::bne_instruction(Instruction instruction) {
    Operand op = instruction.operand;
    unsigned char second_byte;
    unsigned char third_byte;
    unsigned char forth_byte;

    if (second_pass) {
        switch (op.adressing_mode) {
        case LIT_DIR:
            if (fits_12_bits(op.literal)) {
                second_byte = (instruction.gpr1) & 0x0F;
                third_byte =
                    ((instruction.gpr2 << 4) & 0xF0) | (op.literal >> 8) & 0x0F;
                forth_byte = op.literal & 0xFF;
                current_section->add_bytes(
                    {0x32, 0x00, third_byte, forth_byte});

            } else {
                int offset_to_literal =
                    current_section->literal_pool[op.literal] - lc - 4;
                second_byte = ((pc << 4) & 0xF0) | ((instruction.gpr1) & 0x0F);
                third_byte = ((instruction.gpr2 << 4) & 0xF0) |
                             (offset_to_literal >> 8) & 0x0F;
                forth_byte = offset_to_literal & 0xFF;
                current_section->add_bytes(
                    {0x3A, second_byte, third_byte, forth_byte});
            }
            lc += 4;
            break;
        case SYM_DIR:
            int offset_to_symbol =
                current_section->symbol_pool[op.symbol] - lc - 4;
            second_byte = ((pc << 4) & 0xF0) | ((instruction.gpr1) & 0x0F);
            third_byte = ((instruction.gpr2 << 4) & 0xF0) |
                         (offset_to_symbol >> 8) & 0x0F;
            forth_byte = offset_to_symbol & 0xFF;
            current_section->add_bytes(
                {0x3A, second_byte, third_byte, forth_byte});
            lc += 4;
            break;
        }
    } else {
        switch (op.adressing_mode) {
        case LIT_DIR:
            if (!fits_12_bits(op.literal)) {
                current_section->register_literal(op.literal);
            }
            lc += 4;
            break;
        case SYM_DIR:
            symbol_used(op.symbol);
            lc += 4;
            break;
        }
    }
}

void Assembler::bgt_instruction(Instruction instruction) {
    Operand op = instruction.operand;
    unsigned char second_byte;
    unsigned char third_byte;
    unsigned char forth_byte;

    if (second_pass) {
        switch (op.adressing_mode) {
        case LIT_DIR:
            if (fits_12_bits(op.literal)) {
                second_byte = (instruction.gpr1) & 0x0F;
                third_byte =
                    ((instruction.gpr2 << 4) & 0xF0) | (op.literal >> 8) & 0x0F;
                forth_byte = op.literal & 0xFF;
                forth_byte = op.literal & 0xFF;
                current_section->add_bytes(
                    {0x33, 0x00, third_byte, forth_byte});

            } else {
                int offset_to_literal =
                    current_section->literal_pool[op.literal] - lc - 4;
                second_byte = ((pc << 4) & 0xF0) | ((instruction.gpr1) & 0x0F);
                third_byte = ((instruction.gpr2 << 4) & 0xF0) |
                             (offset_to_literal >> 8) & 0x0F;
                forth_byte = offset_to_literal & 0xFF;
                current_section->add_bytes(
                    {0x3B, second_byte, third_byte, forth_byte});
            }
            lc += 4;
            break;
        case SYM_DIR:
            int offset_to_symbol =
                current_section->symbol_pool[op.symbol] - lc - 4;
            second_byte = ((pc << 4) & 0xF0) | ((instruction.gpr1) & 0x0F);
            third_byte = ((instruction.gpr2 << 4) & 0xF0) |
                         (offset_to_symbol >> 8) & 0x0F;
            forth_byte = offset_to_symbol & 0xFF;
            current_section->add_bytes(
                {0x3B, second_byte, third_byte, forth_byte});
            lc += 4;
            break;
        }
    } else {
        switch (op.adressing_mode) {
        case LIT_DIR:
            if (!fits_12_bits(op.literal)) {
                current_section->register_literal(op.literal);
            }
            lc += 4;
            break;
        case SYM_DIR:
            symbol_used(op.symbol);
            lc += 4;
            break;
        }
    }
}

void Assembler::push_instruction(int reg) {
    if (second_pass) {
        unsigned char second_byte = (sp << 4) & 0xF0;
        unsigned char third_byte = ((reg << 4) & 0xF0) | 0x0F;
        current_section->add_bytes({0x81, second_byte, third_byte, 0xFC});
    }
    lc += 4;
}

void Assembler::pop_instruction(int reg) {
    if (second_pass) {
        unsigned char second_byte = ((reg << 4) & 0xF0) | (sp & 0x0F);
        current_section->add_bytes({0x93, second_byte, 0x00, 0x04});
    }
    lc += 4;
}

void Assembler::xchg_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char source = instruction.gpr1;
        unsigned char destination = instruction.gpr2;

        unsigned char second_byte = 0x00 | (source & 0x0F);
        unsigned char third_byte = 0x00 | ((source << 4) & 0xF0);
        current_section->add_bytes({0x40, second_byte, third_byte, 0x00});
    }
    lc += 4;
}

void Assembler::add_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char source = instruction.gpr1;
        unsigned char destination = instruction.gpr2;

        unsigned char second_byte =
            ((destination << 4) & 0xF0) | (destination & 0x0F);
        unsigned char third_byte = 0x00 | ((source << 4) & 0xF0);
        current_section->add_bytes({0x50, second_byte, third_byte, 0x00});
    }
    lc += 4;
}

void Assembler::sub_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char source = instruction.gpr1;
        unsigned char destination = instruction.gpr2;

        unsigned char second_byte =
            ((destination << 4) & 0xF0) | (destination & 0x0F);
        unsigned char third_byte = 0x00 | ((source << 4) & 0xF0);
        current_section->add_bytes({0x51, second_byte, third_byte, 0x00});
    }
    lc += 4;
}

void Assembler::mul_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char source = instruction.gpr1;
        unsigned char destination = instruction.gpr2;

        unsigned char second_byte =
            ((destination << 4) & 0xF0) | (destination & 0x0F);
        unsigned char third_byte = 0x00 | ((source << 4) & 0xF0);
        current_section->add_bytes({0x52, second_byte, third_byte, 0x00});
    }
    lc += 4;
}

void Assembler::div_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char source = instruction.gpr1;
        unsigned char destination = instruction.gpr2;

        unsigned char second_byte =
            ((destination << 4) & 0xF0) | (destination & 0x0F);
        unsigned char third_byte = 0x00 | ((source << 4) & 0xF0);
        current_section->add_bytes({0x53, second_byte, third_byte, 0x00});
    }
    lc += 4;
}

void Assembler::not_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char reg = instruction.gpr1;
        unsigned char second_byte = ((reg << 4) & 0xF0) | (reg & 0x0F);
        current_section->add_bytes({0x50, second_byte, 0x00, 0x00});
    }
    lc += 4;
}

void Assembler::and_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char source = instruction.gpr1;
        unsigned char destination = instruction.gpr2;

        unsigned char second_byte =
            ((destination << 4) & 0xF0) | (destination & 0x0F);
        unsigned char third_byte = 0x00 | ((source << 4) & 0xF0);
        current_section->add_bytes({0x61, second_byte, third_byte, 0x00});
    }
    lc += 4;
}

void Assembler::or_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char source = instruction.gpr1;
        unsigned char destination = instruction.gpr2;

        unsigned char second_byte =
            ((destination << 4) & 0xF0) | (destination & 0x0F);
        unsigned char third_byte = 0x00 | ((source << 4) & 0xF0);
        current_section->add_bytes({0x62, second_byte, third_byte, 0x00});
    }
    lc += 4;
}

void Assembler::xor_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char source = instruction.gpr1;
        unsigned char destination = instruction.gpr2;

        unsigned char second_byte =
            ((destination << 4) & 0xF0) | (destination & 0x0F);
        unsigned char third_byte = 0x00 | ((source << 4) & 0xF0);
        current_section->add_bytes({0x63, second_byte, third_byte, 0x00});
    }
    lc += 4;
}

void Assembler::shl_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char source = instruction.gpr1;
        unsigned char destination = instruction.gpr2;

        unsigned char second_byte =
            ((destination << 4) & 0xF0) | (destination & 0x0F);
        unsigned char third_byte = 0x00 | ((source << 4) & 0xF0);
        current_section->add_bytes({0x70, second_byte, third_byte, 0x00});
    }
    lc += 4;
}

void Assembler::shr_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char source = instruction.gpr1;
        unsigned char destination = instruction.gpr2;

        unsigned char second_byte =
            ((destination << 4) & 0xF0) | (destination & 0x0F);
        unsigned char third_byte = 0x00 | ((source << 4) & 0xF0);
        current_section->add_bytes({0x71, second_byte, third_byte, 0x00});
    }
    lc += 4;
}

void Assembler::ld_instruction(Instruction instruction) {
    if (second_pass) {
        Operand op = instruction.operand;
        unsigned char destination = instruction.gpr1;
        unsigned char second_byte;
        unsigned char third_byte;
        unsigned char forth_byte;

        int offset_to_symbol;
        unsigned char source;

        switch (op.adressing_mode) {
        case IMMED:
            if (fits_12_bits(op.literal)) {
                second_byte = (destination << 4) & 0xF0;
                third_byte = (op.literal >> 8) & 0x0F;
                forth_byte = op.literal & 0xFF;
                current_section->add_bytes(
                    {0x91, second_byte, third_byte, forth_byte});

            } else {
                int offset_to_literal =
                    current_section->literal_pool[op.literal] - lc - 4;
                second_byte = ((destination << 4) & 0xF0) | (pc & 0x0F);
                third_byte = (offset_to_literal >> 8) & 0x0F;
                forth_byte = offset_to_literal & 0xFF;
                current_section->add_bytes(
                    {0x92, second_byte, third_byte, forth_byte});
            }
            lc += 4;
            break;
        case SYMBOL:
            offset_to_symbol = current_section->symbol_pool[op.symbol] - lc - 4;
            second_byte = ((destination << 4) & 0xF0) | (pc & 0x0F);
            third_byte = (offset_to_symbol >> 8) & 0x0F;
            forth_byte = offset_to_symbol & 0xFF;
            current_section->add_bytes(
                {0x92, second_byte, third_byte, forth_byte});
            lc += 4;
            break;
        case LIT_DIR:
            if (fits_12_bits(op.literal)) {
                second_byte = (destination << 4) & 0xF0;
                third_byte = (op.literal >> 8) & 0x0F;
                forth_byte = op.literal & 0xFF;
                current_section->add_bytes(
                    {0x92, second_byte, third_byte, forth_byte});
                lc += 4;
            } else {
                // reg[A] = literal
                int offset_to_literal =
                    current_section->literal_pool[op.literal] - lc - 4;
                second_byte = ((destination << 4) & 0xF0) | (pc & 0x0F);
                third_byte = (offset_to_literal >> 8) & 0x0F;
                forth_byte = offset_to_literal & 0xFF;
                current_section->add_bytes(
                    {0x92, second_byte, third_byte, forth_byte});

                // reg[A] = mem[reg[A]]
                second_byte =
                    ((destination << 4) & 0xF0) | (destination & 0x0F);
                current_section->add_bytes({0x92, second_byte, 0x00, 0x00});
                lc += 8;
            }
            break;
        case SYM_DIR:
            // reg[A] = symbol_value
            offset_to_symbol = current_section->symbol_pool[op.symbol] - lc - 4;
            second_byte = ((destination << 4) & 0xF0) | (pc & 0x0F);
            third_byte = (offset_to_symbol >> 8) & 0x0F;
            forth_byte = offset_to_symbol & 0xFF;
            current_section->add_bytes(
                {0x92, second_byte, third_byte, forth_byte});

            // reg[A] = mem[reg[A]]
            second_byte = ((destination << 4) & 0xF0) | (destination & 0x0F);
            current_section->add_bytes({0x92, second_byte, 0x00, 0x00});
            lc += 8;
            break;
        case REGDIR:
            source = op.reg;
            second_byte = ((destination << 4) & 0xF0) | (source & 0x0F);
            current_section->add_bytes({0x91, second_byte, 0x00, 0x00});
            lc += 4;
            break;
        case REGIND:
            source = op.reg;
            second_byte = ((destination << 4) & 0xF0) | (source & 0x0F);
            current_section->add_bytes({0x92, second_byte, 0x00, 0x00});
            lc += 4;
            break;
        case REG_LIT:
            // if (!fits_12_bits(op.literal)) {
            //     cout << "Literal must fit 12 bits!" << endl;
            //     exit(-1);
            // }

            source = op.reg;
            second_byte = ((destination << 4) & 0xF0) | (source & 0x0F);
            third_byte = (op.literal >> 8) & 0x0F;
            forth_byte = op.literal & 0xFF;
            current_section->add_bytes(
                {0x92, second_byte, third_byte, forth_byte});
            lc += 4;
            break;
        case REG_SYM:
            cout << "Symbol value unknown!" << endl;
            exit(-1);
            break;
        }
    } else {
        Operand op = instruction.operand;
        switch (instruction.operand.adressing_mode) {
        case IMMED:
            if (!fits_12_bits(op.literal)) {
                current_section->register_literal(op.literal);
            }
            lc += 4;
            break;
        case SYMBOL:
            symbol_used(op.symbol);
            lc += 4;
            break;
        case LIT_DIR:
            if (!fits_12_bits(op.literal)) {
                current_section->register_literal(op.literal);
                lc += 4;
            }
            lc += 4;
            break;
        case SYM_DIR:
            symbol_used(op.symbol);
            lc += 8;
            break;
        case REGDIR:
        case REGIND:
        case REG_LIT:
            lc += 4;
            break;
        case REG_SYM:
            cout << "Symbol value unknown!" << endl;
            exit(-1);
            break;
        }
    }
}

void Assembler::st_instruction(Instruction instruction) {
    Operand op = instruction.operand;
    if (second_pass) {
        unsigned char source = instruction.gpr1;
        unsigned char second_byte;
        unsigned char third_byte;
        unsigned char forth_byte;

        int offset_to_symbol;
        unsigned char destination;

        switch (op.adressing_mode) {
        case IMMED:
            cout << "Can't store to literal!" << endl;
            exit(-1);
            break;
        case SYMBOL:
            cout << "Can't store to symbol!" << endl;
            exit(-1);
            break;
        case LIT_DIR:
            if (fits_12_bits(op.literal)) {
                third_byte = ((source << 4) & 0xF0) | (op.literal >> 8) & 0x0F;
                forth_byte = op.literal & 0xFF;
                current_section->add_bytes(
                    {0x80, 0x00, third_byte, forth_byte});

            } else {
                int offset_to_literal =
                    current_section->literal_pool[op.literal] - lc - 4;
                second_byte = (pc << 4) & 0xF0;
                third_byte =
                    ((source << 4) & 0xF0) | (offset_to_literal >> 8) & 0x0F;
                forth_byte = offset_to_literal & 0xFF;
                current_section->add_bytes(
                    {0x82, second_byte, third_byte, forth_byte});
            }
            lc += 4;
            break;
        case SYM_DIR:
            offset_to_symbol = current_section->symbol_pool[op.symbol] - lc - 4;
            second_byte = (pc << 4) & 0xF0;
            third_byte =
                ((source << 4) & 0xF0) | ((offset_to_symbol >> 8) & 0x0F);
            forth_byte = offset_to_symbol & 0xFF;
            current_section->add_bytes(
                {0x82, second_byte, third_byte, forth_byte});
            lc += 4;
            break;
        case REGDIR:
            cout << "Can't store to register value!" << endl;
            exit(-1);
            break;
        case REGIND:
            destination = op.reg;
            second_byte = (destination << 4) & 0xF0;
            third_byte = (source << 4) & 0xF0;
            current_section->add_bytes({0x80, second_byte, third_byte, 0x00});
            lc += 4;
            break;
        case REG_LIT:
            if (!fits_12_bits(op.literal)) {
                cout << "Literal must fit 12 bits!" << endl;
                exit(-1);
            }

            destination = op.reg;
            second_byte = (destination << 4) & 0xF0;
            third_byte = ((source << 4) & 0xF0) | (op.literal >> 8) & 0x0F;
            forth_byte = op.literal & 0xFF;
            current_section->add_bytes(
                {0x80, second_byte, third_byte, forth_byte});
            lc += 4;
            break;
        case REG_SYM:
            cout << "Symbol value unknown!" << endl;
            exit(-1);
            break;
        }
    } else {
        switch (instruction.operand.adressing_mode) {
        case IMMED:
            cout << "Can't store to literal!" << endl;
            exit(-1);
            break;
        case SYMBOL:
            cout << "Can't store to symbol!" << endl;
            exit(-1);
            break;
        case LIT_DIR:
            if (!fits_12_bits(op.literal)) {
                current_section->register_literal(op.literal);
            }
            lc += 4;
            break;
        case SYM_DIR:
            symbol_used(op.symbol);
            lc += 4;
            break;
        case REGDIR:
            cout << "Can't store to register value!" << endl;
            exit(-1);
            break;
        case REGIND:
        case REG_LIT:
            lc += 4;
            break;
        case REG_SYM:
            cout << "Symbol value unknown!" << endl;
            exit(-1);
            break;
        }
    }
}

void Assembler::csrrd_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char gpr = instruction.gpr1;
        unsigned char csr = instruction.csr;

        unsigned char second_byte = ((gpr << 4) & 0xF0) | (csr & 0x0F);
        current_section->add_bytes({0x90, second_byte, 0x00, 0x00});
    }
    lc += 4;
}

void Assembler::csrwr_instruction(Instruction instruction) {
    if (second_pass) {
        unsigned char gpr = instruction.gpr1;
        unsigned char csr = instruction.csr;

        unsigned char second_byte = ((csr << 4) & 0xF0) | (gpr & 0x0F);
        current_section->add_bytes({0x94, second_byte, 0x00, 0x00});
    }
    lc += 4;
}

bool fits_12_bits(int number) { return number >= 0x800 && number <= 0x7FF; }

void print_section(Section &section) {
    output_file << section.name << "\n";
    output_file << section.length << "\n";
    for (unsigned char byte : section.content) {
        output_file << hex << setw(2) << setfill('0') << (int)byte << ' '
                    << dec;
    }
    output_file << "\n";

    for (const auto &relocation : section.relocations) {
        output_file << relocation.offset << " " << relocation.addend << " "
                    << relocation.symbol << "\n";
    }
    output_file << "---\n";
}

void print_symbol_table(unordered_map<string, Symbol> &sym_tab) {
    output_file << "Symbol table:\n";
    for (const auto &sym : sym_tab) {
        if (sym.second.is_global) {
            output_file << sym.first << " " << sym.second.value << " "
                        << sym.second.is_defined << " "
                        << (sym.second.section_name == ""
                                ? "UND"
                                : sym.second.section_name)
                        << "\n";
        }
    }
}
