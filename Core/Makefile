CC      = gcc
AR      = ar
ARFLAGS = rcs

SRCS = bios.c iop.c ioptimer.c mkhebios.c psx.c r3000.c r3000asm.c r3000dis.c spu.c spucore.c vfs.c
OBJS_DEBUG   = $(SRCS:%.c=Debug/%.o)
OBJS_RELEASE = $(SRCS:%.c=Release/%.o)

TARGET_DEBUG   = Debug/PSXCore.a
TARGET_RELEASE = Release/PSXCore.a

DEFINES_COMMON = -DEMU_COMPILE -DEMU_LITTLE_ENDIAN -D_LIB
CFLAGS_COMMON  = -Wall -m32 -ffast-math -std=gnu99

CFLAGS_DEBUG   = $(CFLAGS_COMMON) $(DEFINES_COMMON) -D_DEBUG -O0 -g \
                 -fno-inline

CFLAGS_RELEASE = $(CFLAGS_COMMON) $(DEFINES_COMMON) -DNDEBUG -O3 \
                 -finline-functions -fomit-frame-pointer \
                 -ffunction-sections -mno-sse2 \
                 -ffast-math

.PHONY: all debug release clean

all: release

debug: $(TARGET_DEBUG)

release: $(TARGET_RELEASE)

Debug/%.o: %.c | Debug
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

Release/%.o: %.c | Release
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

$(TARGET_DEBUG): $(OBJS_DEBUG)
	$(AR) $(ARFLAGS) $@ $^

$(TARGET_RELEASE): $(OBJS_RELEASE)
	$(AR) $(ARFLAGS) $@ $^

Debug Release:
	mkdir -p $@

clean:
	rm -rf Debug Release
