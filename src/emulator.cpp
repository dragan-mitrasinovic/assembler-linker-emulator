#include <fstream>
#include <iomanip>
#include <iostream>

#include "emulator.hpp"

using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cout << "Expected 1 argument, got " << argc - 1 << "!" << endl;
        return -1;
    }
    Emulator emulator;
    emulator.load_memory(argv[1]);
    emulator.run();
    return 0;
}

void Emulator::load_memory(string input_file_name) {
    ifstream file(input_file_name);
    string line;
    unsigned int address;
    while (getline(file, line)) {
        stringstream line_stream(line);
        string address_string;
        line_stream >> address_string;
        address_string.pop_back();
        address = stoul(address_string, nullptr, 16);

        unsigned int byte;
        while (line_stream >> hex >> byte) {
            mem[address] = byte;
            address++;
        }
    }
    file.close();
}

void Emulator::print_state() {
    cout << "-----------------------------------------------------------------"
         << "\n";
    cout << "Emulated processor state:";
    for (int i = 0; i < 16; i++) {
        if (i % 4 == 0) {
            cout << "\n";
        }
        if (i < 10) {
            cout << " ";
        }
        cout << "r" << dec << i << "=0x" << hex << setw(8) << setfill('0')
             << gpr[i] << "\t";
    }
    cout << "\n";
}

void Emulator::run() {
    for (int i = 0; i < 16; i++) {
        gpr[i] = 0;
    }
    for (int i = 0; i < 3; i++) {
        csr[i] = 0;
    }
    pc = 0x40000000;

    while (true) {
        execute_instruction();
    }
}

void Emulator::execute_instruction() {
    vector<unsigned char> bytes;
    for (int i = 0; i < 4; i++) {
        bytes.push_back(mem[pc]);
        pc++;
    }

    switch ((bytes[0] >> 4) & 0x0F) {
    case HALT:
        print_state();
        exit(0);
        break;
    case INT:
        int_instruction();
        break;
    case CALL:
        call_instruction(bytes);
        break;
    case JUMP:
        jmp_instruction(bytes);
        break;
    case XCHG:
        xchg_instruction(bytes);
        break;
    case ARIT:
        arit_instruction(bytes);
        break;
    case LOG:
        log_instruction(bytes);
        break;
    case SH:
        sh_instruction(bytes);
        break;
    case ST:
        st_instruction(bytes);
        break;
    case LD:
        ld_instruction(bytes);
        break;
    default:
        invalid_instruction();
        break;
    }
}

void Emulator::int_instruction() {
    push(pc);
    push(status);
    cause = 4;
    status = status & (~0x1);
    pc = handle;
}

void Emulator::call_instruction(vector<unsigned char> &bytes) {
    unsigned char a = (bytes[1] >> 4) & 0x0F;
    unsigned char b = bytes[1] & 0x0F;
    int d = ((bytes[2] << 8) & 0xF00) | bytes[3];

    if (d & 0x800) {
        d |= 0xFFFFF000;
    }

    push(pc);
    switch (bytes[0] & 0x0F) {
    case CALL_DIR:
        pc = gpr[a] + gpr[b] + d;
        break;
    case CALL_IND:
        pc = read_word(gpr[a] + gpr[b] + d);
        break;
    default:
        invalid_instruction();
        break;
    }
}

void Emulator::jmp_instruction(vector<unsigned char> &bytes) {
    unsigned char a = (bytes[1] >> 4) & 0x0F;
    unsigned char b = bytes[1] & 0x0F;
    unsigned char c = (bytes[2] >> 4) & 0x0F;
    int d = ((bytes[2] << 8) & 0xF00) | bytes[3];

    if (d & 0x800) {
        d |= 0xFFFFF000;
    }

    switch (bytes[0] & 0x0F) {
    case JMP:
        pc = gpr[a] + d;
        break;
    case JEQ:
        if (gpr[b] == gpr[c])
            pc = gpr[a] + d;
        break;
    case JNE:
        if (gpr[b] != gpr[c])
            pc = gpr[a] + d;
        break;
    case JGT:
        if ((int)gpr[b] > (int)gpr[c])
            pc = gpr[a] + d;
        break;
    case BRANCH:
        pc = read_word(gpr[a] + d);
        break;
    case BEQ:
        if (gpr[b] == gpr[c])
            pc = read_word(gpr[a] + d);
        break;
    case BNE:
        if (gpr[b] != gpr[c])
            pc = read_word(gpr[a] + d);
        break;
    case BGT:
        if ((int)gpr[b] > (int)gpr[c])
            pc = read_word(gpr[a] + d);
        break;
    default:
        invalid_instruction();
        break;
    }
}

