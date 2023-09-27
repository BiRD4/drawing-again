NAME = drawing-again

CC = gcc

OBJS = main.c
LIBS = -lmingw32 -lSDL2main -lSDL2

LD_FLAGS = -m64 -s
CC_FLAGS = -w -Wl,-subsystem=console -Wall

INCLUDE_PATHS = -I 'd:/Moved Downloads/12-16-2022/SDL2-2.0.12/x86_64-w64-mingw32/include'
LIBRARY_PATHS = -L 'd:/Moved Downloads/12-16-2022/SDL2-2.0.12/x86_64-w64-mingw32/lib'

all: $(OBJS)
	$(CC) -o $(NAME) $(OBJS) $(LIBS) $(LD_FLAGS) $(CC_FLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS)
