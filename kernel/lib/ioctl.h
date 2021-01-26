#ifndef __LIB__IOCTL_H__
#define __LIB__IOCTL_H__

#define TCGETS 0x5401
#define TCSETS 0x5402
#define TIOCSCTTY 0x540E
#define TIOCGWINSZ 0x5413

struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

#endif
