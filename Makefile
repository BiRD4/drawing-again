NAME = drawing-again

CC = gcc

OBJS = ./src/*
LIBS = -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lcomdlg32 -lole32

LD_FLAGS = -m64 -s
CC_FLAGS = -w -Wl,-subsystem=console -Wall

INCLUDE_PATHS = -I './SDL2-2.0.12/x86_64-w64-mingw32/include' -I './include'
LIBRARY_PATHS = -L './SDL2-2.0.12/x86_64-w64-mingw32/lib'

all: $(OBJS)
	$(CC) -o ./bin/$(NAME) $(OBJS) $(LIBS) $(LD_FLAGS) $(CC_FLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS)
