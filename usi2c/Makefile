AVR=attiny85
AVRDUDE = avrdude -p $(AVR)
CC = avr-gcc -Wall -mmcu=$(AVR) -Os -g

.SUFFIXES: .elf .hex .c .s .S .o .l

all:	test.hex

.c.o:
	$(CC) -c $< -o $@

.S.o:
	$(CC) -c $< -o $@

.c.s:
	$(CC) -S $< -o $@

.elf.hex:
	avr-objcopy -j .text -j .data -O ihex $< $@

.S.l:
	avr-as -adhlms=$@ -o /dev/null $< -mmcu=$(AVR)

upload:	all
	$(AVRDUDE) -U flash:w:test.hex



xtalfuse:
	$(AVRDUDE) -U hfuse:w:0xd7:m -U lfuse:w:0xed:m

noxtalfuse:
	$(AVRDUDE) -U hfuse:w:0xd7:m -U lfuse:w:0xe2:m

dwfuse:
	$(AVRDUDE) -U hfuse:w:0x17:m -U lfuse:w:0xe2:m

clean:
	rm -f *.hex *.elf *.o *.s *.l

test.elf: test.o sendchar.o usi2c.o
	$(CC) -o test.elf $^

#avr-ld -mavr4 -Tnomain.scr usi2c.o -o usi2c.elf
