/**
 * @file userspace/miniutils/irc.c
 * @brief IRC server
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <ctype.h>
#include <stdarg.h>
#include <asm/ioctls.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>


/* Socket file */
static FILE *fsock = NULL;

/* Current nick name */
static char *irc_nick = NULL;

/* Current IRC channel */
static char *irc_channel = NULL;

/* IRC ASCII colors */
static int irc_colors[] = {
    15, 0, 4, 2, 9, 1, 5, 3, 11, 10, 6, 14, 12, 13, 8, 7
};

/* Is command */
#define ISCMD(c) (strstr(cmd, "/" c) == cmd)
#define CMDARG(c)    ({ char *m = strstr(c, " "); if (m) m++; m; })

/* IRC numerics that we recognize */
#define IRC_WELCOME         001
#define IRC_NUM_NAMES       353
#define IRC_NUM_NAMES_END   366
#define IRC_MOTD            372
#define IRC_MOTD_START      375
#define IRC_END_OF_MOTD     376

struct termios og;
/**
 * @brief Show prompot at bottom of screen
 */
static void show_prompt(char *buf) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) < 0) return;

    printf("\033[%d;1H\033[K", w.ws_row);

    if (irc_channel) {
        printf("\033[34m%s\033[0m ", irc_channel);
    }

    printf("\033[1m%s\033[0m> %s", irc_nick ? irc_nick : "irc", buf ? buf : "");
    fflush(stdout);
}


/**
 * @brief IRC client usage
 */
void usage() {
    printf("Usage: irc [-n NICK] [IP] [PORT]\n");
    printf("Client for communicating with IRC servers\n");
    exit(1);
}

/**
 * @brief IRC client version
 */
