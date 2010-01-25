#include <avr/sleep.h>
#include <avr/io.h>
#define F_CPU 8000000
#include <util/delay.h>

extern void USIxfer(unsigned char *msg);
//extern void sendchar(unsigned char c);

// len, len bytes of transaction, first even = wrinte, odd=read per i2c
// Start, len bytes, rep start, len bytes, 0 does stop
// 20 char
#if 1
#define LCDADDR 0x78 
unsigned char sendlcd0[] = {
    13,
    LCDADDR, 0x00, 0x38, 0x38, 0x38, 0x39,
    0x14, 0x78, 0x5e, 0x6d, 0x0c, 0x06, 0x01,
    0
};
#else
#define LCDADDR 0x7c
unsigned char sendlcd0[] = {
    13, LCDADDR, 0x00, 0x30, 0x30, 0x30, 0x39,
    0x14, 0x7f, 0x5f, 0x6a, 0x0f, 0x06, 0x01,
    0
};
#endif

unsigned char sendlcda[] = {
    7, LCDADDR, 0x80, 0x80, 0x40, 'O', 'n', 'e', '=', '=','=','=','=', '+','+','+','+', '=','=','=','=', '=','O','n','e',
    0,0,0,0,0,0,0,0
};

unsigned char sendlcdb[] = {
    7, LCDADDR, 0x80, 0xc0, 0x40, 'T', 'w', 'o', '=', '=','=','=','=', '+','+','+','+', '=','=','=','=', '=','T','w','o',
    0, 0,0,0,0,0,0,0
};

#define WADXL (0x3a)
#define RADXL (WADXL+1)
unsigned char msgsetup[] = {
    3, WADXL, 0x2d, 8,
    3, WADXL, 0x31, 0xb,
    3, WADXL, 0x2c, 0xa,//f,
    3, WADXL, 0x38, 0x9f,
    0
};

unsigned char queryfifo[] = {
    2, WADXL, 0x39,
    2, RADXL, 0,                //5
    0
};

unsigned char readxyz[] = {
    2, WADXL, 0x32,
    7, RADXL, 0, 0, 0, 0, 0, 0,
    0
};
#if 0
unsigned char sendadc[] = {
    3, 0xc0, 0, 0,
    0
};

unsigned char sendpxpsetout[] = {
    4, 0x40, 6, 0, 0,
    0
};
unsigned char sendpxpsetbits[] = {
    4, 0x40, 2, 0x55, 0x55,
    0
};
unsigned char lsdig[] = { 0x5f, 0x06, 0x3b, 0x2f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c, 0x59, 0x3e, 0x79, 0x71 };
unsigned char msdig[] = { 0xf6, 0xc0, 0x6e, 0xea, 0xd8, 0xba, 0xbe, 0xe0, 0xfe, 0xfa, 0xfc, 0x9e, 0x36, 0xce, 0x3e, 0x3c };
#endif

unsigned char readtherm[] = {
    3, 0x91, 0, 0,
    0,0,0,0
};

unsigned char settherm[] = {
    2, 0x90, 0,
    0
};

unsigned char sendhmci[] = {
    5, 0x3c, 0, 0x18, 0, 0,
    0
};

unsigned char sendhmcpr[] = {
    2, 0x3c, 9,
    0
};

unsigned char sendhmcrd[] = {
    8, 0x3d, 0, 0, 0, 0, 0, 0, 0,
    0,0,0,0
};

static unsigned tens[] = {1, 10, 100, 1000, 10000};
static void ndnumout(unsigned char x, unsigned char y, unsigned char w, unsigned val)
{
    unsigned t;
    x |= (y << 6);
    x |= 0x80;
    sendlcdb[3] = x;
    sendlcdb[0] = 4+w;
    char *c;
    c = &sendlcdb[5];
    y = w;
    while( y-- )
        *c++ = ' ';
    c = &sendlcdb[5];
    if( val > 32768 ) {
        *c++ = '-';
        val = ~val;
        val++;
    }
    t = 4;
    while( t && val < tens[t] )
        t--;
    do {
        *c = '0';
        while( val >= tens[t] ) {
            val -= tens[t];
            (*c)++;
        }
        c++;
    } while( t-- );
    USIxfer(sendlcdb);
    _delay_us(250);
}


int main()
{
unsigned j, k;

    DDRB = PORTB = 7;
    PORTB |= 0x20;
    // set the parameters

    _delay_ms(5); // warm up
    USIxfer(sendlcd0); // this will send 0xff followed by 0x7c which will be naked.  I don't know why.
    _delay_ms(10);
    USIxfer(sendlcd0);
    _delay_ms(10);
    //    USIxfer(sendlcda);
    _delay_ms(10);
        USIxfer(sendlcdb);
    _delay_ms(10);
    USIxfer(msgsetup);
    _delay_ms(200);

    USIxfer(sendhmci);
    //    USIxfer(sendpxpsetout);
    USIxfer(settherm);

    unsigned count;
    for (;;) {
        if( !(count++ & 3) )
        switch( (count>>2) & 3 ) {
        case 0:
            USIxfer(readtherm);
            j = (readtherm[2] << 8) + readtherm[3];
            j >>= 4; // 12 bits
            ndnumout( 1, 0, 3, j );
            break;
        case 1:
            k = readxyz[5] + (readxyz[6] << 8);
            ndnumout( 5, 0, 5, k );
            break;
        case 2:
            k = readxyz[7] + (readxyz[8] << 8);
            ndnumout( 10, 0, 5, k );
            break;
        case 3:
            k = readxyz[9] + (readxyz[10] << 8);
            ndnumout( 15, 0, 5, k );
            break;
        case 4:
            USIxfer(sendhmcpr);
            USIxfer(sendhmcrd);

            j = sendhmcrd[2];
            ndnumout( 1, 1, 1, j );
            break;
        case 5:

            k = sendhmcrd[3] + (sendhmcrd[4] << 8);
            k += 2000;
            ndnumout( 5, 1, 5, k );
            break;
        case 6:
            k = sendhmcrd[2 + 3] + (sendhmcrd[2 + 4] << 8);
            k += 2000;
            ndnumout( 10, 1, 5, k );
            break;
        case 7:
            k = sendhmcrd[4 + 3] + (sendhmcrd[4 + 4] << 8);
            k += 2000;
            ndnumout( 15, 1, 5, k );
            break;
        }
        // wait for data?
        USIxfer(queryfifo);
        unsigned char qlen = queryfifo[5];
        while (qlen--) {
            USIxfer(readxyz);
#if 0
            i = 40960 / 33;
            j = readxyz[4 + 3] + (readxyz[4 + 4] << 8);
            j += i;
            sendadc[3] = j;
            sendadc[2] = 0x0f & (j >> 8);
            //            USIxfer(sendadc);
#endif
        }
    }
}
