CC ?= gcc
CFLAGS ?= -O0 -g -Wall -Wextra -Werror
ROOT := ../..

CPPFLAGS := \
	-I$(ROOT)/components/bind/include \
	-I$(ROOT)/components/render/include \
	-I$(ROOT)/components/board/include \
	-I$(ROOT)/host/fake_display \
	-I$(ROOT)/host/common

SRCS := \
	$(ROOT)/components/bind/src/bind.c \
	$(ROOT)/components/render/src/render.c \
	$(ROOT)/host/fake_display/fake_display.c

.PHONY: all test clean

all: test

test_bind: test_bind.c $(SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_bind.c $(SRCS)

test: test_bind
	@echo "== test_bind =="; ./test_bind

clean:
	$(RM) test_bind
