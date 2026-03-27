CC=clang
ARCH_FLAGS = --target=aarch64-linux-gnu
CFLAGS = -Wall -Wextra -g $(ARCH_FLAGS)
LDFLAGS = -static $(ARCH_FLAGS)

TARGET=pwn_hyp_with_its
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

DEVMEM_TARGET=mem-poke.ko

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
		$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
		$(CC) $(CFLAGS) -c $< -o $@

clean:
		rm -f $(OBJS) $(TARGET)

run:
	adb root
	adb push $(DEVMEM_TARGET) /data/local/tmp
	adb shell 'if [ ! -e /dev/mem_poke ]; then insmod /data/local/tmp/$(DEVMEM_TARGET); fi'
	adb push $(TARGET) /data/local/tmp
	adb shell 'chmod +x /data/local/tmp/$(TARGET)'
#	adb shell /data/local/tmp/$(TARGET)