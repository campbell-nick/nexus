CC ?= cc
# -Wall -Wextra catch real issues; the two -Wno- flags silence GCC's
# truncation warnings, which are false positives here: every copy is either a
# deliberately bounded snprintf or a strncpy that is null-terminated right
# after (long system strings are meant to be clipped).
CFLAGS ?= -O2 -Wall -Wextra -Wno-stringop-truncation -Wno-format-truncation
LDLIBS ?= -lm

BIN := nexus
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

PREFIX ?= /usr/local

.PHONY: all clean install

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

src/%.o: src/%.c src/nexus.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(BIN)

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
