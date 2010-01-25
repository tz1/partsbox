#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#define TXBUFLEN (1024)
static unsigned char txbuf[TXBUFLEN];
static unsigned txhead;
static unsigned txtail;

ISR(USART0_UDRE_vect)
{
    if (txhead == txtail)
        UCSR0B &= ~0x20;
    else {
        UDR0 = txbuf[txtail++];
        txtail &= TXBUFLEN - 1;
    }
}

void txenqueue(unsigned char *dataOut, unsigned length)
{
    while (length--) {
        txbuf[txhead++] = *dataOut++;
        txhead &= TXBUFLEN - 1;
    }
    UCSR0B |= 0x20;
}

unsigned char hobuf[20];
unsigned long ltens[12];
static void widthout(unsigned char polarity, unsigned long width)
{
    unsigned char *h = hobuf, i, j;
    if (polarity != 0xff)
        *h++ = polarity & 1 ? '4' : '5';
    else {
        *h++ = '\r';
        *h++ = '\n';
        *h++ = 'T';
    }
    *h++ = polarity & 0xc0 ? '/' : '\\';
    i = 1;
    while (ltens[i] < width)
        i++;
    while (i--) {
        j = '0';
        while (width >= ltens[i]) {
            width -= ltens[i];
            j++;
        }
        *h++ = j;
    }

    if (1 || polarity & 0xc0) {
        *h++ = '\r';
        *h++ = '\n';
    }

    txenqueue(hobuf, h - hobuf);
}

static unsigned long hitime4, hitime5, lastedge4, lastedge5;
unsigned char lastpref = 0;
unsigned char dispmode = 'H';
static unsigned long lastmsg = 0;
static void pfxhex(unsigned char prefix, unsigned char val)
{
    unsigned char hobuf[8], *h = hobuf;
    unsigned char i;

    if (prefix != lastpref) {
        unsigned long thismsg = hitime4 + TCNT4;
        widthout(0xff, (thismsg - lastmsg) >> 1);
        lastmsg = thismsg;

        *h++ = prefix;
        lastpref = prefix;
    }
    if (dispmode == 'H') {
        i = val >> 4;
        i &= 15;
        *h++ = i > 9 ? 'A' - 10 + i : '0' + i;
        i = val & 15;
        *h++ = i > 9 ? 'A' - 10 + i : '0' + i;
    } else {
        i = val;
        if (i < ' ' || i > 0x7e)
            i = '.';
        *h++ = i;
    }
    txenqueue(hobuf, h - hobuf);
}

ISR(USART1_RX_vect)
{
    pfxhex('x', UDR1);
}

ISR(USART2_RX_vect)
{
    pfxhex('y', UDR2);
}

ISR(USART3_RX_vect)
{
    pfxhex('z', UDR3);
}

static unsigned char toggle4 = 1, toggle5 = 1;

unsigned divisors[10] = {
    20000 / 1152 - 1,
    20000 / 12 - 1,
    20000 / 24 - 1,
    20000 / 384 - 1,
    20000 / 48 - 1,
    20000 / 576 - 1,
    20000 / 6 - 1,
    20000 / 288 - 1,
    20000 / 192 - 1,
    20000 / 96 - 1
};

char usage[] = "\r\nh-text, H-hex"
  "\r\nX-enable x-disable serial 1, YyZz for 2,3"
  "\r\nbaud digit sets on all DISABLED serials"
  "\r\n[1]200 [2]400 [3]8400 [4]800 [5]7600 [6]00 [9]600"
  "\r\n[7]:28800 [8]:19200 [0]:115200 " "\r\nTimes are uS 1st char of msg to 1st of other msg" "\r\n";

