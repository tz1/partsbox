#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

static int serfd;
static struct termios tio;

static int seravail(unsigned long tmo)
{
    struct timeval tv = { 0L, tmo };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(serfd, &fds);
    tv.tv_usec = tmo;
    int i = select(serfd + 1, &fds, NULL, NULL, &tv);
    return i;
}

static void readlen(unsigned char *buf, unsigned len)
{
    int i = 0;
    while (i < len) {
        seravail(10000);
        i += read(serfd, &buf[i], len - i);
    }
}

static unsigned char cmdjpeg[6] = "\xAA\x01\x00\x07\x00\x07";
static unsigned char cmdreq[6] = "\xAA\x04\x05\x00\x00\x00";
static unsigned char cmdsnap[6] = "\xAA\x05\x00\x00\x00\x00";
static unsigned char cmdp512[6] = "\xAA\x06\x08\x00\x02\x00";
static unsigned char cmdbaud[6] = "\xAA\x07\x01\x01\x00\x00";
static unsigned char cmdrst[6] = "\xAA\x08\x00\x00\x00\x00";
static unsigned char cmdsync[6] = "\xAA\x0D\x00\x00\x00\x00";
static unsigned char cmdack[6] = "\xAA\x0E\x0D\x00\x00\x00";
static unsigned char cmdnak[6] = "\xAA\x0F\x00\x00\x00\x00";

static int cmdwack(unsigned char *cmd)
{
    unsigned char buf[6];
    unsigned char max = 3;
    while (max--) {
        write(serfd, cmd, 6);
        if (!seravail(5000)) {
            printf("cmd %x timeout\n", cmd[1]);
            continue;
        }
        readlen(buf, 6);
        if (memcmp(cmdack, buf, 2)) {
            if (!memcmp(cmdnak, buf, 3))
                printf("cmd %x NAK %x\n", cmd[1], buf[4]);
            else
                printf("cmd %x badack %x %x %x %x\n", cmd[1], buf[0], buf[1], buf[2], buf[3]);
            continue;
        }
        if (buf[2] == cmd[1])
            return 0;
        printf("cmd %x misack\n", cmd[1]);
    }
    return 1;
}

main(int argc, char *argv[])
{
    unsigned char buf[520];
    unsigned long xsize, size;
    unsigned i;
    unsigned len;
    unsigned cks;
    unsigned maxpkt;
    unsigned pktnum;
    unsigned char bigbuf[256000], *bp;
    int outfd;

    outfd = open("cam.jpg", O_RDWR | O_CREAT, 0644);
    if( outfd < 0 )
        return -11;
    serfd = open("/dev/ttyUSB0", O_RDWR);
    if (serfd < 0)
        return -10;
    if ((tcgetattr(serfd, &tio)) == -1)
        return -1;
    cfmakeraw(&tio);

    for (;;) {
        cfsetspeed(&tio, B115200);
        if ((tcsetattr(serfd, TCSAFLUSH, &tio)) == -1)
            return -1;

        maxpkt = 300;
        printf("start\n");
        memset( &cmdack[2], 0, 4 );
        while (maxpkt--) {
            write(serfd, cmdsync, 6);
            if (!seravail(10000))
                continue;
            usleep(10000);
            if (6 != read(serfd, buf, 6))
                return -5;
            if (memcmp(cmdack, buf, 3))
                continue;
            cmdack[3] = buf[3]; // may be needed
            usleep(10000);
            if (6 != read(serfd, buf, 6))
                continue;
            if (!memcmp(cmdsync, buf, 6))
                break;
        }
        if (!maxpkt)
            return -19;
        printf("synced\n");
        cmdack[2] = 0xd;
        write(serfd, cmdack, 6);

        cmdwack(cmdbaud);
        cfsetspeed(&tio, B921600);
        if ((tcsetattr(serfd, TCSAFLUSH, &tio)) == -1)
            return -1;

        cmdwack(cmdjpeg);
        cmdwack(cmdp512);
        sleep(2); // to adjust for lighting
        for (;;) {
            usleep(1000); // small wait requires to reset
            cmdwack(cmdsnap);
            cmdwack(cmdreq);
            readlen(buf, 6);
            size = buf[5];
            size <<= 16;
            size |= buf[4] << 8;
            size |= buf[3];
            maxpkt = size / 506;
            xsize = 0;
            pktnum = 0;
            cmdack[2] = cmdack[3] = 0;
            bp = bigbuf;
            while (pktnum <= maxpkt && xsize < size) {
                cmdack[4] = pktnum;
                cmdack[5] = pktnum >> 8;
                write(serfd, cmdack, 6);
                readlen(buf, 4);
                //if (buf[0] == 0xaa && buf[1] == 0x0f) break;
                //if (buf[0] != cmdack[4] || buf[1] != cmdack[5]) break;
                len = buf[3] << 8;
                len |= buf[2];
                //if (len > 506) break;
                xsize += len;
                cks = buf[0] + buf[1] + buf[2] + buf[3];

                readlen(buf, len);
                for (i = 0; i < len; i++)
                    cks += buf[i];
                memcpy(bp, buf, len);
                bp += len;
                readlen(buf, 2);
                if ((cks & 0xff) != buf[0]) break;
                ++pktnum;
            }
            cmdack[4] = cmdack[5] = 0xf0;
            write(serfd, cmdack, 6);
            if (size != xsize) {
                printf("sz: %d xsz %d\n", size, xsize);
                write(serfd, cmdrst, 6);
                break;
            }
            printf("=%d\n", size);
            write(outfd, bigbuf, size);
            ftruncate(outfd, size);
            lseek(outfd, 0, SEEK_SET);
        }
        sleep(15);
    }
}
