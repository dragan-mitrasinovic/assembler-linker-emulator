#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

class Emulator {
  private:
    unordered_map<unsigned int, unsigned char> mem;
    unsigned int gpr[16];
    unsigned int csr[3];
    unsigned int &pc = gpr[15];
    unsigned int &sp = gpr[14];
    unsigned int &status = csr[0];
    unsigned int &handle = csr[1];
    unsigned int &cause = csr[2];

  public:
    Emulator() {}
    void load_memory(string input_file_name);
    void run();
    void print_state();

    void execute_instruction();
    void int_instruction();
    void call_instruction(vector<unsigned char> &bytes);
    void jmp_instruction(vector<unsigned char> &bytes);
    void xchg_instruction(vector<unsigned char> &bytes);
    void arit_instruction(vector<unsigned char> &bytes);
    void log_instruction(vector<unsigned char> &bytes);
    void sh_instruction(vector<unsigned char> &bytes);
    void st_instruction(vector<unsigned char> &bytes);
    void ld_instruction(vector<unsigned char> &bytes);
    void invalid_instruction();

    void set_gpr(int index, unsigned int value) {
        if (index != 0) {
            gpr[index] = value;
        }
    }

    void push(unsigned int value) {
        for (int i = sizeof(unsigned int) - 1; i >= 0; i--) {
            sp--;
            unsigned char byte = (value >> (i * 8)) & 0xFF;
            mem[sp] = byte;
        }
    }

    unsigned int pop() {
        unsigned int value = 0;
        for (int i = 0; i < sizeof(unsigned int); i++) {
            unsigned char byte = mem[sp];
            value |= ((unsigned int)(byte) << (i * 8));
            sp++;
        }
        return value;
    }

    unsigned int read_word(unsigned int address) {
        unsigned int value = 0;
        for (int i = 0; i < sizeof(unsigned int); i++) {
            unsigned char byte = mem[address];
            value |= ((unsigned int)(byte) << (i * 8));
            address++;
        }
        return value;
    }

    void write_word(unsigned int address, unsigned int value) {
        for (int i = 0; i < sizeof(unsigned int); i++) {
            unsigned char byte = (value >> (i * 8)) & 0xFF;
            mem[address] = byte;
            address++;
        }
    }
};

enum Instructions { HALT, INT, CALL, JUMP, XCHG, ARIT, LOG, SH, ST, LD };
enum Calls { CALL_DIR, CALL_IND };
enum Jumps { JMP, JEQ, JNE, JGT, BRANCH = 8, BEQ, BNE, BGT };
enum Aritmethic { ADD, SUB, MUL, DIV };
enum Logic { NOT, AND, OR, XOR };
enum Shift { SHL, SHR };
enum Stores { ST_DIR, ST_PUSH, ST_IND };
enum Loads {
    GPR_CSR,
    GPR_GPR,
    GPR_MEM,
    GPR_POP,
    CSR_GPR,
    CSR_CSR,
    CSR_MEM,
    CSR_POP
};