void version() {
    printf("irc (Ethereal miniutils) 1.0\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(1);
}

/**
 * @brief Get user color
 * Sourced from Toaru-IRC
 */
static int user_color(char * user) {
	int i = 0;
	while (*user) {
		i += *user;
		user++;
	}
	i = i % 5;
	switch (i) {
		case 0: return 2;
		case 1: return 3;
		case 2: return 4;
		case 3: return 6;
		case 4: return 10;
	}

	return 0;
}


/**
 * @brief Print output but make it colored
 */
void irc_write(char *fmt, ...) {
    int irc_italic = 0;
    int irc_bold = 0;

    // reset position to start
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    printf("\033[%d;1H\033[K", w.ws_row);

    va_list ap;
    va_start(ap, fmt);
    char *temp = malloc(4096);
    vsnprintf(temp, 4096, fmt, ap);
    va_end(ap);


    time_t t;
    time(&t);
    struct tm *tm = localtime(&t);
    printf("%02d:%02d ", tm->tm_hour, tm->tm_min);

    char *p = temp;
    while (*p) {
        // Check for colors...
        // 0x03 for color
        if (*p == 0x03) {
            // Advance
            p++;
            
            int fg = -1;
			int bg = -1;

            // First parse the fg
			if (isdigit(*p)) {
				fg = (*p-'0');
				p++;

                // Double number
                if (isdigit(*p)) {
                    fg *= 10;
                    fg += (*p - '0');
                    p++;
                }
            }
			
            // Comma indicates bg
			if (*p == ',') {
				p++;			
                bg = (*p - '0');
                p++;
				

				if (isdigit(*p)) {
					bg *= 10;
					bg += (*p - '0');
					p++;
				}
			}

            // Now convert this to a color pair
            if (fg != -1) fg = irc_colors[fg % 16];
            if (bg != -1) bg = irc_colors[bg % 16];
            
            // Print them out
            printf("\033[");

            if (fg == -1 || fg > 15) {
                printf("39");
            } else if (fg > 7) {
                printf("9%d", fg-8);
            } else {
                printf("3%d", fg);
            }

            printf(";");
            if (bg == -1 || bg > 15) {
                printf("49");
            } else if (bg > 7) {
                printf("10%d", bg-8);
            } else {
                printf("4%d", bg);
            }

            printf("m");
            fflush(stdout);
            continue;
        }
        
        // bold on
        if (*p == 0x02) {
            irc_bold = !irc_bold;

            if (irc_bold) printf("\033[1m");
            else printf("\033[22m");
            p++;
            continue;
        }

        if (*p == 0x16) {
            irc_italic = !irc_italic;
            if (irc_italic) printf("\033[3m");
            else printf("\022[23m");
            p++;
            continue;
        }

        putchar(*p);
        p++;
    }

    free(temp);
}

/**
 * @brief Parse IRC command/input and send it
 * @param cmd The command to parse
 */
void irc_parseCommand(char *cmd) {
    // Is this a command?
    if (*cmd == '/') {
        if (ISCMD("join")) {
            // /join command to join a channel
            char *c = CMDARG(cmd);
            if (!c) {
                irc_write("EtherealIRC: Usage: /join <channel>\n");
                return;
            }

            // Construct JOIN message
            fprintf(fsock, "JOIN %s\r\n", c);
            fflush(fsock);
            if (irc_channel) free(irc_channel);
            irc_channel = strdup(c);
        } else if (ISCMD("quit")) {
            // /quit command to quit the client
            irc_write("EtherealIRC: Closing connection with server.\n");
            char *c = CMDARG(cmd);

            if (c) {
                fprintf(fsock, "QUIT %s\r\n", c);
            } else {
                fprintf(fsock, "QUIT\r\n");
            }

            exit(0);
        } else if (ISCMD("motd")) {
            // /motd command to print MOTD
            fprintf(fsock, "MOTD\r\n");
            fflush(fsock);
        } else if (ISCMD("help")) {
            irc_write("EtherealIRC: Help not available\n");
            irc_write("EtherealIRC: (leave me alone)\n");
        } else if (ISCMD("nick")) {
            char *c = CMDARG(cmd);
            if (!c) {
                irc_write("EtherealIRC: Usage /nick <nickname>\n");
                return;
            }  

            fprintf(fsock, "NICK %s\r\n", c);
            fflush(fsock);
            free(irc_nick);
            irc_nick = strdup(c);
        } else {
            irc_write("EtherealIRC: Unrecognized command: \"%s\"\n", cmd);
        }
    } else {
        // No, send it to the current channel
        if (!irc_channel) {
            // We aren't in a channel
            irc_write("EtherealIRC: You aren't in an IRC channel - use /join to join a channel!\n");
            return;
        }

        fprintf(fsock, "PRIVMSG %s :%s\r\n", irc_channel, cmd);
        fflush(fsock);

        irc_write("\00314<\003\002%s\002\00314>\003 %s\n", irc_nick, cmd);
    }
}

/**
 * @brief Parse IRC response and display it
 * @param resp The response to parse
 */
void irc_parseResponse(char *response) {
    if (response && *response) {
        if (strstr(response, "PING") == response) {
            // Oh shoot, they are pinging us!
            char *r = strstr(response, ":");
            if (!r) {
                printf("[Ethereal] WARNING: Invalid PING request received\n");
                return;
            }

            fprintf(fsock, "PONG %s\r\n", r);
            fflush(fsock);
        }


        // IRC messages are in this format:
        // HOST CMD USER MESSAGE
        char *host = response;
        if (!host) return;
        if (*host == ':') host++; // Skip past the colon.

        char *cmd = strchr(host, ' ');
        if (!cmd) return;
        *cmd = 0;
        cmd++;

        char *chnl = strchr(cmd, ' ');
        if (!chnl) {
            irc_write("%s %s\n", host, cmd);
            return;
        }

        *chnl = 0;
        chnl++;

        char *msg = strchr(chnl, ' ');
        if (msg) {
            *msg = 0;
            msg++;
            if (*msg == ':')  msg++;
        } else {
            char *nl = strchr(chnl, '\n');
            if (nl) *nl = 0;
        }

        // Convert to int if possible
        int cmd_int = strtol(cmd, NULL, 10);

        // Also get alternative user
        char *alt_user = strchr(host, '!');
        if (!alt_user) {
            alt_user = "";
        } else {
            *alt_user = 0;
            alt_user++;
        }

        // Check command
        switch (cmd_int) {
            case IRC_WELCOME:
                irc_write("\033[42m - %s -\033[0m\n", msg);
                return;
            case IRC_NUM_NAMES:
                // Print names
                char *msg_fixed = strchr(msg, '@');
                if (!msg_fixed) { irc_write("\033[41mBad /NAMES command format received (missing @ in %s)\033[0m\n", msg); return; }
                msg_fixed += 2;
                msg_fixed = strchrnul(msg_fixed, ' ');
                if (*msg_fixed == ':') msg_fixed++;
                irc_write("\033[32mUsers in %s:\033[0m %s\n", irc_channel, msg_fixed);
                return;
            case IRC_NUM_NAMES_END:
                irc_write("\033[33mEnd of names list in %s.\033[0m\n", irc_channel);
                return;
            case IRC_MOTD:
                irc_write("\033[35m-!- MOTD:\033[0m %s\n", msg);
                return;
            case IRC_MOTD_START:
                irc_write("\033[45m- %s Message of the Day -\033[0m\n", host);
                return;
            case IRC_END_OF_MOTD:
                irc_write("\033[45m- End of Message of the Day -\033[0m\n");
                return;
        }

        // Now that we've got that, let's process the message for any special identifiers
        if (!strcmp(cmd, "PRIVMSG")) {
            if (!msg) return;
            
            if (strstr(msg, "\001ACTION ") == msg) {
                msg += 8;
                irc_write("\002* \003%d%s\003\002 %s\n", user_color(host), host, msg);
            } else {
                irc_write("\00314<\003%d%s\00314>\003 %s\n", user_color(host), host, msg);
            }
        } else if (!strcmp(cmd, "JOIN")) {
            irc_write("\033[34m%s\033[0m has joined %s\n", host, chnl);
        } else if (!strcmp(cmd, "PART")) {
            irc_write("\033[31m%s\033[0m has left %s\n", host, chnl);
        } else {
            irc_write("%s %s %s %s\n", host, cmd, chnl, msg);
        }
    

    }
}


void cleanup() {
    tcsetattr(STDIN_FILENO, TCSANOW, &og);
}

int main(int argc, char *argv[]) {
    char *nick = "EtherealUser";

    // Disable canonical mode
    struct termios new;
    tcgetattr(STDIN_FILENO, &new);
    tcgetattr(STDIN_FILENO, &og);
    atexit(cleanup);
    new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new);

    // Parse any options given to us
    int c;
    while ((c = getopt(argc, argv, "n:hv")) != -1) {
        switch (c) {
            case 'n':
                // Use custom nick
                if (!optarg) {
                    printf("irc: option \'-n\' requires an argument\n");
                    return 1;
                }

                nick = optarg;
                break;
            case 'v':
                version();
                break;
            case 'h':
            default:
                usage();
                break;
        }
    }

    if (argc - optind < 2) usage();

    // Get IP and port
    char *server_ip = argv[optind];
    uint32_t server_port = strtol(argv[optind+1], NULL, 10);

    // Establish connection
    printf("Establishing connection to %s:%d\n", server_ip, server_port);
    struct hostent *ent = gethostbyname(server_ip);
    if (!ent) {
        printf("irc: %s: not found by DNS\n", server_ip);
        return 1;
    }

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in dest = {
        .sin_port = htons(server_port),
        .sin_family = AF_INET,
    };
    memcpy(&dest.sin_addr.s_addr, ent->h_addr_list[0], ent->h_length);


    // Connect to server
    if (connect(sock, (const struct sockaddr*)&dest, sizeof(struct sockaddr_in)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to IRC server successfully\n");

    // Open socket as file and inform server of our nickname
    fsock = fdopen(sock, "r+");
    fprintf(fsock, "NICK %s\r\nUSER %s * 0 :%s\r\n", nick, nick, nick);
    fflush(fsock);

    irc_nick = strdup(nick);

    // Enter main loop
    char buf[512];
    buf[0] = 0;
    size_t bufidx = 0;
    while (1) {
        struct pollfd fds[2];
        fds[0].fd = sock;
        fds[0].events = POLLIN;
        
        fds[1].fd = STDIN_FILENO;
        fds[1].events = POLLIN;

        int p = poll(fds, 2, -1);
        if (p < 0) {
            perror("poll");
            return 1;
        }

        // Check for events on socket
        if (p && fds[0].revents & POLLIN) {
            // We have socket events
            char data[4096];
            
            ssize_t r = 0;
            if ((r = recv(sock, data, 4095, 0)) < 0) {
                perror("recv");
                return 1;
            } 

            data[r] = 0;
            
            printf("\033[2K\r");
            fflush(stdout);

            char *save;
            char *pch = strtok_r(data, "\n", &save);
            if (!pch) irc_parseResponse(data);
            while (pch) {
                irc_parseResponse(pch);
                pch = strtok_r(NULL, "\n", &save);
            }
        }

        // Anything from stdin?
        if (p && fds[1].revents & POLLIN) {
            int ch = getchar();
            if (ch == EOF) {
                clearerr(stdin);
                continue;
            }

            if (ch == '\r') ch = '\n';
            fflush(stdout);

            if (ch == '\n') {
                if (bufidx >= sizeof(buf)) bufidx = sizeof(buf) - 1;
                buf[bufidx] = '\0';
                show_prompt("");
                fflush(stdout);
                irc_parseCommand(buf);
                bufidx = 0;
                buf[0] = 0;

            } else if (ch == 8 || ch == 127) {
                if (bufidx > 0) {
                    bufidx--;
                    buf[bufidx] = '\0';
                    fflush(stdout);
                }
            } else if (ch >= 32 && ch < 127) {
                if (bufidx + 1 < sizeof(buf)) {
                    buf[bufidx++] = (char)ch;
                    buf[bufidx] = 0;
                    fflush(stdout);
                }
            }
        }


        show_prompt(buf);
    }
}