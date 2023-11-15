.PHONY: all release run clean
NAME = drawing-again

ifeq ($(OS),Windows_NT) # Windows-specific
        OS = windows
else
        ifneq (,$(findstring Linux,$(shell uname -o)))
                OS = linux
        endif
        ifneq (,$(findstring Darwin,$(shell uname -o)))
                OS = osx
        endif
endif

BIN = ./bin/$(NAME)

DEBUG = 0
WITH_TINYFD = 1

OBJS += ./src/main.c
ifeq ($(WITH_TINYFD),1)
        OBJS += ./src/tinyfiledialogs.c
endif

CC = gcc
OUT = -o $(BIN)
INCLUDE_PATHS += -iquote ./include

LD_FLAGS += -m64
CC_FLAGS += -w -Wall
ifeq ($(DEBUG),1)
        CC_FLAGS += -D DEBUG
endif
ifeq ($(WITH_TINYFD),1)
        CC_FLAGS += -D WITH_TINYFD
endif

ifeq ($(OS),windows) # Windows-specific
        ifeq ($(DEBUG),1)
                LD_FLAGS += -mconsole
        else
                LD_FLAGS += -mwindows
        endif
        ifeq ($(WITH_TINYFD),1) # needed for tinyfd
                LIBS += -lcomdlg32 -lole32
        endif
        ifneq (,$(findstring MINGW,$(shell uname))) # MINGW-specific
                LIBS += -lmingw32
        endif
else
        LIBS += -lm # took an hour for me to finally figure this out lol
endif
LIBS += -lSDL2main -lSDL2 -lSDL2_image # must come after -lmingw32

all: $(OBJS)
	@if [[ ! -d $(dir $(BIN)) ]]; then \
		mkdir $(dir $(BIN)); \
	fi
	$(CC) $(OUT) $(OBJS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) $(LIBS) $(LD_FLAGS) $(CC_FLAGS)

release: all
	@if [[ ! -d release/$(NAME)_$(OS)_x86_64 ]]; then \
		mkdir -p release/$(NAME)_$(OS)_x86-64; \
	fi
	cp $(BIN) ./release/$(NAME)_$(OS)_x86-64
ifeq ($(OS),windows)
	cp ./lib/*.dll ./release/$(NAME)_$(OS)_x86-64
endif
	zip -mr release/$(NAME)_$(OS)_x86-64 ./release/$(NAME)_$(OS)_x86-64

run: all
	$(BIN)

clean:
	rm -rf $(dir $(BIN))
