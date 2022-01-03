# Compiled from Cygwin environment using Mingw64 standalone compiler

# use the full filename of the mingw compiler if it is not in the path
# gcc = "C:/Program Files/mingw-w64/x86_64-8.1.0-posix-seh-rt_v6-rev0/mingw64/bin/g++"
gcc = g++

ifeq ($(DEBUG),1)
	opts_compiler=-g
else
	opts_compiler=-O3
	opts_linker=-mwindows
endif

.PHONY: clean distributable all

MillenniaControlApp.exe: main.o
	$(gcc) $(opts_compiler) $(opts_linker) main.o -lgdi32 -lShlwapi -lstdc++ -o MillenniaControlApp.exe

main.o: main.cpp
	$(gcc) $(opts_compiler) -c main.cpp

all: clean distributable

distributable: MillenniaControlApp.exe
	@cp -v "`which libstdc++-6.dll`" distributable/
	@cp -v "`which libwinpthread-1.dll`" distributable/
	@cp -v MillenniaControlApp.exe distributable/

clean:
	rm -f main.o