ISR(USART0_RX_vect)
{
    unsigned char d = UDR0;
    unsigned long br;
    if (d >= '0' && d <= '9') {
        unsigned br = divisors[d - '0'];
        if (!(UCSR1B & 0x80))
            UBRR1 = br;
        if (!(UCSR2B & 0x80))
            UBRR2 = br;
        if (!(UCSR3B & 0x80))
            UBRR3 = br;
        return;
    }
    switch (d) {
    case '?':
        txenqueue((unsigned char *) usage, sizeof(usage));
        txenqueue((unsigned char *) ((UCSR1B & 0x80) ? "X" : "x"), 1);
        br = 2000000 / (UBRR1 + 1);
        widthout(0xff, br);
        txenqueue((unsigned char *) ((UCSR2B & 0x80) ? "Y" : "y"), 1);
        br = 2000000 / (UBRR2 + 1);
        widthout(0xff, br);
        txenqueue((unsigned char *) ((UCSR3B & 0x80) ? "Z" : "z"), 1);
        br = 2000000 / (UBRR3 + 1);
        widthout(0xff, br);
        return;
    case 'h':
        dispmode = 'h';
        return;
    case 'H':
        dispmode = 'H';
        return;
    case 'x':
        UCSR1B = 0x18;
        return;
    case 'X':
        UCSR1B = 0x98;
        return;
    case 'y':
        UCSR2B = 0x18;
        return;
    case 'Y':
        UCSR2B = 0x98;
        return;
    case 'z':
        UCSR3B = 0x18;
        return;
    case 'Z':
        UCSR3B = 0x98;
        return;
    }
}

static unsigned long hitime4, hitime5, lastedge4, lastedge5;
ISR(TIMER4_CAPT_vect)
{
    unsigned long counts;
    if (toggle4)
        TCCR4B ^= 0x40;
    counts = ICR4;
    counts += hitime4;
    widthout(1 | (TCCR4B & 0x40), (counts - lastedge4) >> 1);
    lastedge4 = counts;
}

ISR(TIMER5_CAPT_vect)
{
    unsigned long counts;
    if (toggle5)
        TCCR5B ^= 0x40;
    counts = ICR5;
    counts += hitime5;
    widthout(TCCR5B & 0x50, (counts - lastedge5) >> 1);
    lastedge5 = counts;
}

ISR(TIMER4_OVF_vect)
{
    hitime4 += 0x10000;
    hitime5 += 0x10000;
}

unsigned char xbuf[280];
int main(void)
{
    int i;
    unsigned long lt = 1;
    for (i = 0; i < 12; i++) {
        ltens[i] = lt;
        lt *= 10;
    }

    txhead = txtail = 0;
    hitime4 = hitime5 = 0;

    DDRB = 0x20;
    PORTB |= 0x20;
    // Startup

    GTCCR = 0x81;
    TCCR4B = 0xC0 | 2;          // NoiseC, edge ...  off,1,8,64,256,1024,ext
    TCCR5B = 0x80 | 2;          // NoiseC, edge ...  off,1,8,64,256,1024,ext
    // /8
    GTCCR = 0x80;

    // Crystal based: 921600 = 14745600 / 16
#define MAINBAUD 1000000
    UBRR0 = 2000000 / MAINBAUD - 1;

    UCSR0C = 0x06;              //8N1 (should be this from reset)
    UCSR0A = 0xE2;              // clear Interrupts, UART at 2x (xtal/8)
    UCSR0B = 0x98;              // oring in 0x80 would enable rx interrupt

    PORTB ^= 0x20;
    UDR0 = '[';
    while (!(UCSR0A & 0x40));
    PORTB ^= 0x20;
    UDR0 = ']';
    while (!(UCSR0A & 0x40));
    PORTB ^= 0x20;

    sei();

#define BAUD 57600
    UBRR1 = 2000000 / BAUD - 1;
    UBRR2 = 2000000 / BAUD - 1;
    UBRR3 = 2000000 / BAUD - 1;

    UCSR1C = 0x06;              //8N1 (should be this from reset)
    UCSR1A = 0xE2;              // clear Interrupts, UART at 2x (xtal/8)
    UCSR1B = 0x18;              // oring in 0x80 would enable rx interrupt

    UCSR2C = 0x06;              //8N1 (should be this from reset)
    UCSR2A = 0xE2;              // clear Interrupts, UART at 2x (xtal/8)
    UCSR2B = 0x18;              // oring in 0x80 would enable rx interrupt

    UCSR3C = 0x06;              //8N1 (should be this from reset)
    UCSR3A = 0xE2;              // clear Interrupts, UART at 2x (xtal/8)
    UCSR3B = 0x18;              // oring in 0x80 would enable rx interrupt

    TIFR4 |= 0x21;              // reset interrupt triggers
    TIFR5 |= 0x21;              // reset interrupt triggers
    TIMSK4 |= 0x21;             // enable interrupt
    TIMSK5 |= 0x20;             // enable interrupt

    for (;;)
        sleep_mode();

}
