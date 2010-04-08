OS := $(shell uname -s)
ifeq ("$(OS)", "Darwin")
	PCSC_CFLAGS := -framework PCSC
else
	PCSC_CFLAGS ?= $(shell pkg-config libpcsclite --cflags)
	PCSC_LDLIBS ?= $(shell pkg-config libpcsclite --libs)
endif

CFLAGS := -Wall -O2 -g -DVERSION=\"$(VERSION)\" $(PCSC_CFLAGS)
LDLIBS := $(PCSC_LDLIBS)

PROGRAMS := SCardBeginTransaction \
	BufferOverflow

all: $(PROGRAMS)

clean:
	rm -f $(PROGRAMS)
