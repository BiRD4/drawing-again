.PHONY: all
NAME = drawing-again

WITH_TINYFD = 1

OBJS = ./src/main.c
ifeq ($(WITH_TINYFD),1)
        OBJS += ./src/tinyfiledialogs.c
endif

CC = gcc
OUT = -o ./bin/$(NAME)
INCLUDE_PATHS = -I ./include

LD_FLAGS = -m64
CC_FLAGS = -w -Wall
ifeq ($(WITH_TINYFD),1)
        CC_FLAGS += -D WITH_TINYFD
endif

ifeq ($(OS),Windows_NT) # Windows-specific
        ifneq (,$(findstring MINGW,$(shell uname))) # MINGW-specific
                CC_FLAGS += -Wl,-subsystem=console
                LIBS += -lmingw32
        endif
        ifeq ($(WITH_TINYFD),1) # needed for tinyfd
                LIBS += -lcomdlg32 -lole32
        endif

endif
LIBS += -lSDL2main -lSDL2 -lSDL2_image # must come after -lmingw32

all: $(OBJS)
	@if [[ ! -d bin ]]; then \
		mkdir bin; \
	fi
	$(CC) $(OUT) $(OBJS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) $(LD_FLAGS) $(CC_FLAGS) $(LIBS)
