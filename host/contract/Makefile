CC ?= gcc
CFLAGS ?= -O0 -g -Wall -Wextra -Werror
ROOT := ../..

CPPFLAGS := \
	-I$(ROOT)/components/contract/include \
	-I$(ROOT)/components/codec/include \
	-I$(ROOT)/components/render/include \
	-I$(ROOT)/components/board/include \
	-I$(ROOT)/host/fake_display \
	-I$(ROOT)/host/common

CODEC_SRCS := \
	$(ROOT)/components/codec/src/codec.c \
	$(ROOT)/components/codec/src/codec_raw.c \
	$(ROOT)/components/codec/src/codec_delta_rle.c

PARSE_SRCS := $(ROOT)/components/contract/src/contract_parse.c
APPLY_SRCS := $(ROOT)/components/contract/src/contract_apply.c

RENDER_SRCS := \
	$(ROOT)/components/render/src/render.c \
	$(ROOT)/host/fake_display/fake_display.c

TEST_BINS := test_contract_parse test_contract_apply

.PHONY: all test clean

all: test

test_contract_parse: test_contract_parse.c $(PARSE_SRCS) $(CODEC_SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_contract_parse.c $(PARSE_SRCS) \
		$(CODEC_SRCS)

test_contract_apply: test_contract_apply.c $(PARSE_SRCS) $(APPLY_SRCS) $(CODEC_SRCS) $(RENDER_SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_contract_apply.c $(PARSE_SRCS) \
		$(APPLY_SRCS) $(CODEC_SRCS) $(RENDER_SRCS)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "== $$t =="; ./$$t || exit 1; done

clean:
	$(RM) $(TEST_BINS)
