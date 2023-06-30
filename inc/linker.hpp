#include <map>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

struct Symbol {
    string name;
    unsigned int value;
    bool is_defined;
    string section_name;
    string file_name;
};

struct Relocation {
    unsigned int offset;
    int addend;
    string symbol;
};

struct Section {
    vector<unsigned char> content;
    vector<Relocation> relocations;
    string name;
    int length = 0;
    unsigned int location = 0;
    bool in_output = false;
};

class Linker {
  private:
    unsigned long lc = 0;
    unordered_map<string, vector<Section>> sections;
    unordered_map<string, Symbol> symbol_table;
    unordered_map<string, Section> out_sections;

  public:
    void read_files(vector<string> files);
    void check_for_undefined_symbols();
    void place_sections(map<unsigned int, string> place_options,
                        vector<string> files);
    void update_symbols();
    void relocate();
    void output(string output_file_name);

    void add_symbol(Symbol symbol);
    Section *get_section(string file_name, string section_name);
    bool file_contains_section(string file_name, string section_name);
    void write_word(string section, unsigned int location, unsigned int value);
};