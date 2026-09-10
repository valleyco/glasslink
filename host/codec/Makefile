CC ?= gcc
CFLAGS ?= -O0 -g -Wall -Wextra -Werror
ROOT := ../..

CPPFLAGS := \
	-I$(ROOT)/components/codec/include \
	-I$(ROOT)/host/common

CODEC_SRCS := \
	$(ROOT)/components/codec/src/codec.c \
	$(ROOT)/components/codec/src/codec_raw.c

TEST_BINS := test_codec_raw

.PHONY: all test clean

all: test

test_codec_raw: test_codec_raw.c $(CODEC_SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_codec_raw.c $(CODEC_SRCS)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "== $$t =="; ./$$t || exit 1; done

clean:
	$(RM) $(TEST_BINS)
