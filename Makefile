.PHONY: all
NAME = drawing-again

CC = gcc

OBJS = ./src/*
INCLUDE_PATHS = -I ./include

ifeq ($(OS),Windows_NT)
        ifneq (,$(findstring MINGW,$(shell uname)))
                LIBS += -lmingw32
        endif
        LIBS += -lcomdlg32 -lole32
endif
LIBS += -lSDL2main -lSDL2 -lSDL2_image

LD_FLAGS = -m64
CC_FLAGS = -w -Wl,-subsystem=console -Wall

all: $(OBJS)
	$(CC) -o ./bin/$(NAME) $(OBJS) $(INCLUDE_PATHS) $(LIBS) $(LD_FLAGS) $(CC_FLAGS)
