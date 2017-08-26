vpath %.h = inc/
vpath %.c = src/

SRC += $(wildcard *.c)
SRC += $(wildcard src/*.c)

CC = arm-none-linux-gnueabi-gcc

CPPFLAGS += -I ./inc
CPPFLAGS += -I ./jpeglib/include

LDFLAGS += -L ./jpeglib/lib
LDFLAGS += -ljpeg

main: $(SRC)
	$(CC) $^ -o $@ $(CPPFLAGS) $(LDFLAGS)

.PHONY clean:
clean:
	$(RM) main