void Emulator::xchg_instruction(vector<unsigned char> &bytes) {
    unsigned char b = bytes[1] & 0x0F;
    unsigned char c = (bytes[2] >> 4) & 0x0F;

    unsigned int temp = gpr[b];
    set_gpr(b, gpr[c]);
    set_gpr(c, temp);
}

void Emulator::arit_instruction(vector<unsigned char> &bytes) {
    unsigned char a = (bytes[1] >> 4) & 0x0F;
    unsigned char b = bytes[1] & 0x0F;
    unsigned char c = (bytes[2] >> 4) & 0x0F;

    switch (bytes[0] & 0x0F) {
    case ADD:
        set_gpr(a, gpr[b] + gpr[c]);
        break;
    case SUB:
        set_gpr(a, gpr[b] - gpr[c]);
        break;
    case MUL:
        set_gpr(a, gpr[b] * gpr[c]);
        break;
    case DIV:
        set_gpr(a, gpr[b] / gpr[c]);
        break;
    default:
        invalid_instruction();
        break;
    }
}

void Emulator::log_instruction(vector<unsigned char> &bytes) {
    unsigned char a = (bytes[1] >> 4) & 0x0F;
    unsigned char b = bytes[1] & 0x0F;
    unsigned char c = (bytes[2] >> 4) & 0x0F;

    switch (bytes[0] & 0x0F) {
    case NOT:
        set_gpr(a, ~gpr[b]);
        break;
    case AND:
        set_gpr(a, gpr[b] & gpr[c]);
        break;
    case OR:
        set_gpr(a, gpr[b] | gpr[c]);
        break;
    case XOR:
        set_gpr(a, gpr[b] ^ gpr[c]);
        break;
    default:
        invalid_instruction();
        break;
    }
}

void Emulator::sh_instruction(vector<unsigned char> &bytes) {
    unsigned char a = (bytes[1] >> 4) & 0x0F;
    unsigned char b = bytes[1] & 0x0F;
    unsigned char c = (bytes[2] >> 4) & 0x0F;

    switch (bytes[0] & 0x0F) {
    case SHL:
        set_gpr(a, gpr[b] << gpr[c]);
        break;
    case SHR:
        set_gpr(a, gpr[b] >> gpr[c]);
        break;
    default:
        invalid_instruction();
        break;
    }
}

void Emulator::st_instruction(vector<unsigned char> &bytes) {
    unsigned char a = (bytes[1] >> 4) & 0x0F;
    unsigned char b = bytes[1] & 0x0F;
    unsigned char c = (bytes[2] >> 4) & 0x0F;
    int d = ((bytes[2] << 8) & 0xF00) | bytes[3];

    if (d & 0x800) {
        d |= 0xFFFFF000;
    }

    switch (bytes[0] & 0x0F) {
    case ST_DIR:
        write_word(gpr[a] + gpr[b] + d, gpr[c]);
        break;
    case ST_IND:
        write_word(read_word(gpr[a] + gpr[b] + d), gpr[c]);
        break;
    case ST_PUSH:
        set_gpr(a, (int)gpr[a] + d);
        write_word(gpr[a], gpr[c]);
        break;
    default:
        invalid_instruction();
        break;
    }
}

void Emulator::ld_instruction(vector<unsigned char> &bytes) {
    unsigned char a = (bytes[1] >> 4) & 0x0F;
    unsigned char b = bytes[1] & 0x0F;
    unsigned char c = (bytes[2] >> 4) & 0x0F;
    int d = ((bytes[2] << 8) & 0xF00) | bytes[3];

    if (d & 0x800) {
        d |= 0xFFFFF000;
    }

    switch (bytes[0] & 0x0F) {
    case GPR_CSR:
        set_gpr(a, csr[b]);
        break;
    case GPR_GPR:
        set_gpr(a, gpr[b] + d);
        break;
    case GPR_MEM:
        set_gpr(a, read_word(gpr[b] + gpr[c] + d));
        break;
    case GPR_POP:
        set_gpr(a, read_word(gpr[b]));
        set_gpr(b, gpr[b] + d);
        break;
    case CSR_GPR:
        csr[a] = gpr[b];
        break;
    case CSR_CSR:
        csr[a] = csr[b] + d;
        break;
    case CSR_MEM:
        csr[a] = read_word(gpr[b] + gpr[c] + d);
        break;
    case CSR_POP:
        csr[a] = read_word(gpr[b]);
        set_gpr(b, gpr[b] + d);
        break;
    default:
        invalid_instruction();
        break;
    }
}

void Emulator::invalid_instruction() {
    push(pc);
    push(status);
    cause = 1;
    status = status & (~0x1);
    pc = handle;
}