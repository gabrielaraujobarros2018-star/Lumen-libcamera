# libcamera.c 1.0 - COMPLETE WILDCARD MAKEFILE
# Explicit targets using % wildcards - NO missing rules

# Compiler setup
CC = gcc
CFLAGS = -Wall -O3 -fPIC -shared -march=armv7-a -mfpu=neon -mfloat-abi=softfp
LDFLAGS = -shared

# Explicit wildcard patterns
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
TARGETS := libcamera.so libcamera.a libcamera_test

# Primary target
all: libcamera.so
\t@echo " libcamera.so BUILT (74KB expected)"
\t@ls -lh libcamera.so

# Explicit shared library rule using wildcard
libcamera.so: libcamera.c
\t$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# Explicit static library rule  
libcamera.a: libcamera.c
\t$(CC) $(CFLAGS) -c -o libcamera.o $<
\tar rcs $@ libcamera.o
\trm -f libcamera.o

# Explicit test program rule
libcamera_test: libcamera_test.c libcamera.so
\t$(CC) $(CFLAGS) -I. -o $@ libcamera_test.c -L. -l:libcamera.so

# Generic wildcard rules for ANY .c file
%.o: %.c
\t$(CC) $(CFLAGS) -c -o $@ $<

%.so: %.c
\t$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# Explicit phony targets
.PHONY: all clean test size install debug arm native

# Test target
test: libcamera_test
\t@echo " Test program ready"

# Size analysis (explicit dependency)
size: libcamera.so
\tobjdump -h libcamera.so | grep -E ".(text|data|bss)"
\treadelf -s libcamera.so | grep FUNC | tail -5 || true

# Debug build (explicit flags override)
debug:
\t$(MAKE) CFLAGS="-Wall -g3 -O0 -fPIC -march=armv7-a -mfpu=neon -mfloat-abi=softfp"

# ARM cross-compile
arm:
\t$(MAKE) CC="arm-none-eabi-gcc"

# Native build
native:
\t$(MAKE) CFLAGS="-Wall -O3 -fPIC"

# Install
install: libcamera.so
\tmkdir -p dist/system/lib
\tcp libcamera.so dist/system/lib/
\t@echo " Installed to dist/system/lib/"

# Clean ALL explicit targets
clean:
\trm -f libcamera.so libcamera.a libcamera_test *.o
\trm -rf dist/
\t@echo " Cleaned"

# Help
help:
\t@echo "make         # libcamera.so (default)"
\t@echo "make clean   # Clean all"
\t@echo "make test    # Test program"
\t@echo "make size    # Size analysis" 
\t@echo "make debug   # Debug build"
\t@echo "make arm     # ARM cross-compile"
\t@echo "make install # Deploy"

# Default target
.DEFAULT_GOAL := all