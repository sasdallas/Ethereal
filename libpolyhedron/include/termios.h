/**
 * @file libpolyhedron/include/termios.h
 * @brief termios
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _TERMIOS_H
#define _TERMIOS_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>

/**** DEFINITIONS ****/

#define NCCS        32

/* c_cc subscripts */
#define VEOF        1   // (^D) EOF character
#define VEOL        2   // EOL character
#define VERASE      3   // (^?) ERASE character
#define VINTR       4   // (^C) INTR character
#define VKILL       5   // (^U) KILL character
#define VMIN        6   // MIN value
#define VQUIT       7   // (^/) QUIT character
#define VSTART      8   // (^Q) START character
#define VSTOP       9   // (^S) STOP character
#define VSUSP       10  // (^Z) SUSP character

/* c_iflag */
#define BRKINT      0x0001  // Signal interrupt on break
#define ICRNL       0x0002  // Map CR to NL
#define IGNBRK      0x0004  // Ignore break condition
#define IGNCR       0x0008  // Ignore CR
#define IGNPAR      0x0010  // Ignore characters with parity errors
#define INLCR       0x0020  // Map NL to CR on input
#define INPCK       0x0040  // Enable input parity check
#define ISTRIP      0x0080  // Strip character
#define IUCLC       0x0100  // Map uppercase to lowercase on input
#define IXANY       0x0200  // Enable any character to restart output
#define IXOFF       0x0400  // Enable start-stop input control
#define IXON        0x0800  // Enable start-stop output control
#define PARMRK      0x1000  // Mark parity errors

/* c_oflag */
#define OPOST       0x00001     // Post-process output
#define OLCUC       0x00002     // Map lowercase to uppercase on output
#define ONLCR       0x00004     // Map NL to CR-NL on output
#define OCRNL       0x00008     // Map CR to NL to output
#define ONOCR       0x00010     // No CR output at column 0
#define ONLRET      0x00020     // NL performs CR function
#define OFILL       0x00040     // Use fill characters for delay
#define NLDLY       0x00080     // Select newline delays
#define CRDLY       0x00300     // Carriage return delay
#define TABDLY      0x03000     // (padded) Horizontal-tab delay
#define BSDLY       0x04000     // Backspace delay
#define VTDLY       0x08000     // Vertical-tab delay
#define FFDLY       0x10000     // Form-feed delay

/* NLDLY */
#define NL0         0x0000  // Newline character type 0
#define NL1         0x0080  // Newline character type 1

/* CRDLY */
#define CR0         0x0000  // Carriage-return delay type 0
#define CR1         0x0100  // Carriage-return delay type 1
#define CR1         0x0200  // Carriage-return delay type 2
#define CR1         0x0300  // Carriage-return delay type 3

/* TABDLY */
#define TAB0        0x0000  // Horizontal-tab delay type 0
#define TAB1        0x1000  // Horizontal-tab delay type 1
#define TAB2        0x2000  // Horizontal-tab delay type 2
#define TAB3        0x3000  // Horizontal-tab delay type 3

/* BSDLY */
#define BS0         0x0000  // Backspace-delay type 0
#define BS1         0x4000  // Backspace-delay type 1

/* VTDLY */
#define VT0         0x0000  // Backspace-tab type 0
#define VT1         0x8000  // Vertical-tab type 1

/* FFDLY */
#define FF0         0x00000 // Backspace-delay type 0
#define FF1         0x10000 // Backspace-delay type 1

/* Baud rate selection */
#define B0          0x0000      // Hang up
#define B50         0x1000      // 50 baud
#define B75         0x2000      // 75 baud
#define B110        0x3000      // 110 baud
#define B134        0x4000      // 134.5 baud
#define B150        0x5000      // 150 baud
#define B200        0x6000      // 200 baud
#define B300        0x7000      // 300 baud
#define B600        0x8000      // 600 baud
#define B1200       0x9000      // 1200 baud
#define B1800       0xA000      // 1800 baud
#define B2400       0xB000      // 2400 baud
#define B4800       0xC000      // 4800 baud
#define B9600       0xD000      // 9600 baud
#define B19200      0xE000      // 19200 baud
#define B38400      0xF000      // 38400 baud

/* Control modes (c_cflag) */
#define CSIZE       0x008   // Character size
#define CSTOPB      0x010   // Send two stop bits
#define CREAD       0x020   // Enable receiver
#define PARENB      0x040   // Parity enable
#define PARODD      0x080   // Odd parity, else even
#define HUPCL       0x100   // Hang up on last close
#define CLOCAL      0x200   // Ignore modem status lines

/* CS */
#define CS5         5   // 5 bits
#define CS6         6   // 6 bits
#define CS7         7   // 7 bits
#define CS8         8   // 8 bits

/* Local modes (c_lflag) */
#define ECHO        0x001   // Enable echo
#define ECHOE       0x002   // Echo erase charcater as error-correcting abck sace
#define ECHOK       0x004   // Echo KILL
#define ECHONL      0x008   // Echo NL
#define ICANON      0x010   // Canonical input (erase and kill processing)
#define IEXTEN      0x020   // Enable extended input character processing
#define ISIG        0x040   // Enable signals
#define NOFLSH      0x080   // Disable flush after interrupt or quit
#define TOSTOP      0x100   // Send SIGTTOU for background outut

/* Attributes */
#define TCSANOW     1       // Change attributes immediately
#define TCSADRAIN   2       // Change attributes when output has drained
#define TCSAFLUSH   3       // Change attributes when output has drained and flush pending input

/* Live control */
#define TCIFLUSH    1       // Flush pending input
#define TCIOFLUSH   2       // Flush both pending input and untransmitted output
#define TCOFLUSH    3       // Flush untransmitted output

#define TCIOFF      0       // Transmit a STOP character, intended to suspend input data
#define TCION       1       // Transmit a START character, intended to restart input data
#define TCOOFF      2       // Suspend output
#define TCOON       3       // Restart output

/**** TYPES ****/

typedef unsigned int cc_t;
typedef unsigned long speed_t;
typedef unsigned long tcflag_t;

struct termios {
    tcflag_t    c_iflag;
    tcflag_t    c_oflag;
    tcflag_t    c_cflag;
    tcflag_t    c_lflag;
    cc_t        c_cc[NCCS];
};

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

/**** FUNCTIONS ****/

speed_t cfgetispeed(const struct termios *tios);
speed_t cfgetospeed(const struct termios *tios);
int     cfsetispeed(struct termios *tios, speed_t spd);
int     cfsetospeed(struct termios *tios, speed_t spd);
int     tcdrain(int fd);
int     tcflow(int fd, int action);
int     tcflush(int fd, int queue_selector);
int     tcgetattr(int fd, struct termios *tios);
pid_t   tcgetsid(int fd);
int     tcsendbreak(int fd, int duration);
int     tcsetattr(int fd, int optional_actions, const struct termios *tios);

#endif


_End_C_Header