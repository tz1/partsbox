all: pulserial.hex

clean:
	rm -f *.elf *.hex *.eep *.lss *.sym

CPU=atmega1280
PORT=/dev/ttyUSB0

.SUFFIXES: .elf .hex .eep .lss .sym .upload

.c.elf:
	avr-gcc -O -Wall -mmcu=$(CPU) $^ -o $@

.elf.hex:
	avr-objcopy -O ihex -R .eeprom $^ $@

.elf.eep:
	avr-objcopy -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 --no-change-warnings -O ihex $^ $@

.hex.upload: 
	avrdude -F -p $(CPU) -P $(PORT) -c stk500v1 -b 57600 -U $^
