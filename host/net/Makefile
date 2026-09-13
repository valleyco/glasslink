CC ?= gcc
CFLAGS ?= -O0 -g -Wall -Wextra -Werror
ROOT := ../..

CPPFLAGS := -I$(ROOT)/components/wd_net/include

FORM_SRC := $(ROOT)/components/wd_net/src/wd_form.c

.PHONY: all test clean

all: test

test_wd_form: test_wd_form.c $(FORM_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_wd_form.c $(FORM_SRC)

test: test_wd_form
	./test_wd_form
	python3 test_glasslink.py

clean:
	rm -f test_wd_form
