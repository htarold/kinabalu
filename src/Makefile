# Simple makefile.

# Device specific info
DEVICE_FULL=stm32f051c8t6
DEVICE_SHORTER=stm32f051
DEVICE_SHORTEST=stm32f0
MDEV=-mcpu=cortex-m0 -mthumb -msoft-float

# Paths
PREFIX=arm-none-eabi-
OPENCM3=/home/harold/build/libopencm3
OPENCM3_LIB=$(OPENCM3)/lib
OPENCM3_INC=$(OPENCM3)/include

# Tools
CC=$(PREFIX)gcc
LD=$(PREFIX)ld
NM=$(PREFIX)nm
OBJCOPY=$(PREFIX)objcopy
SIZE=$(PREFIX)size
#stdir=/home/harold/.arduino15/packages/STM32/tools/STM32Tools/1.1.0/tools/linux64/stlink
#STFLASH=LD_LIBRARY_PATH=$(stdir)/lib $(stdir)/st-flash
STFLASH=/home/harold/build/stlink-master/build/Release/st-flash
STM32FLASH=stm32flash

DFLAGS+=-D$(shell echo $(DEVICE_FULL) |tr '[a-z]' '[A-Z]')
DFLAGS+=-D$(shell echo $(DEVICE_SHORTER) |tr '[a-z]' '[A-Z]')
DFLAGS+=-D$(shell echo $(DEVICE_SHORTEST) |tr '[a-z]' '[A-Z]')
# Depending on which crystal installed, either 44100 or 48000 or?
DFLAGS+=-DWAV_SPS=44100
DFLAGS+=$(EXTRA_DFLAGS)
IFLAGS+=-I$(OPENCM3_INC)
LIBS=--static -nostartfiles -Tlinkscript.ld -L$(OPENCM3_LIB) \
-lopencm3_$(DEVICE_SHORTEST) -Wl,--start-group -lc -lgcc -lnosys \
-Wl,--end-group -Wl,-Map=map -Wl,--cref -Wl,--gc-sections

CFLAGS=-O -std=c99 $(MDEV) -Wall -Wundef -Wstrict-prototypes \
-fno-common \
-ffunction-sections -fdata-sections -MD -ggdb3

test-stm32flash:
	echo $(STM32FLASH)

# Important to use these flags, due to execution timing
isr.o:isr.c
	$(CC) $(DFLAGS) -O $(MDEV) -Wall -MD -ggdb3 $(IFLAGS) -c $< -o $@
%.list:%.c
	$(CC) $(DFLAGS) $(CFLAGS) $(IFLAGS) -E $<
%.objdump:%.o
	$(PREFIX)objdump -dS $< >$@
%.o:%.c
	$(CC) $(DFLAGS) $(CFLAGS) $(IFLAGS) -c $< -o $@
%.dep:%.c
	$(CC) $(DFLAGS) $(IFLAGS) -MM $< |grep -v '/'
%.clean:
	rm -f $*.o $*.d $*.bin $*.elf $*.map map
clean:
	rm -f *.o *.d *.bin *.elf *.map map
%.bin:%.elf
	$(OBJCOPY) -Obinary $< $@
%.stflash:%.bin
	$(STFLASH) write $*.bin 0x8000000
%.stm32flash:%.bin
	$(STM32FLASH) -w $< /dev/ttyUSB0
%.read:
	$(STFLASH) read $@ 0x8000000 8192
%.erase:
	$(STFLASH) erase

linkscript.ld: $(OPENCM3)/ld/linker.ld.S $(OPENCM3)/scripts/genlink.py \
$(OPENCM3)/ld/devices.data
	$(CC) $(shell $(OPENCM3)/scripts/genlink.py $(OPENCM3)/ld/devices.data $(DEVICE_FULL) DEFS) -E $< |grep -v '^#' >$@

