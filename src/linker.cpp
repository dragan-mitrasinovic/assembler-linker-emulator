#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>

#include "linker.hpp"

using namespace std;

int main(int argc, char *argv[]) {
    string output_name;
    vector<string> files;
    map<unsigned int, string> place_options;
    bool hex_appeared = false;
    bool out_appeared = false;
    regex pattern(R"(^-place=(.*?)@(0x[0-9a-fA-F]+)$)");

    for (int i = 1; i < argc; i++) {
        string arg = string(argv[i]);
        if (arg == "-hex") {
            hex_appeared = true;
            continue;
        }

        if (arg == "-o") {
            output_name = string(argv[i + 1]);
            out_appeared = true;
            i++;
            continue;
        }

        smatch matches;
        if (regex_match(arg, matches, pattern)) {
            string section = matches[1];
            unsigned int address = stoul(matches[2], nullptr, 16);
            place_options[address] = section;
            continue;
        }

        files.push_back(arg);
    }

    if (!hex_appeared) {
        cout << "-hex option is mandatory" << endl;
        exit(-1);
    }
    if (!out_appeared) {
        cout << "-o <oputput_name> option is mandatory" << endl;
        exit(-1);
    }

    Linker linker;
    linker.read_files(files);
    linker.place_sections(place_options, files);
    linker.update_symbols();
    linker.relocate();
    linker.output(output_name);

    return 0;
}

void Linker::read_files(vector<string> files) {
    for (string file_name : files) {
        string line;
        ifstream file(file_name);
        vector<Section> file_sections;

        while (getline(file, line)) {
            if (line == "Symbol table:") {
                break;
            }

            Section section;
            section.name = line;
            string length_text;
            string bytes_text;

            getline(file, length_text);
            section.length = stoul(length_text);

            getline(file, bytes_text);
            stringstream bytes(bytes_text);
            unsigned int byte;
            while (bytes >> hex >> byte) {
                section.content.push_back(byte);
            }

            string relocation_line;
            getline(file, relocation_line);
            while (relocation_line != "---") {
                Relocation relocation;
                stringstream rel_ss(relocation_line);
                rel_ss >> relocation.offset >> relocation.addend >>
                    relocation.symbol;
                section.relocations.push_back(relocation);
                getline(file, relocation_line);
            }
            file_sections.push_back(section);
        }

        while (getline(file, line)) {
            if (line == "") {
                continue;
            }
            Symbol symbol;
            stringstream sym_ss(line);
            sym_ss >> symbol.name >> symbol.value >> symbol.is_defined >>
                symbol.section_name;
            symbol.file_name = file_name;
            add_symbol(symbol);
        }

        sections[file_name] = file_sections;
        file.close();
    }
}

void Linker::add_symbol(Symbol symbol) {
    if (symbol_table.find(symbol.name) != symbol_table.end() &&
        symbol_table[symbol.name].is_defined) {
        cout << "Symbol " << symbol.name << " defined multiple times!" << endl;
        exit(-1);
    }
    symbol_table[symbol.name] = symbol;
}

void Linker::check_for_undefined_symbols() {
    for (auto &entry : symbol_table) {
        if (!entry.second.is_defined) {
            cout << "Symbol " << entry.first << " was not defined!" << endl;
            exit(-1);
        }
    }
}

void Linker::output(string output_file_name) {
    map<unsigned int, vector<unsigned char>> prints;
    for (auto &section : out_sections) {
        prints[section.second.location] = section.second.content;
    }

    unsigned int loc = 0;
    int cnt = 0;
    bool first = true;

    ofstream output_file(output_file_name);
    for (auto &entry : prints) {
        if (loc != entry.first) {
            loc = entry.first;
            cnt = 0;
            if (first) {
                first = false;
            } else {
                output_file << "\n";
            }
            output_file << hex << setw(4) << setfill('0') << loc << ": ";
        }
        for (auto byte : entry.second) {
            if (cnt == 8) {
                output_file << "\n";
                cnt = 0;
                output_file << hex << setw(4) << setfill('0') << loc << ": ";
            }

            output_file << hex << setw(2) << setfill('0') << (unsigned int)byte;
            loc++;
            cnt++;
            if (cnt != 8) {
                output_file << " ";
            }
        }
    }
    output_file.close();
}

