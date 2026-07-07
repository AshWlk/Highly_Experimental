CC      = gcc
AR      = ar
ARFLAGS = rcs

SRCS = bios.c iop.c ioptimer.c mkhebios.c psx.c r3000.c r3000asm.c r3000dis.c spu.c spucore.c vfs.c
OBJS_DEBUG   = $(SRCS:%.c=Debug/%.o)
OBJS_RELEASE = $(SRCS:%.c=Release/%.o)

TARGET_DEBUG_STATIC  = Debug/PSXCore.a
TARGET_RELEASE_STATIC = Release/PSXCore.a
TARGET_DEBUG_SHARED  = Debug/PSXCore.so
TARGET_RELEASE_SHARED = Release/PSXCore.so

DEFINES_COMMON = -DEMU_COMPILE -DEMU_LITTLE_ENDIAN -D_LIB
CFLAGS_COMMON  = -Wall -ffast-math -std=gnu99 -fPIC

CFLAGS_DEBUG   = $(CFLAGS_COMMON) $(DEFINES_COMMON) -D_DEBUG -O0 -g \
                 -fno-inline

CFLAGS_RELEASE = $(CFLAGS_COMMON) $(DEFINES_COMMON) -DNDEBUG -O3 \
                 -finline-functions -fomit-frame-pointer \
                 -ffunction-sections \
                 -ffast-math

LDFLAGS = -shared

.PHONY: all debug release clean

all: release

debug: $(TARGET_DEBUG_STATIC) $(TARGET_DEBUG_SHARED)

release: $(TARGET_RELEASE_STATIC) $(TARGET_RELEASE_SHARED)

Debug/%.o: %.c | Debug
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

Release/%.o: %.c | Release
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

$(TARGET_DEBUG_STATIC): $(OBJS_DEBUG)
	$(AR) $(ARFLAGS) $@ $^

$(TARGET_RELEASE_STATIC): $(OBJS_RELEASE)
	$(AR) $(ARFLAGS) $@ $^

$(TARGET_DEBUG_SHARED): $(OBJS_DEBUG)
	$(CC) $(LDFLAGS) -o $@ $^

$(TARGET_RELEASE_SHARED): $(OBJS_RELEASE)
	$(CC) $(LDFLAGS) -o $@ $^

Debug Release:
	mkdir -p $@

clean:
	rm -rf Debug Release
