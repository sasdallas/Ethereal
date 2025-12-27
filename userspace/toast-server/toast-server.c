/**
 * @file userspace/toast-server/toast-server.c
 * @brief Toast server
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/toast.h>

#include <stdio.h>
#include <ethereal/celestial.h>
#include <graphics/gfx.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <errno.h>
#include <assert.h>

#define DEBUG_TOASTS

#ifdef DEBUG_TOASTS
#define TOAST_DEBUG(...) printf("toast-server: " __VA_ARGS__)
#else
#define TOAST_DEBUG(...)
#endif

#define TOAST_SERVER_PATH           "/comm/toast-server"
#define TOAST_MAX_CLIENTS           32


typedef struct toast_data {
    window_t *win;
    int pos_x;
    int pos_y;
    int remaining;
} toast_data_t;

/* Socket */
int toast_sock = -1;

/* TODO: Stupid */
list_t *toast_clients = NULL;

/* List of toast data */
list_t *toast_data = NULL;

gfx_font_t *font_title = NULL;
gfx_font_t *font_desc = NULL;

void toast_accept() {
    int fd;
    if ((fd = accept(toast_sock, NULL, NULL)) < 0) {
        if (errno == EWOULDBLOCK) return;
        perror("accept");
        exit(1);
    }

    list_append(toast_clients, (void*)(uintptr_t)fd);
}

void toast_handle(int fd) {
    TOAST_DEBUG("Received content from fd %d\n", fd);

    toast_t t;
    ssize_t r = recv(fd, &t, sizeof(toast_t), 0);
    if (r < 0) {
        if (errno == ECONNRESET) {
            TOAST_DEBUG("Connection reset!\n");
            list_delete(toast_clients, list_find(toast_clients, (void*)(uintptr_t)fd));
            return;
        }

        perror("recv");
        return;
    }

    if (!r) {
        TOAST_DEBUG("Connection removed\n");
        list_delete(toast_clients, list_find(toast_clients, (void*)(uintptr_t)fd));
        return;
    }


    celestial_info_t *info = celestial_getServerInformation();
    int x = info->screen_width - 265;
    int y = info->screen_height - 40 - 110;
    
    // Do some shifting
    foreach (data_node, toast_data) {
        toast_data_t *dat = (toast_data_t*)data_node->value;
        dat->pos_y -= 110;
        celestial_setWindowPosition(dat->win, dat->pos_x, dat->pos_y); 
        celestial_flip(dat->win);
    }

    TOAST_DEBUG("Title = %s Description = %s\n", t.title, t.description);

    // Create window
    wid_t w = celestial_createWindowUndecorated(CELESTIAL_WINDOW_FLAG_NO_AUTO_FOCUS, 250, 100);
    window_t *win = celestial_getWindow(w);
    celestial_setWindowPosition(win, x, y);
    gfx_context_t *ctx = celestial_getGraphicsContext(win); 
    gfx_clear(ctx, GFX_RGBA(0,0,0,0));

    // Draw top bar
    gfx_drawRectangleFilled(ctx, &GFX_RECT(0, 0, 250, 20), GFX_RGBA(0,0,0,0));
    gfx_drawRoundedRectangleGradient(ctx, &GFX_RECT(0, 0, 250, 24), 4, GFX_GRADIENT_HORIZONTAL, GFX_RGB(0x3f, 0x3b, 0x42), GFX_RGB(0x95, 0x90, 0x99));
    gfx_drawRoundedRectangle(ctx, &GFX_RECT(0, 20, 250, 70), 4, GFX_RGB(255, 255, 255));
    gfx_drawRectangleFilled(ctx, &GFX_RECT(0, 20, 250, 6), GFX_RGB(255, 255, 255));


    // Render text
    int title_x = 10;

    if (!(t.flags & TOAST_FLAG_NO_ICON)) {
        // Try to render the icon too
        FILE *f = fopen(t.icon, "r");
        if (f) {
            sprite_t *sp = gfx_createSprite(0, 0);
            gfx_loadSprite(sp, f);
            fclose(f);
            gfx_renderSprite(ctx, sp, title_x, 2);
            gfx_destroySprite(sp);

            title_x += 20;
        } else {
            TOAST_DEBUG("Warning: Icon %s not found\n", t.icon);
        }
    }

    
    gfx_renderString(ctx, font_title, t.title, title_x, 15, GFX_RGB(255, 255, 255));


    int ty = 40;

    // Rendering the description is trickier since it can have newlines
    char *tok = strtok(t.description, "\n");
    while (tok) {
        gfx_renderString(ctx, font_desc, tok, 10, ty, GFX_RGB(0, 0, 0));
        tok = strtok(NULL, "\n");
        ty += 13;
    }
    
    
    // Render and flip
    gfx_render(ctx);
    celestial_flip(win);

    // Create toast data
    toast_data_t *dat = malloc(sizeof(toast_data_t));
    dat->win = win;
    dat->pos_x = x;
    dat->pos_y = y;
    dat->remaining = 10;
    list_append(toast_data, dat);

    free(info);
}

int main(int argc, char *argv[]) {
    TOAST_DEBUG("Starting toast server...\n");
    toast_clients = list_create("toast clients");
    toast_data = list_create("toast data");

    toast_sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);

    if (toast_sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
        .sun_path = TOAST_SERVER_PATH,
    };

    if (bind(toast_sock, (const struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(toast_sock, 5) < 0) {
        perror("listen");
        return 1;
    }

    int i = 1;
    if (ioctl(toast_sock, FIONBIO, &i) < 0) {
        perror("FIONBIO");
        return 1;
    }

    TOAST_DEBUG("Accepting connections on " TOAST_SERVER_PATH "\n");

    font_title = gfx_loadFont(NULL, "/usr/share/DejaVuSans.ttf");
    font_desc = gfx_loadFont(NULL, "/usr/share/DejaVuSans.ttf");

    assert(font_desc && font_title);

    gfx_setFontSize(font_desc, 11);
    gfx_setFontSize(font_title, 13);
    
    while (1) {
        struct pollfd fds[toast_clients->length + 1];
        fds[0].fd = toast_sock;
        fds[0].events = POLLIN;

        int i = 1;
        foreach(n, toast_clients) {
            fds[i].fd = (int)(uintptr_t)n->value;
            fds[i].events = POLLIN;
            i++;
        }

        long ret = poll(fds, toast_clients->length + 1, 1000);

        if (ret < 0) {
            perror("poll");
            return 1;
        }

        if (fds[0].revents & POLLIN) {
            toast_accept();
        }

        for (unsigned i = 0; i < toast_clients->length; i++) {
            if (fds[i+1].revents & POLLIN) {
                toast_handle(fds[i+1].fd);
            }
        }


        node_t *n = toast_data->head;
        while (n) {
            toast_data_t *dat = (toast_data_t*)n->value;
            dat->remaining--;

            if (!dat->remaining) {
                celestial_closeWindow(dat->win);
                free(dat);
                node_t *old = n;
                n = n->next;
                list_delete(toast_data, old);
                free(old);
                continue;
            }

            n = n->next;
        }

    }

}