CC ?= gcc
CFLAGS ?= -O0 -g -Wall -Wextra -Werror
ROOT := ../..

CPPFLAGS := \
	-I$(ROOT)/components/board/include \
	-I$(ROOT)/components/render/include \
	-I$(ROOT)/host/fake_display \
	-I$(ROOT)/host/common

SRCS := \
	$(ROOT)/components/render/src/render.c \
	$(ROOT)/host/fake_display/fake_display.c

TEST_BINS := test_render_basic

.PHONY: all test clean

all: test

test_render_basic: test_render_basic.c $(SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_render_basic.c $(SRCS)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "== $$t =="; ./$$t || exit 1; done

clean:
	$(RM) $(TEST_BINS)
