.PHONY: all
NAME = drawing-again

DEBUG = 0
WITH_TINYFD = 1

OBJS += ./src/main.c
ifeq ($(WITH_TINYFD),1)
        OBJS += ./src/tinyfiledialogs.c
endif

CC = gcc
OUT = -o ./bin/$(NAME)
INCLUDE_PATHS += -I ./include

LD_FLAGS += -m64
CC_FLAGS += -w -Wall
ifeq ($(DEBUG),1)
        CC_FLAGS += -D DEBUG
endif
ifeq ($(WITH_TINYFD),1)
        CC_FLAGS += -D WITH_TINYFD
endif

ifeq ($(OS),Windows_NT) # Windows-specific
        ifneq (,$(findstring MINGW,$(shell uname))) # MINGW-specific
                LIBS += -lmingw32
                CC_FLAGS += -Wl,-subsystem=console
        ifeq ($(DEBUG),1)
                LD_FLAGS += -mconsole
        else
                LD_FLAGS += -mwindows
        endif
        ifeq ($(WITH_TINYFD),1) # needed for tinyfd
                LIBS += -lcomdlg32 -lole32
        endif
else
        LIBS += -lm # took an hour for me to finally figure this out lol
endif
LIBS += -lSDL2main -lSDL2 -lSDL2_image # must come after -lmingw32

all: $(OBJS)
	@if [[ ! -d bin ]]; then \
		mkdir bin; \
	fi
	$(CC) $(OUT) $(OBJS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) $(LIBS) $(LD_FLAGS) $(CC_FLAGS)

run: all
	./bin/$(NAME)
