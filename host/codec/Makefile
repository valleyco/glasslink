CC ?= gcc
CFLAGS ?= -O0 -g -Wall -Wextra -Werror
BENCH_CFLAGS ?= -O2 -g -Wall -Wextra -Werror
ROOT := ../..

CPPFLAGS := \
	-I$(ROOT)/components/codec/include \
	-I$(ROOT)/host/common \
	-Igen

CODEC_SRCS := \
	$(ROOT)/components/codec/src/codec.c \
	$(ROOT)/components/codec/src/codec_raw.c \
	$(ROOT)/components/codec/src/codec_delta_rle.c

GEN_SRCS := gen/corpus_gen.c

TEST_BINS := test_codec_raw test_codec_delta_rle
BENCH_BIN := bench_codec
MD_OUT ?= $(ROOT)/docs/benches/codec-bench-mvp.md

.PHONY: all test bench clean

all: test

test_codec_raw: test_codec_raw.c $(CODEC_SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_codec_raw.c $(CODEC_SRCS)

test_codec_delta_rle: test_codec_delta_rle.c $(CODEC_SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_codec_delta_rle.c $(CODEC_SRCS)

$(BENCH_BIN): bench_main.c $(CODEC_SRCS) $(GEN_SRCS)
	$(CC) $(BENCH_CFLAGS) $(CPPFLAGS) -o $@ bench_main.c $(CODEC_SRCS) $(GEN_SRCS) -lm

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "== $$t =="; ./$$t || exit 1; done

# CLASS=solid,ui_panel ENC=delta_rle_v1 optional
bench: $(BENCH_BIN)
	./$(BENCH_BIN) --md-out $(MD_OUT) $(BENCH_ARGS)

clean:
	$(RM) $(TEST_BINS) $(BENCH_BIN)