all-tests: test-usart.elf test-sd-spi.elf \
test-multi2.elf test-i2s.elf \
test-pcm1808.elf \
test-i2c.elf test-blink.elf test-wkup.elf \
test-int.elf test-cfg.elf \
test-tick.elf test-fil.elf test-clock.elf \
test-board.elf test-nomulti.elf test-singlewrite.elf test-fat.elf \
test-bosch.elf kinabalu.elf

test-usart.elf:test-usart.o fmt.o tx.o usart_setup.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@

test-sd-spi.elf: test-sd-spi.o tx.o fmt.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@

test-multi2.elf:test-multi2.o tx.o fmt.o sd2.o fil.o usart_setup.o \
sd-arch.o wav.o cfg.o rtc.o ds3231.o i2c2.o rtc_i2c.o tick.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@

test-i2s.elf:test-i2s.o tx.o fmt.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@

test-pcm1808.elf:test-pcm1808.o pcm1808.o fmt.o tx.o usart_setup.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@

test-i2c.elf:test-i2c.o fmt.o tx.o usart_setup.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@
test-blink.elf:test-blink.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@

test-wkup.elf:test-wkup.o wkup.o rtc.o fmt.o tx.o usart_setup.o power.o \
rtc_i2c.o i2c2.o ds3231.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@

test-int.elf: test-int.o tx.o fmt.o usart_setup.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@

test-cfg.elf: test-cfg.o fmt.o tx.o usart_setup.o power.o sd2.o sd-arch.o \
fil.o rtc.o cfg.o cfg_parse.o wkup.o tick.o ds3231.o i2c2.o rtc_i2c.o attn.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@

test-tick.elf: test-tick.o fmt.o tx.o usart_setup.o tick.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@

	$(CC) $(MDEV) $^ $(LIBS) -o $@
kinabalu.elf: test-master.o fmt.o tx.o usart_setup.o power.o sd2.o \
sd-arch.o fil.o rtc.o cfg.o cfg_parse.o wkup.o tick.o pcm1808.o wav.o \
vdda.o i2c2.o bosch.o attn.o rtc_i2c.o ds3231.o rtc.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@
test-fil.elf: test-fil.o fmt.o tx.o usart_setup.o power.o fil.o \
sd2.o sd-arch.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@

test-clock.elf:test-clock.o power.o tx.o fmt.o rtc.o usart_setup.o \
ds3231.o i2c2.o rtc_i2c.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@
test-board.elf:test-board.o usart_setup.o tx.o fmt.o fil.o sd2.o \
sd-arch.o attn.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@
logger.elf:logger.o usart_setup.o tx.o fmt.o sd2.o fil.o sd-arch.o \
power.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@
test-nomulti.elf: test-nomulti.o fmt.o tx.o usart_setup.o power.o sd2.c \
sd-arch.o fil.o cfg.o cfg_parse.o wkup.o tick.o pcm1808.o wav.o \
vdda.o i2c2.o attn.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@
test-nomulti.wrap:
	EXTRA_DFLAGS=-DSIMULATE_MULTI make -B test-nomulti.elf
test-singlewrite.elf: test-singlewrite.o fmt.o tx.o usart_setup.o power.o \
sd2.o sd-arch.o fil.o cfg.o cfg_parse.o wkup.o tick.o pcm1808.o wav.o \
vdda.o i2c2.o attn.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@
test-singlewrite.wrap:
	EXTRA_DFLAGS=-DUSE_SINGLE_BLOCK_WRITE make -B test-singlewrite.elf
test-fat.elf:test-fat.o tx.o fmt.o fil.o usart_setup.o power.o \
sd2.o sd-arch.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@
test-fat.wrap:
	EXTRA_DFLAGS='-DSD_READONLY -DDBG_FIL' make -B test-fat.elf
test-bosch.elf:test-bosch.o tx.o fmt.o usart_setup.o \
power.o i2c2.o bosch.o rtc_i2c.o
	$(CC) $(MDEV) $^ $(LIBS) -o $@
un36:un36.c
	gcc un36.c -o un36