Section *Linker::get_section(string file_name, string section_name) {
    for (auto &sec : sections[file_name]) {
        if (sec.name == section_name) {
            return &sec;
        }
    }
    return nullptr;
}

bool Linker::file_contains_section(string file_name, string section_name) {
    for (auto &sec : sections[file_name]) {
        if (sec.name == section_name) {
            return true;
        }
    }
    return false;
}

void Linker::place_sections(map<unsigned int, string> place_options,
                            vector<string> files) {
    for (auto &place : place_options) {
        if (lc > place.first) {
            cout << "Section " << place.second
                 << " can't be placed the following way due to overlap!"
                 << endl;
            exit(-1);
        }
        lc = place.first;
        string section_name = place.second;
        for (string &file : files) {
            if (!file_contains_section(file, section_name)) {
                continue;
            }
            Section &section = *get_section(file, section_name);
            if (lc + section.length > 0xFFFFFFFF) {
                cout << "Section " << section.name
                     << " can't be placed the following way, end of memory!"
                     << endl;
                exit(-1);
            }

            if (out_sections.find(section_name) == out_sections.end()) {
                Section out_sec;
                out_sec.location = lc;
                out_sec.length = section.length;
                out_sec.content = section.content;
                out_sections[section_name] = out_sec;
            } else {
                Section &out_sec = out_sections[section_name];
                out_sec.length += section.length;
                out_sec.content.insert(out_sec.content.end(),
                                       section.content.begin(),
                                       section.content.end());
            }

            section.location = lc;
            section.in_output = true;
            lc += section.length;
        }
    }

    for (string &file : files) {
        for (Section &sec : sections[file]) {
            if (sec.in_output) {
                continue;
            }

            if (lc + sec.length > 0xFFFFFFFF) {
                cout << "Section " << sec.name
                     << " can't be placed the following way, end of memory!"
                     << endl;
                exit(-1);
            }

            if (out_sections.find(sec.name) == out_sections.end()) {
                Section out_sec;
                out_sec.location = lc;
                out_sec.length = sec.length;
                out_sec.content = sec.content;
                out_sections[sec.name] = out_sec;
            } else {
                Section &out_sec = out_sections[sec.name];
                out_sec.length += sec.length;
                out_sec.content.insert(out_sec.content.end(),
                                       sec.content.begin(), sec.content.end());
            }

            sec.location = lc;
            sec.in_output = true;
            lc += sec.length;
        }
    }
}

void Linker::update_symbols() {
    for (auto &entry : symbol_table) {
        Symbol &sym = entry.second;
        sym.value += get_section(sym.file_name, sym.section_name)->location;
    }
}

void Linker::relocate() {
    for (auto &file_entry : sections) {
        for (auto &section : file_entry.second) {
            for (auto &reloc : section.relocations) {
                if (symbol_table.find(reloc.symbol) != symbol_table.end()) {
                    write_word(section.name, reloc.offset + section.location,
                               symbol_table[reloc.symbol].value);
                } else {
                    write_word(section.name, reloc.offset + section.location,
                               section.location + reloc.addend);
                }
            }
        }
    }
}

void Linker::write_word(string section, unsigned int location,
                        unsigned int value) {
    vector<unsigned char> &content = out_sections[section].content;
    location -= out_sections[section].location;

    for (int i = 0; i < sizeof(unsigned int); ++i) {
        unsigned char byte = (value >> (i * 8)) & 0xFF;
        content[location] = byte;
        location++;
    }
}