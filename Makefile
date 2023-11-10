.PHONY: all
NAME = drawing-again

OBJS = ./src/*

CC = gcc
ifneq (,$(findstring gcc,$(CC)))
        INCLUDE_PATHS = -I ./include
        LD_FLAGS = -m64
        CC_FLAGS = -w -Wall
endif

ifeq ($(OS),Windows_NT)
        LIBS += -lcomdlg32 -lole32
        CC_FLAGS += -Wl,-subsystem=console
        ifneq (,$(findstring MINGW,$(shell uname)))
                LIBS += -lmingw32
        endif

endif
LIBS += -lSDL2main -lSDL2 -lSDL2_image

all: $(OBJS)
	@if [[ ! -d bin ]]; then \
		mkdir bin; \
	fi
	$(CC) -o ./bin/$(NAME) $(OBJS) $(INCLUDE_PATHS) $(LIBS) $(LD_FLAGS) $(CC_FLAGS)
