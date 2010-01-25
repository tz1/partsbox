;;; copyright 2009, all rights reserved. released under the terms of the GNU
;;; general public license V3.  See www.gnu.org for details.  For other terms,
;;; contact tz@execpc.com

;;; USI I2C for Atmel 25 and larger chips.

;;; Features: Uniform frequency (9 cycles high, 14 cycles low) for SCK except
;;; for STOP condition where it is 15 cycles low.

;;; Input: Pointer to unsigned character buffer (in R24:25).
;;; Returns any read data into same buffer.
;;; Buffer format: One or more of: { LEN ADDR-RW BYTES }, followed by zero
;;; LEN is the number of bytes for the transfer, the address-r/w plus any
;;; bytes read or written.  A start condition is generated, the address is
;;; sent, then bytes are read or written.  If there is another transfer, a
;;; repeated start is generated, and the second transfer occurs.  Otherwise
;;; a stop condition is generated.

;;; The USI on the ATtinyx5 uses PB0 and PB2 which are properly set upon start.

;;; Constants
        
ZERO = 1
PORTB = 0x18
DDRB = 0x17
PINB = 0x16
USIDR = 0x0f
USISR = 0x0e
USICR = 0x0d

;;; Macros

;;; Waitup - pause until the SCL line is high. Required for normal clocking and STOP
        .macro  WaitUp
1:      sbis PINB,2
        rjmp 1b
        .endm

;;; PulseSCL - generate the high-time for SCL during data bit transfers.  Start and
;;; Stop are handled separately.  Timing adjusted for minimum of 6 cycles, but normal
;;; pullups will do 9 cycles since the test to wait for the line to actually go high
;;; is just after the line is released so reads as 0 at the instant.
        
;;; r21 contains 43 (0x2b)
        .macro PulseSCL
 lpm
        out USICR,r21           ;Toggle SCL High
        WaitUp
        lpm                     ;3 cycle delay
        out USICR,r21           ;Toggle SCL Low
        .endm

;;; XFER8 - transfer 8 data bits using USISR
        
;;; r22 has 0xf0 for count
        .macro Xfer8        
        out USISR,r22
8:      PulseSCL                ;SCL High pulse
        sbic USISR,6            ;Exit xfer8 on interrupt
        rjmp 9f                 ; jump to exit
        rcall .dly7             ; 7 cycle delay
        rjmp 6f                 ; 2 cycle delay - +1 to make it the uniform 14.
6:      rjmp 8b                 ; loop for next bit
9:                              ;3 cycles after SCL low
        .endm

;;; Function USIxfer Body.
        .text
.global USIxfer
        .type   USIxfer, @function
USIxfer:
        mov r26,r24             ;Pointer to transaction to X
;;; r19 will hold PORTB status for efficiency.  This and later sections of the
;;; code will need to be changed to do a read-modify-write with interrupts
;;; disabled if an interrupt can alter PORTB elsewhere.  Instead of redoing RMW
;;; r19 will be written to PORTB.to set the pins high
        in r19,PORTB            ;alt: sbi PORTB,0...2
        ori r19,lo8(5)
        out PORTB,r19
;;; set the SDA/SCL to input.
        in r24,DDRB
        ori r24,lo8(5)
        out DDRB,r24
;;; Setup the USI registers and local values used later
        ldi r23,lo8(-1)
        out USIDR,r23
        ldi r21,lo8(42)
        out USICR,r21           ;enable USI
        inc r21                 ;change into toggle command
        ldi r22,lo8(-16)
        out USISR,r22
        rjmp .MainBot

;;; X (r26/27) - pointer to message buffer
;;; r25 - current msg length left
;;; r24 scratchpad
;;; r23 const 0xff
;;; r22 const 0xf0 for 8 bits
;;; r21 const 0x2b to toggle SCL
;;; r20 - read/write flasg (break write loop after 1)
;;; r19 - incoming both high PORTB image
.MainTop:
        out PORTB,r19           ;Set both SDA and SCL high
