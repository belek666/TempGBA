
EE_BIN = TempGBA.elf

EE_CFLAGS += -g -DzHOST -DMIPS_XBURST -DLOAD_ALL_ROM -DCHEATS -DGIT_VERSION=$(shell git describe --always)
EE_INCS += -I./ -I$(PS2SDK)/ports/include -I$(PS2DEV)/gskit/include -I../ -I../mips
EE_LDFLAGS += -L$(PS2SDK)/ports/lib -L$(PS2DEV)/gskit/lib
EE_LIBS += -lgskit -ldmakit -laudsrv -lpad -lz -lps2time -lcdvd \
		   -lm -lpatches -lhdd -lfileXio -lpoweroff -ldebug

EE_CFLAGS += -Wno-unused-variable

COMMON_OBJS = ../cpu.o ../input.o ../memory.o ../sound.o \
			  ../stats.o ../unifont.o ../video.o ../zip.o ../mips/stub.o
		   
EE_OBJS = $(COMMON_OBJS) draw.o main.o ps2input.o ps2memory.o \
		ps2sound.o port.o port-asm.o ps2.o gui.o settings.o cheats.o charsets.o \
		scalers/2xSaI.o scalers/scale2x.o scalers/scale3x.o scalers/scaler.o \
		iomanX_irx.o usbhdfsd_irx.o usbd_irx.o freesd_irx.o audsrv_irx.o \
		poweroff_irx.o fileXio_irx.o ps2atad_irx.o ps2fs_irx.o ps2hdd_irx.o ps2dev9_irx.o \
		smscdvd_irx.o gba_bios_bin.o
		
all: $(EE_BIN)

gba_bios_bin.o: ../../bios/gba_bios.bin
	bin2s $^ gba_bios_bin.s gba_bios_bin
	$(EE_AS) $(EE_ASFLAGS) gba_bios_bin.s -o $@
smscdvd_irx.o: SMSCDVD/smscdvd.irx
	bin2s $^ smscdvd_irx.s smscdvd_irx
	$(EE_AS) $(EE_ASFLAGS) smscdvd_irx.s -o $@
%_irx.o: $(PS2SDK)/iop/irx/%.irx
	bin2s $^ $*_irx.s $*_irx
	$(EE_AS) $(EE_ASFLAGS) $*_irx.s -o $@
SMSCDVD/smscdvd.irx:
	make -C SMSCDVD
run:
	ps2client -h 192.168.0.10 execee host:$(EE_BIN)
reset:
	ps2client -h 192.168.0.10 reset
pack:
	ps2-packer $(EE_BIN) packed_$(EE_BIN)
strip:
	$(EE_STRIP) $(EE_BIN)
tests:
	$(EE_PREFIX)addr2line -e $(EE_BIN) 001B2870
clean:
	rm -f $(EE_BIN) $(EE_OBJS) *.s
	make -C SMSCDVD clean

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
