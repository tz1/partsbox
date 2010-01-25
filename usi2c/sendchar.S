	.file	"sendchar.S"
PORTB = 0x18
	.text
        .global	sendchar
	.type	sendchar, @function
sendchar:
        in r23,PORTB
        andi r23,0xfd // Zeroing mask - clear port bit
        out PORTB,r23 // start bit
        sec
        .rept 9
        mov r22,r23
        sbrc r24,0
        ori r22,0x02 // Marking mask - set port bit
        out PORTB,r22
        ror r24
        .endr
        ret
	.size	sendchar, .-sendchar