.sdaupwait:
        sbis PINB,2
        rjmp .sdaupwait         ;wait until SDA is actually high (periph wait)
        ld r20,X                ;save message first byte for read/write bit
        cbi PORTB,0             ;set SDA low for start condition
        lpm                     ;delay 3 cycles
        rjmp 5f                 ;delay 2 cycles
5:      
        cbi PORTB,2             ;*** set SCL Low
        out USIDR,ZERO          ;this prevents a cosmetic glitch on the SDA line
        sbi PORTB,0             ;set SDA high so USI can use it
        lpm                     ;delay 3 cycles
        nop                     ;delay 1 for uniform 14 cycle lowtime
        rjmp .WriteTop          ;Enter write loop
;;; If the address or any byte is NAKed, the current transaction will end.
.nacked:
        add r26,r25             ;skip bytes to next length
        adc r27,ZERO            ; high byte of word
        rjmp .PastRead          ; end this transaction
;;; WRITE LOOP
.WriteTop:
        ld r24,X+               ;Get (next) message byte
        out USIDR,r24           ;send to USI
        Xfer8                   ; send it
        cbi DDRB,0              ;reverse SDA direction
        rcall .dly7             ;delay 7 cycles
        nop                     ;one more cycle for 14 uniform
        PulseSCL                ;Get the Ack or Nak - SCL high pulse
        dec r25                 ;len--
        breq .PastRead          ; no more chars, end
;;; SDA back to out, but if we switch to read we clear it?  Maybe optimize
        sbi DDRB,0              ;return to output
        sbic USIDR,0            ;Did we get a NAK?
        rjmp .nacked            ; yes, skip rest of this transaction
;;; This causes the longest nomal path at 14 cycles.
;;; For faster, e.g. at 8Mhz RC, it would be faster to do a split out address write,
;;; then jump to the write or read loop instead of checking this bit each loop
        sbrs r20,0              ;check read/write bit
        rjmp .WriteTop          ;Back to top
        nop                     ;delay for uniform 14
;;; Read Loop
.ReadTop:
        cbi DDRB,0              ;switch to input on SDA
        nop                     ;delay for uniform 14
        Xfer8                   ; get data byte
        in r24,USIDR            ;read data byte
        st X+,r24               ;save to buffer
        out USIDR,r23           ;set output high (-1) in case of NAK
        sbi DDRB,0              ;set SDA to output
        dec r25                 ;len--
        breq .doNAK             ;NAK of last byte - note optimize out tst r25 lower
        out USIDR,ZERO          ;more data, Ack - drive SDA low
.doNAK:
        nop                     ;delay for uniform 14 
        PulseSCL                ;Pulse the SCL line high to send the (n)ack
        tst r25                 ;retest length
        breq .PastRead          ;exit loop
        lpm                     ;delay 3
        rjmp 9f                 ;delay 2
9:      rjmp .ReadTop           ;continue read at next byte

.PastRead:                      ;Read (or write) complete
        out USIDR,r23           ;set output high if USI active
        sbi DDRB,0              ;set SDA to output
.MainBot:
        ld r25,X+               ;get length of (next) transaction
        tst r25                 ;check for 0/end
        breq .doStop            ;skip to stop conditon
        nop                     ;delay for uniform 14
        rjmp .MainTop           ;do next transaction
.doStop:
        cbi PORTB,0             ;clear SDA for STOP
        sbi PORTB,2             ;bring SCL high
        WaitUp                  ;wait unit it really does
        lpm                     ;delay 3 cycles
        sbi PORTB,0             ;Stop: Bring SDA high after 0.6
;;;Need to hold SDA for 13 before going low again - ret - call - setup will be longer
.dly7:  ret                     ;call to ret is 7 cycles - space efficient delay
        .size   USIxfer, .-USIxfer
