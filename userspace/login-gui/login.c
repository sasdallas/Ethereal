/**
 * @file userspace/login-gui/login.c
 * @brief Login GUI
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/widget.h>
#include <ethereal/celestial.h>
#include <ethereal/version.h>
#include <graphics/gfx.h>
#include <assert.h>

#define DEFAULT_BACKGROUND "/usr/share/wallpapers/lines.bmp"


window_t *bg_win = NULL;
window_t *win = NULL;
gfx_font_t *font;

struct tm last_tm;
sprite_t *sp;
widget_t *username;
widget_t *password;

void create_info_strings() {

    gfx_context_t *ctx = celestial_getGraphicsContext(bg_win);

    char str[256];

    gfx_setFontSize(font, 12);
    gfx_renderSpriteScaled(ctx, sp, GFX_RECT(0, 0, bg_win->width, bg_win->height));

    // Render versioning strings
    const ethereal_version_t *ver = ethereal_getVersion();
    snprintf(str, 256, "Ethereal v%d.%d.%d", ver->version_major, ver->version_minor, ver->version_lower);
    gfx_renderStringShadow(ctx, font, str, 10, bg_win->height - 40, GFX_RGB(255, 255, 255), 1);
    snprintf(str, 256, "Codename \"%s\"", ver->codename);
    gfx_renderStringShadow(ctx, font, str, 10, bg_win->height - 25, GFX_RGB(255, 255, 255), 1);


    gfx_string_size_t ss;

    char hostname[256] = { 0 };
    gethostname(hostname, 256);
    gfx_getStringSize(font, hostname, &ss);
    gfx_renderStringShadow(ctx, font, hostname, bg_win->width - ss.width - 10, bg_win->height - 55, GFX_RGB(255, 255, 255), 1);


    time_t rtime = time(NULL);
    struct tm *timeinfo = localtime(&rtime);

    strftime(str, 256, "%a %B %d %Y", timeinfo);
    gfx_getStringSize(font, str, &ss);
    gfx_renderStringShadow(ctx, font, str, bg_win->width - ss.width - 10, bg_win->height - 30, GFX_RGB(255, 255, 255), 1);

    strftime(str, 256, "%H:%M:%S", timeinfo);
    gfx_getStringSize(font, str, &ss);
    gfx_renderStringShadow(ctx, font, str, bg_win->width - ss.width - 10, bg_win->height - 15, GFX_RGB(255, 255, 255), 1);

    memcpy(&last_tm, timeinfo, sizeof(struct tm));

    // Final updates
    gfx_render(ctx);
    celestial_flip(bg_win);
    gfx_setFontSize(font, 12);

}

void create_background() {
    celestial_info_t *info = celestial_getServerInformation();

    wid_t wid = celestial_createWindowUndecorated(0, info->screen_width, info->screen_height);
    bg_win = celestial_getWindow(wid);
    celestial_setZArray(bg_win, CELESTIAL_Z_BACKGROUND);
    free(info);

    gfx_context_t *ctx = celestial_getGraphicsContext(bg_win);

    FILE *f = fopen(DEFAULT_BACKGROUND, "r");
    if (!f) {
        fprintf(stderr, "login-gui: " DEFAULT_BACKGROUND " is corrupted/missing\n");
        goto _backup;
    }

    sp = gfx_createSprite(0, 0);
    gfx_loadSprite(sp, f);
    fclose(f);

    create_info_strings();
    return;

_backup:
    gfx_clear(ctx, GFX_RGB(0,0,0));
    gfx_render(ctx);
    celestial_flip(bg_win);
    create_info_strings();
}

void login_event(widget_t *w, void *context) {
    fprintf(stderr, "LOGIN: %s and %s\n", ((widget_input_t*)username->impl)->buffer, ((widget_input_t*)password->impl)->buffer);

    celestial_closeWindow(win);
    sleep(1);
    celestial_closeWindow(bg_win);
    
    char *argv[] = { "desktop", NULL };
    execvp("desktop", argv);
    exit(1);
}

int main(int argc, char *argv[]) {
    font = gfx_loadFont(NULL, "/usr/share/DejaVuSans.ttf");


    create_background();
    
    wid_t wid = celestial_createWindow(0, 420, 256);
    win = celestial_getWindow(wid);
    celestial_setTitle(win, "Login to Ethereal");
    

    gfx_context_t *ctx = celestial_getGraphicsContext(win);
    gfx_clear(ctx, GFX_RGB(0xfb,0xfb,0xfb));
    widget_t *root = frame_createRoot(win, FRAME_NO_BG);

    sprite_t *s = gfx_createSprite(0, 0);
    gfx_loadSprite(s, fopen("/usr/share/EtherealLogo.bmp", "r"));

    gfx_renderSprite(ctx, s, 90, 10);
    gfx_setFontSize(font, 32);
    gfx_renderString(ctx, font, "Ethereal", 160, 55, GFX_RGB(0,0,0));
    gfx_setFontSize(font, 12);

    gfx_drawRectangleFilled(ctx, &GFX_RECT(10, 200, 400, 3), GFX_RGB(0xdd, 0xdd, 0xdd));

    username = input_create(root, INPUT_TYPE_DEFAULT, "Username", 300, 20);
    widget_renderAtCoordinates(username, 100, 100);
    password = input_create(root, INPUT_TYPE_PASSWORD, "Password", 300, 20);
    widget_renderAtCoordinates(password, 100, 150);

    widget_renderAtCoordinates(label_create(root, "User name: ", 12), 15, 113);
    widget_renderAtCoordinates(label_create(root, "Password: ", 12), 20, 163);
    

    widget_t *login_btn = button_create(root, "Login", GFX_RGB(0,0,0), BUTTON_ENABLED);
    widget_renderAtCoordinates(login_btn, 340, 220);
    login_btn->width += 15;

    widget_render(ctx, root);
    gfx_render(ctx);
    celestial_flip(win);

    widget_setHandler(login_btn, WIDGET_EVENT_CLICK, login_event, NULL);
    input_onNewline(username, login_event, NULL);
    input_onNewline(password, login_event, NULL);

    while (celestial_running()) {
        celestial_poll();
        if (widget_update(root, ctx)) celestial_flip(win);

        time_t rtime = time(NULL);
        struct tm *now = localtime(&rtime);

        if (now->tm_hour != last_tm.tm_hour || now->tm_min != last_tm.tm_min || now->tm_sec != last_tm.tm_sec) {
            create_info_strings();
        }

    }

    // what?
    system("reboot");
    return 0;
}