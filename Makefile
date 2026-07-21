CC ?= cc
VERSION := $(shell sed -n '1p' VERSION)
ifeq ($(strip $(VERSION)),)
$(error Le fichier VERSION est absent ou vide)
endif

CPPFLAGS ?=
CPPFLAGS += -Iinclude -DDFX_VERSION=\"$(VERSION)\"
ifeq ($(LAB_MODE),1)
CPPFLAGS += -DDFX_ENABLE_LAB_MODE=1
TEST_VERSION := $(VERSION)-lab
else
TEST_VERSION := $(VERSION)
endif
CFLAGS ?= -O2
CFLAGS += -std=c17 -Wall -Wextra -Wpedantic -Werror
OBJ = build/main.o build/audit.o build/device.o build/erase.o build/purge.o build/platform.o build/qualification.o build/runtime.o build/sha256.o
HEADERS = include/dfx.h include/dfx_sha256.h src/audit_internal.h src/erase_internal.h src/purge_internal.h

ifeq ($(shell uname -s),Darwin)
LDLIBS += -framework IOKit -framework CoreFoundation
endif
ifeq ($(OS),Windows_NT)
LDLIBS += -lbcrypt
endif

.PHONY: all clean test

all: build/diskpurge

build/diskpurge: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) $(LDLIBS) -o $@

build/%.o: src/%.c $(HEADERS)
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/tests/%.o: tests/%.c $(HEADERS)
	mkdir -p build/tests
	$(CC) $(CPPFLAGS) -Isrc $(CFLAGS) -c $< -o $@

build/test-sha256: build/tests/test_sha256.o build/sha256.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

build/test-purge: build/tests/test_purge.o build/purge.o build/runtime.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

build/test-safety: build/tests/test_safety.o build/device.o build/purge.o build/qualification.o build/runtime.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

build/tests/qualification.o: src/qualification.c $(HEADERS)
	mkdir -p build/tests
	$(CC) $(CPPFLAGS) -DDFX_TEST_QUALIFICATION=1 $(CFLAGS) -c $< -o $@

build/test-qualification: build/tests/test_qualification.o build/tests/qualification.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

build/test-audit: build/tests/test_audit.o build/audit.o build/device.o build/purge.o build/qualification.o build/runtime.o build/sha256.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

build/test-erase: build/tests/test_erase.o build/erase.o build/device.o build/purge.o build/platform.o build/qualification.o build/runtime.o build/sha256.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

test: build/diskpurge build/test-sha256 build/test-purge build/test-safety build/test-qualification build/test-audit build/test-erase
	./tests/test_cli.sh ./build/diskpurge $(TEST_VERSION)
	./build/test-sha256
	./build/test-purge
	./build/test-safety
	./build/test-qualification
	./build/test-audit
	./build/test-erase

clean:
	rm -rf build
