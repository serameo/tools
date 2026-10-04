# Makefile - builds xconf WITHOUT CMake and WITHOUT flex/bison.
#
# This assumes the flex/bison-generated files already exist as plain
# C source under src/ (see "Vendoring the generated files" in
# README.md for how to produce them once, from a machine that does
# have flex/bison):
#   src/lexer.yy.c     (generated from src/lexer.l)
#   src/parser.tab.c   (generated from src/parser.y)
#   src/parser.tab.h   (generated from src/parser.y)
#
# Once those three files are present, this Makefile never invokes
# flex or bison again - it just compiles plain C. It builds a static
# library, libxconf.a (no .so), plus every tests/test_*.c as its own
# executable linked against that library.
#
# Usage:
#   make            # build libxconf.a and all test executables
#   make lib        # build only libxconf.a
#   make test       # build (if needed) and run all tests
#   make clean

CC      ?= gcc
AR      ?= ar
CFLAGS  ?= -std=c99 -Wall -Wextra -O2
INCLUDES := -Iinclude -Isrc

SRC_DIR  := src
TEST_DIR := tests

LIB := libxconf.a

# Plain C sources plus the vendored (pre-generated) lexer/parser.
# NOTE: lexer.l and parser.y themselves are NOT compiled here - only
# their already-generated .c/.h counterparts are.
LIB_SRCS := \
	$(SRC_DIR)/xconf.c \
	$(SRC_DIR)/xconf_value.c \
	$(SRC_DIR)/xconf_writer.c \
	$(SRC_DIR)/lexer.yy.c \
	$(SRC_DIR)/parser.tab.c

LIB_OBJS := $(LIB_SRCS:.c=.o)

TEST_SRCS := $(wildcard $(TEST_DIR)/test_*.c)
TEST_BINS := $(patsubst %.c,%,$(TEST_SRCS))

.PHONY: all lib tests test clean check-vendored

all: check-vendored lib tests

# Fail early with a clear message if the generated files were never
# copied in, instead of a confusing "No such file" from the compiler.
check-vendored:
	@for f in $(SRC_DIR)/lexer.yy.c $(SRC_DIR)/parser.tab.c $(SRC_DIR)/parser.tab.h; do \
		if [ ! -f $$f ]; then \
			echo "error: $$f not found."; \
			echo "Generate it once (on a machine with flex/bison) and copy it here - see README.md, 'Vendoring the generated files'."; \
			exit 1; \
		fi; \
	done

lib: check-vendored $(LIB)

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $(LIB_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

tests: lib $(TEST_BINS)

$(TEST_DIR)/%: $(TEST_DIR)/%.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(LIB) -o $@

test: tests
	@status=0; \
	for t in $(TEST_BINS); do \
		echo "== $$t =="; \
		./$$t || status=1; \
	done; \
	exit $$status

clean:
	rm -f $(LIB_OBJS) $(LIB) $(TEST_BINS)
