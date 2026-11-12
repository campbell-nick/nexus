CC ?= cc
LDFLAGS ?=
LDLIBS ?= -lm

# -Wall -Wextra catch real issues. GCC's truncation warnings are false positives
# here: every copy is either a deliberately bounded snprintf or a strncpy that is
# null-terminated right after (long system strings are meant to be clipped).
# clang does not know -Wno-stringop-truncation, so each extra flag is only added
# when the compiler accepts it (-Werror turns an unknown-flag warning into an
# error, which is what makes the probe reliable).
try_flag = $(shell printf 'int main(void){return 0;}' | $(1) -Werror $(2) -x c -c - -o /dev/null 2>/dev/null && echo $(2))

WARNINGS := -Wall -Wextra
WARNINGS += $(call try_flag,$(CC),-Wno-stringop-truncation)
WARNINGS += $(call try_flag,$(CC),-Wno-format-truncation)

CFLAGS ?= -O2 $(WARNINGS)

# Probe for statx() so the Age field works on glibc (>= 2.28) and musl (>= 1.2.4).
# Older libcs simply omit it. The probe avoids '#' (a make comment) and newlines
# by pulling the headers in with -include; it is kept in its own variable so it
# survives a user-supplied CFLAGS on the command line.
STATX_PROBE = int main(void){ struct statx s; return statx(AT_FDCWD, "/", 0, 0, &s); }
HAVE_STATX := $(shell printf '%s' '$(STATX_PROBE)' | $(CC) -D_GNU_SOURCE -include fcntl.h -include sys/stat.h -x c -c - -o /dev/null >/dev/null 2>&1 && echo 1)
DEFS := $(if $(HAVE_STATX),-DNEXUS_HAVE_STATX=1)

# Static linking (common with musl for a fully portable binary): make STATIC=1.
ifdef STATIC
LDFLAGS += -static
endif

BIN := nexus
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

PREFIX ?= /usr/local

.PHONY: all clean install musl static

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

src/%.o: src/%.c src/nexus.h
	$(CC) $(CFLAGS) $(DEFS) -c -o $@ $<

# Convenience wrappers. `make musl` needs the musl-gcc wrapper installed.
musl:
	$(MAKE) CC=musl-gcc

static:
	$(MAKE) STATIC=1

clean:
	rm -f $(OBJ) $(BIN)

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
