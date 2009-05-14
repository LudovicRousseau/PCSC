PCSC_CFLAGS ?= $(shell pkg-config libpcsclite --cflags)
PCSC_LDLIBS ?= $(shell pkg-config libpcsclite --libs)

CFLAGS := -Wall -O2 -g -DVERSION=\"$(VERSION)\" $(PCSC_CFLAGS)
LDLIBS := $(PCSC_LDLIBS)

all: SCardBeginTransaction \
	BufferOverflow
