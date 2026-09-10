CC ?= gcc
CFLAGS ?= -O0 -g -Wall -Wextra -Werror
ROOT := ../..

CPPFLAGS := \
	-I$(ROOT)/components/contract/include \
	-I$(ROOT)/components/codec/include \
	-I$(ROOT)/components/render/include \
	-I$(ROOT)/components/board/include \
	-I$(ROOT)/host/fake_display

SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS := $(shell pkg-config --libs sdl2)

SRCS := \
	$(ROOT)/components/contract/src/contract_parse.c \
	$(ROOT)/components/contract/src/contract_apply.c \
	$(ROOT)/components/codec/src/codec.c \
	$(ROOT)/components/codec/src/codec_raw.c \
	$(ROOT)/components/render/src/render.c \
	$(ROOT)/host/fake_display/fake_display.c

BIN := wd-sim

.PHONY: all run run-demo clean

all: $(BIN)

$(BIN): main.c $(SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -o $@ main.c $(SRCS) $(SDL_LIBS)

run run-demo: $(BIN)
	./$(BIN) --demo

clean:
	$(RM) $(BIN)
