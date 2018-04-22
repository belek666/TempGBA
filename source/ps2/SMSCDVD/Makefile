IOP_SRC_DIR  = ./
IOP_OBJS_DIR = ./

IOP_BIN  = SMSCDVD.irx
IOP_OBJS = SMSCDVD.o imports.o

IOP_LDFLAGS = -s
IOP_CFLAGS  += -I./ -Wall -Wsign-compare -fno-builtin -D_IOP

all: $(IOP_BIN)

clean:
	rm -f *.elf *.o *.a *.irx

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.iopglobal
include $(PS2SDK)/samples/Makefile.pref
