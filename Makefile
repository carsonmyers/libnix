LIBS = 
CFLAGS = -g -O0 -Wall -Iinclude -Iextern -Isrc

PATH_OBJ_LIBNIX = build/libnix/obj
PATH_OBJ_TEST = build/test/obj

PATH_OUT_LIBNIX = build/libnix
PATH_OUT_TEST = build/test
PATH_OUT_RESULTS = build/test/results

PATH_SRC_LIBNIX = src
PATH_SRC_TEST = test

PATH_INCLUDE = include

PATH_UNITY = extern/unity/src

BUILD_PATHS = $(PATH_OBJ_LIBNIX) $(PATH_OBJ_TEST) \
	      $(PATH_OUT_LIBNIX) $(PATH_OUT_TEST) $(PATH_OUT_RESULTS)

LIBNIX_TARGET = libnix.a
LIBNIX_SRC = $(wildcard $(PATH_SRC_LIBNIX)/*.c)
LIBNIX_OBJ = $(addprefix $(PATH_OBJ_LIBNIX)/,$(notdir $(LIBNIX_SRC:.c=.o)))
LIBNIX_H = $(shell find $(PATH_INCLUDE) $(PATH_SRC_LIBNIX) -type f -name '*.h')

TEST_LIB = $(PATH_UNITY)/unity.c
TEST_H = $(PATH_UNITY)/unity.h
TEST_SRC = $(wildcard $(PATH_SRC_TEST)/*.c)
TEST_COMMON = $(wildcard $(PATH_SRC_TEST)/common_test.*)
TEST_OUT = $(addprefix $(PATH_OUT_TEST)/,$(notdir $(TEST_SRC:.c=.out)))
TEST_OBJ = $(addprefix $(PATH_OBJ_TEST)/,$(notdir $(TEST_SRC:.c=.o)))
TEST_RESULTS = $(patsubst \
	       $(PATH_SRC_TEST)/test_%.c,\
	       $(PATH_OUT_RESULTS)/test_%.txt,$(TEST_SRC))

.PHONY: clean
.PHONY: test
.PHONY: setup

default: setup $(LIBNIX_TARGET)

.PRECIOUS: $(LIBNIX_TARGET)
.PRECIOUS: $(LIBNIX_OBJ)
.PRECIOUS: $(TEST_OBJ)
.PRECIOUS: $(TEST_OUT)

$(PATH_OBJ_LIBNIX)/%.o: $(PATH_SRC_LIBNIX)/%.c
	cc $(CFLAGS) -c -o $@ $< 

$(PATH_OBJ_TEST)/unity.o: $(TEST_LIB) $(TEST_H)
	cc $(CFLAGS) -c -o $@ $<

$(PATH_OBJ_TEST)/%.o: $(PATH_SRC_TEST)/%.c $(TEST_COMMON)
	cc $(CFLAGS) -I$(PATH_UNITY) -c -o $@ $<

$(PATH_OUT_TEST)/test_%.out: $(PATH_OBJ_TEST)/test_%.o $(LIBNIX_OBJ) \
    $(PATH_OBJ_TEST)/unity.o $(PATH_OBJ_TEST)/common_test.o
	cc -o $@ $^

$(PATH_OUT_RESULTS)/%.txt: $(PATH_OUT_TEST)/%.out
	@echo "\n"
	@-./$< | tee $@

$(LIBNIX_TARGET): $(LIBNIX_OBJ)
	ar rcs $@ $^

test: setup $(TEST_RESULTS)
	@echo "\n------------------- IGNORES --------------------"
	@grep -s "IGNORE:" $(PATH_OUT_RESULTS)/*.txt || echo " "
	@echo "------------------- FAILURES -------------------"
	@grep -s "FAIL:" $(PATH_OUT_RESULTS)/*.txt || echo " "
	@echo "\nTests complete"

clean:
	rm -rf build

setup:
	@mkdir -p $(BUILD_PATHS)
