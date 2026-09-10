CC ?= gcc
CFLAGS ?= -O0 -g -Wall -Wextra -Werror
ROOT := ../..

CPPFLAGS := \
	-I$(ROOT)/components/contract/include \
	-I$(ROOT)/components/codec/include \
	-I$(ROOT)/components/render/include \
	-I$(ROOT)/components/bind/include \
	-I$(ROOT)/components/board/include \
	-I$(ROOT)/host/fake_display \
	-I$(ROOT)/host/common

SRCS_COMMON := \
	$(ROOT)/components/contract/src/contract_parse.c \
	$(ROOT)/components/contract/src/contract_apply.c \
	$(ROOT)/components/codec/src/codec.c \
	$(ROOT)/components/codec/src/codec_raw.c \
	$(ROOT)/components/codec/src/codec_delta_rle.c \
	$(ROOT)/components/render/src/render.c \
	$(ROOT)/components/bind/src/bind.c \
	$(ROOT)/host/fake_display/fake_display.c

BINS := apply_bin encode_rect

.PHONY: all test clean

all: $(BINS)

apply_bin: apply_bin.c $(SRCS_COMMON)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ apply_bin.c $(SRCS_COMMON)

encode_rect: encode_rect.c \
	$(ROOT)/components/codec/src/codec.c \
	$(ROOT)/components/codec/src/codec_raw.c \
	$(ROOT)/components/codec/src/codec_delta_rle.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ encode_rect.c \
		$(ROOT)/components/codec/src/codec.c \
		$(ROOT)/components/codec/src/codec_raw.c \
		$(ROOT)/components/codec/src/codec_delta_rle.c

test: all
	@echo "run: make mqtt-loopback (root)"

clean:
	$(RM) $(BINS)
