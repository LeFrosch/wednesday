CC          := clang
FORMAT      := clang-format
CFLAGS 	    := -Wall -Wextra -Werror -std=gnu17 -O2 -Iinclude
DEBUG_FLAGS := -g -DDEBUG

UNAME := $(shell uname -s)

# select platform-specific settings
ifeq ($(UNAME), Darwin)
	SANITIZER := -fsanitize=address,undefined
else ifeq ($(UNAME), Linux)
	SANITIZER := -fsanitize=address,undefined,leak
else
	$(error Unsupported platform: $(UNAME))
endif

DEBUG_FLAGS += $(SANITIZER)

SRCS := $(addprefix src/, page.c)
INCLUDES := $(wildcard include/*.h)

LIB_OBJS := $(SRCS:src/%.c=build/lib/%.o)
TEST_OBJS := $(SRCS:src/%.c=build/test/%.o) build/test/test.o

$(LIB_OBJS): build/lib/%.o: src/%.c $(INCLUDES)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TEST_OBJS): build/test/%.o: src/%.c $(INCLUDES) 
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -DSNOW_ENABLED -c -o $@ $<

lib.so: $(LIB_OBJS)
	$(CC) -shared $^ -o $@

lib.dylib: $(LIB_OBJS)
	$(CC) -dynamiclib $^ -o $@

test: $(TEST_OBJS) build/test/test.o
	$(CC) $(SANITIZER) $^ -o $@

clean:
	rm -rf build
	rm -f lib.* test

format:
	find . -name '*.c' -o -name '*.h' | xargs $(FORMAT) -i

.PHONY: clean format
