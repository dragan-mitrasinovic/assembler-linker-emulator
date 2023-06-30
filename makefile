SOURCE_ASSEMBLER = \
misc/lex.yy.cpp \
misc/parser.tab.cpp \
src/assembler.cpp

INCLUDE_ASSEMBLER = \
misc/parser.tab.hpp \
inc/assembler.hpp

SOURCE_LINKER = \
src/linker.cpp

INCLUDE_LINKER = \
inc/linker.hpp

SOURCE_EMULATOR = \
src/emulator.cpp

INCLUDE_EMULATOR = \
inc/emulator.hpp

misc/parser.tab.cpp misc/parser.tab.hpp: misc/parser.y
	bison -d -o misc/parser.tab.cpp misc/parser.y 

misc/lex.yy.cpp: misc/lexer.l misc/parser.tab.hpp
	flex -o misc/lex.yy.cpp misc/lexer.l

assembler: $(INCLUDE_ASSEMBLER) $(SOURCE_ASSEMBLER)
	g++ -o assembler $(^) -Iinc -Imisc

linker: $(INCLUDE_LINKER) $(SOURCE_LINKER)
	g++ -o linker $(^) -Iinc

emulator: $(INCLUDE_EMULATOR) $(SOURCE_EMULATOR)
	g++ -o emulator $(^) -Iinc

all: assembler linker emulator

clean:
	rm -f misc/lex.yy.cpp misc/parser.tab.cpp misc/parser.tab.hpp assembler linker emulator *.o program.hex