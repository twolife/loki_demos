/*
    Loki_Demos - A demo launching UI for games distributed by Loki
    Copyright (C) 2000  Loki Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <https://www.gnu.org/licenses/>.

    info@lokigames.com
*/

/* Simple program to launch the Loki demos that are installed */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <time.h>
#include <sys/wait.h>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include "loki_launch.h"


#define PRODUCT     "Loki_Demos"
#define LOGO_URL    "http://www.lokigames.com/"
#define STORE_URL   "http://www.lokigames.com/orders/"
#define MENU            "menu"
#define LAUNCH_PLAQUE   "/launch.png"
#define CONFIG_PLAQUE   "/config.png"
#define CONFIG_APPLET   "./demo_config"
#define MAX_PER_ROW 8
#define DEMO_PANEL_X        70
#define DEMO_PANEL_XSPACE   64
#define DEMO_PANEL_Y        100
#define DEMO_PANEL_YSPACE   68

/* The interface button states */
enum {
    HIDDEN = -1,
    NORMAL,
    HILITE,
    CLICKED,
    NUM_STATES
};

/* The standard pieces of the interface */
enum {
    BACKGROUND,
    LOGO,
    YEAR,
    UPDATE,
    TRAILER,
    PLAY,
    OPTIONS,
    WEBSITE,
    QUIT,
    EMPTY,
    DEMOS
};

/* The main screen surface */
static SDL_Surface *screen;
static SDL_Window *window;
static int num_dirty;
static SDL_Rect dirty_areas[128];

/* The button click sound */
static MIX_Audio *click;
static MIX_Mixer *mixer;
static MIX_Track *track;

/* The pre-defined portions of the interface */
static struct button {
    int x, y;
    int state;
    int sensitive;
    const char  *files[NUM_STATES];
    SDL_Surface *frame;
    SDL_Surface *frames[NUM_STATES];
} images[] = {
    { 0,    0,      NORMAL,  0,
        { "background.png",NULL,NULL },
        NULL, { NULL, NULL, NULL }
    },
    { 48,   22,     NORMAL,  1,
        { "loki_off.png","loki_on.png",NULL },
        NULL, { NULL, NULL, NULL }
    },
    { 192,  46,     NORMAL,  0,
        { "demodisc_2001.png",NULL,NULL },
        NULL, { NULL, NULL, NULL }
    },
    { 470,  46,     NORMAL,  1,
        { "update_off.png","update_on.png",NULL },
        NULL, { NULL, NULL, NULL }
    },
    { 216, 382,     HIDDEN,  1,
        { "viewtrailer_off.png","viewtrailer_on.png","viewtrailer_click.png" },
        NULL, { NULL, NULL, NULL }
    },
    { 216, 382,     HIDDEN,  1,
        { "playdemo_off.png", "playdemo_on.png", "playdemo_click.png" },
        NULL, { NULL, NULL, NULL }
    },
    { 340, 382,     HIDDEN,  1,
        { "demo_options_off.png","demo_options_on.png","demo_options_click.png" },
        NULL, { NULL, NULL, NULL }
    },
    { 464, 382,     HIDDEN,  1,
        { "website_off.png", "website_on.png", "website_click.png" },
        NULL, { NULL, NULL, NULL }
    },
    { 584, 446,     NORMAL,  1,
        { "quit_off.png","quit_on.png",NULL },
        NULL, { NULL, NULL, NULL }
    },
    { DEMO_PANEL_X, DEMO_PANEL_Y,   HIDDEN, 0,
        { "empty_demos.png",NULL,NULL },
        NULL, { NULL, NULL, NULL }
    }
};
static struct button *hilited_button = NULL;

/* The available demos */
static int num_demos;
struct demo {
    char *name;
    int row, col;
    char *trailer;
    char *website;
    struct button box;
    struct button caption;
    struct button text;
    struct button icon;
    struct button extra;
    struct demo *next;
} *demos = NULL, *current_demo = NULL, *hilited_demo = NULL;

static void goto_installpath(char *argv0)
{
    char temppath[PATH_MAX];
    char datapath[PATH_MAX];
    char *home;

    home = getenv("HOME");
    if ( ! home ) {
        home = ".";
    }

    strcpy(temppath, argv0);    /* If this overflows, it's your own fault :) */
    if ( ! strrchr(temppath, '/') ) {
        char *path;
        char *last;
        int found;

        found = 0;
        path = getenv("PATH");
        do {
            /* Initialize our filename variable */
            temppath[0] = '\0';

            /* Get next entry from path variable */
            last = strchr(path, ':');
            if ( ! last )
                last = path+strlen(path);

            /* Perform tilde expansion */
            if ( *path == '~' ) {
                strcpy(temppath, home);
                ++path;
            }

            /* Fill in the rest of the filename */
            if ( last > (path+1) ) {
                strncat(temppath, path, (last-path));
                strcat(temppath, "/");
            }
            strcat(temppath, "./");
            strcat(temppath, argv0);

            /* See if it exists, and update path */
            if ( access(temppath, X_OK) == 0 ) {
                ++found;
            }
            path = last+1;

        } while ( *last && !found );

    } else {
        /* Increment argv0 to the basename */
        argv0 = strrchr(argv0, '/')+1;
    }

    /* Now canonicalize it to a full pathname for the data path */
    datapath[0] = '\0';
    if ( realpath(temppath, datapath) ) {
        /* There should always be '/' in the path */
        *(strrchr(datapath, '/')) = '\0';
    }
    if ( ! *datapath || (chdir(datapath) < 0) ) {
        fprintf(stderr, "Couldn't change to install directory\n");
        exit(1);
    }
}

static void get_menu_path(const char *file, char *path, int maxlen)
{
    static char menu[128];

    if ( menu[0] == '\0' ) {
        FILE *fp;

        fp = fopen("menu.txt", "r");
        if ( fp ) {
            if ( fgets(menu, sizeof(menu), fp) ) {
                menu[strlen(menu)-1] = '\0';
            }
            fclose(fp);
        } else {
            strcpy(menu, MENU);
        }
    }
    snprintf(path, maxlen, "%s/%s", menu, file);
}

static void load_sounds(void)
{
    char path[128];

    get_menu_path("click.wav", path, sizeof(path));
    click = MIX_LoadAudio(NULL, path, false);
}

static void play_click(void)
{
    if ( click && MIX_SetTrackAudio( track, click ) ) {
        MIX_PlayTrack(track, 0);
    }
}

static void free_sounds(void)
{
    if ( click ) {
        MIX_DestroyAudio(click);
        click = NULL;
    }
}

static void add_dirty_rect(const SDL_Rect *area)
{
    dirty_areas[num_dirty++] = *area;
}

static void show_dirty_rects(void)
{
    SDL_UpdateWindowSurfaceRects(window, dirty_areas, num_dirty);
    num_dirty = 0;
}

static void load_button(struct button *button,
                        const char *normal,
                        const char *hilite,
                        const char *clicked, int initial_state)
{
    button->x = 0;
    button->y = 0;
    button->state = initial_state;
    button->sensitive = 1;

    button->files[NORMAL] = NULL;
    if ( normal ) {
        button->frames[NORMAL] = IMG_Load(normal);
    } else {
        button->frames[NORMAL] = NULL;
    }
    button->files[HILITE] = NULL;
    if ( hilite ) {
        button->frames[HILITE] = IMG_Load(hilite);
    } else {
        button->frames[HILITE] = NULL;
    }
    button->files[CLICKED] = NULL;
    if ( clicked ) {
        button->frames[CLICKED] = IMG_Load(clicked);
    } else {
        button->frames[CLICKED] = NULL;
    }
    button->frame = button->frames[NORMAL];
}

static void set_button_xy(struct button *button, int x, int y)
{
    button->x = x;
    button->y = y;
}

static void free_button(struct button *button)
{
    int i;

    for ( i=0; i<NUM_STATES; ++i ) {
        if ( button->frames[i] ) {
            SDL_DestroySurface(button->frames[i]);
        }
    }
}

static void erase_button(struct button *button)
{
    SDL_Rect area;
    SDL_Surface *background;

    background = images[BACKGROUND].frame;
    area.x = button->x;
    area.y = button->y;
    area.w = button->frame->w;
    area.h = button->frame->h;
    SDL_BlitSurface(background, &area, screen, &area);
    add_dirty_rect(&area);
}

static void draw_button(struct button *button)
{
    SDL_Rect area;

    if ( button->state != HIDDEN && button->frame ) {
        area.x = button->x;
        area.y = button->y;
        area.w = button->frame->w;
        area.h = button->frame->h;
        SDL_BlitSurface(button->frame, NULL, screen, &area);
        add_dirty_rect(&area);
    }
}

static void hide_button(struct button *button)
{
    if ( button->state != HIDDEN ) {
        erase_button(button);
        button->state = HIDDEN;
        if ( hilited_button == button ) {
            hilited_button = NULL;
        }
    }
}

static void show_button(struct button *button)
{
    if ( (button->state == HIDDEN) && button->frame ) {
        button->state = NORMAL;
        draw_button(button);
    }
}

static void reset_button(struct button *button)
{
    if ( button ) {
        if ( (button->state != NORMAL) && (button->state != HIDDEN) ) {
            if ( !current_demo || (button != &current_demo->icon) ) {
                erase_button(button);
                button->state = NORMAL;
                button->frame = button->frames[button->state];
                draw_button(button);
            }
        }
        if ( hilited_button == button ) {
            hilited_button = NULL;
        }
    }
}

static void hilite_button(struct button *button)
{
    if ( button->state == NORMAL ) {
        if ( hilited_button != button ) {
            reset_button(hilited_button);
            hilited_button = button;
        }
        erase_button(button);
        button->state = HILITE;
        if ( button->frames[button->state] ) {
            button->frame = button->frames[button->state];
        }
        draw_button(button);
    }
    hilited_button = button;
}

static void select_button(struct button *button)
{
    if ( button->state != CLICKED ) {
        if ( hilited_button != button ) {
            reset_button(hilited_button);
            hilited_button = button;
        }
        erase_button(button);
        button->state = CLICKED;
        if ( button->frames[button->state] ) {
            button->frame = button->frames[button->state];
        }
        draw_button(button);
    }
}

static void activate_button(struct button *button)
{
    play_click();
    hilite_button(button);
}

static void load_images(void)
{
    int i, state;
    char path[128];

    for ( i=0; i<(sizeof images)/(sizeof images[0]); ++i ) {
        for ( state=0; state<NUM_STATES; ++state ) {
            if ( images[i].files[state] ) {
                get_menu_path(images[i].files[state], path, sizeof(path));
                images[i].frames[state] = IMG_Load(path);
                if ( ! images[i].frames[state] && (i != EMPTY) ) {
                    fprintf(stderr, "Warning: couldn't load %s\n",
                            images[i].files[state]);
                }
            } else {
                images[i].frames[state] = NULL;
            }
            images[i].frame = images[i].frames[0];
        }
    }
    /* Special case for the update button - disable it if we can't update */
    if ( access("demos", W_OK) != 0 ) {
        images[UPDATE].state = HIDDEN;
    }
}

static void free_images(void)
{
    int i, state;
    SDL_Surface *frame;

    /* Hide all the normally hidden buttons */
    hilited_button = NULL;
    hide_button(&images[WEBSITE]);
    hide_button(&images[OPTIONS]);
    hide_button(&images[TRAILER]);
    hide_button(&images[PLAY]);

    /* Free the actual memory associated with them */
    for ( i=0; i<(sizeof images)/(sizeof images[0]); ++i ) {
        for ( state=0; state<NUM_STATES; ++state ) {
            frame = images[i].frames[state];
            if ( frame ) {
                SDL_DestroySurface(frame);
            }
        }
    }
}

static void draw_ui(void)
{
    int i;
    struct demo *list;

    for ( i=0; i<DEMOS; ++i ) {
        draw_button(&images[i]);
    }
    for ( list=demos; list; list=list->next ) {
        draw_button(&list->icon);
    }
    if ( current_demo ) {
        draw_button(&current_demo->icon);
        draw_button(&current_demo->box);
        draw_button(&current_demo->caption);
        draw_button(&current_demo->text);
        draw_button(&current_demo->extra);
    }
    show_dirty_rects();
}

static char *read_line(const char *file)
{
    FILE *fp;
    char line[1024];
    char *first_line;

    first_line = NULL;
    fp = fopen(file, "r");
    if ( fp ) {
        if ( fgets(line, sizeof(line), fp) ) {
            line[strlen(line)-1] = '\0';
            first_line = strdup(line);
        }
        fclose(fp);
    }
    return(first_line);
}

static void save_last_demo(const char *last_demo)
{
    FILE *fp;
    char path[PATH_MAX];

    sprintf(path, "%s/.loki", getenv("HOME"));
    mkdir(path, 0700);
    strcat(path, "/loki_demos");
    mkdir(path, 0700);
    strcat(path, "/last_demo.txt");
    fp = fopen(path, "w");
    if ( fp ) {
        fprintf(fp, "%s\n", last_demo);
        fclose(fp);
    }
}

static char *get_last_demo(char *last_demo_buf, int maxlen)
{
    char *last_demo;
    FILE *fp;
    char path[PATH_MAX];

    last_demo = NULL;
    sprintf(path, "%s/.loki/loki_demos/last_demo.txt", getenv("HOME"));
    fp = fopen(path, "r");
    if ( fp ) {
        if ( fgets(path, sizeof(path), fp) ) {
            path[strlen(path)-1] = '\0';
            strncpy(last_demo_buf, path, maxlen);
            last_demo = last_demo_buf;
        }
        fclose(fp);
    }
    return(last_demo);
}

static void free_demo(struct demo *demo)
{
    if ( demo->name ) {
        free(demo->name);
    }
    if ( demo->trailer ) {
        free(demo->trailer);
    }
    if ( demo->website ) {
        free(demo->website);
    }
    free_button(&demo->box);
    free_button(&demo->icon);
    free_button(&demo->caption);
    free_button(&demo->text);
    free_button(&demo->extra);
}

static void load_demo(const char *demo_name)
{
    struct demo *demo, *prev, *list;
    char path[PATH_MAX];
    char icon_normal[PATH_MAX];
    char icon_hilite[PATH_MAX];

    demo = (struct demo *)malloc(sizeof *demo);
    if ( demo ) {
        memset(demo, 0, (sizeof *demo));

        /* Copy the name of the demo for alphabetizing */
        demo->name = strdup(demo_name);
        if ( ! demo->name ) {
            /* Uh oh.. */
            fprintf(stderr, "Out of memory\n");
            free_demo(demo);
            return;
        }

        /* Load the trailer for this game */
        sprintf(path, "demos/%s/trailer.mpg", demo_name);
        if ( access(path, R_OK) == 0 ) {
            demo->trailer = strdup(path);
        } else {
            demo->trailer = NULL;
        }

        /* Load the homepage for the game */
        sprintf(path, "demos/%s/launch/website.txt", demo_name);
        demo->website = read_line(path);

        /* Load the game icon */
        sprintf(icon_normal, "demos/%s/launch/box_off.png", demo_name);
        sprintf(icon_hilite, "demos/%s/launch/box_on.png", demo_name);
        load_button(&demo->icon, icon_normal, icon_hilite, NULL, NORMAL);
        if ( ! demo->icon.frame ) {
            fprintf(stderr, "Couldn't load icon for %s\n", demo_name);
            free_demo(demo);
            return;
        }

        /* Load the icon caption */
        sprintf(path, "demos/%s/launch/caption.png", demo_name);
        load_button(&demo->caption, path, NULL, NULL, HIDDEN);
        if ( ! demo->caption.frame ) {
            fprintf(stderr, "Couldn't load caption for %s\n", demo_name);
            free_demo(demo);
            return;
        }

        /* Load the game box */
        sprintf(path, "demos/%s/launch/box.png", demo_name);
        load_button(&demo->box, path, NULL, NULL, HIDDEN);
        set_button_xy(&demo->box, 64, 250);

        /* Load the text for the game
           FIXME: Add internationalization support?
        */
        sprintf(path, "demos/%s/launch/text.png", demo_name);
        load_button(&demo->text, path, NULL, NULL, HIDDEN);
        set_button_xy(&demo->text, 204, 244);

        /* Load the extra informational icon */
        sprintf(path, "demos/%s/launch/extra.png", demo_name);
        load_button(&demo->extra, path, NULL, NULL, HIDDEN);
        set_button_xy(&demo->extra, 514, 244);

        /* Add the demo to our list */
        prev = NULL;
        for ( list=demos; list; list = list->next ) {
            /* Search for the alphabetical match */
            if ( strcasecmp(demo->name, list->name) <= 0 ) {
                break;
            }
            prev = list;
        }
        demo->next = list;
        if ( prev ) {
            prev->next = demo;
        } else {
            demos = demo;
        }
    }
}

static void hilite_demo(struct demo *demo)
{
    struct demo *previous_demo;

    /* Set the new demo and hide the last one */
    if ( demo != hilited_demo ) {
        previous_demo = hilited_demo;
        hilited_demo = demo;
        if ( previous_demo ) {
            reset_button(&previous_demo->icon);
            hide_button(&previous_demo->caption);
        }

        /* Show the current demo */
        if ( hilited_demo ) {
            hilite_button(&hilited_demo->icon);
            show_button(&hilited_demo->caption);
        }
    }
}

static void activate_demo(struct demo *demo)
{
    struct demo *previous_demo;

    /* Set the new demo and hide the last one */
    previous_demo = current_demo;
    current_demo = demo;
    if ( previous_demo ) {
        hide_button(&previous_demo->box);
        hide_button(&previous_demo->text);
        hide_button(&previous_demo->extra);
        hide_button(&images[WEBSITE]);
        hide_button(&images[OPTIONS]);
        hide_button(&images[TRAILER]);
        hide_button(&images[PLAY]);
    }

    /* Show the current demo */
    if ( current_demo ) {
        hilite_demo(current_demo);
        show_button(&current_demo->box);
        show_button(&current_demo->text);
        show_button(&current_demo->extra);
        if ( current_demo->website ) {
            show_button(&images[WEBSITE]);
        }
        if ( current_demo->trailer ) {
            show_button(&images[TRAILER]);
        } else {
            show_button(&images[PLAY]);
            show_button(&images[OPTIONS]);
        }
        save_last_demo(current_demo->name);
    }
}

static void load_demos(void)
{
    struct demo *demo;
    DIR *dir;
    struct dirent *entry;

    /* Scan the demos directory for demos that have artwork */
    dir = opendir("demos");
    if ( dir ) {
        while ( (entry=readdir(dir)) != NULL ) {
            if ( entry->d_name[0] != '.' ) {
                load_demo(entry->d_name);
            }
        }
        closedir(dir);
    }
    /* Arrange them all on the screen */
    num_demos = 0;
    for ( demo = demos; demo; demo = demo->next ) {
        demo->row = num_demos/MAX_PER_ROW;
        demo->col = num_demos%MAX_PER_ROW;
        set_button_xy(&demo->icon,
                      DEMO_PANEL_X + demo->col * DEMO_PANEL_XSPACE,
                      DEMO_PANEL_Y + demo->row * DEMO_PANEL_YSPACE);
        set_button_xy(&demo->caption,
            demo->icon.x+(demo->icon.frame->w/2)-(demo->caption.frame->w/2) + 4,
            demo->icon.y+demo->icon.frame->h + 4);
        ++num_demos;
    }
    if ( num_demos == 0 ) {
        images[EMPTY].state = NORMAL;
    }
}

static void free_demos(void)
{
    struct demo *freeable;

    while ( demos ) {
        freeable = demos;
        free_demo(freeable);
        demos = demos->next;
    }
    current_demo = NULL;
    hilited_demo = NULL;
}

static int init_ui(int use_sound)
{
    struct demo *demo;
    char last_demo_buf[128];
    char *last_demo;

    if ( ! window ) {
        /* Initialize SDL */
        if ( SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) != true ) {
            fprintf(stderr, "Couldn't init SDL: %s\n", SDL_GetError());
            return(-1);
        }

        window = SDL_CreateWindow("Loki Demo Launcher", 640, 480, 0);
        if ( ! window ) {
            fprintf(stderr, "Couldn't create SDL_Window: %s\n", SDL_GetError());
            SDL_Quit();
            return(-1);
        }
        SDL_SetWindowIcon(window, SDL_LoadBMP("icon.bmp"));
        screen = SDL_GetWindowSurface(window);
    }

    /* Open the audio */
    if ( use_sound ) {
        MIX_Init();
        mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
        track = MIX_CreateTrack(mixer);
    }

    /* Load everything */
    if ( use_sound ) {
        load_sounds();
    }
    load_images();
    load_demos();

    /* Start up the UI, and we're done! */
    draw_ui();

    /* Select the last demo that was launched */
    demo = NULL;
    last_demo = get_last_demo(last_demo_buf, sizeof(last_demo_buf));
    if ( last_demo ) {
        demo = demos;
        while ( demo ) {
            if ( strcasecmp(last_demo, demo->name) == 0 ) {
                break;
            }
            demo = demo->next;
        }
    }
    if ( ! demo ) {
        demo = demos;
    }
    activate_demo(demo);

    return(0);
}

static void quit_ui(void)
{
    /* Free memory we've allocated */
    free_demos();
    free_images();
    free_sounds();

    /* Free system resources */
    MIX_DestroyTrack(track);
    MIX_DestroyMixer(mixer);
}

static int in_button(struct button *button, int x, int y)
{
    int is_in_area = 0;

    if ( button->sensitive && (button->state != HIDDEN) ) {
        int area_x = button->x;
        int area_y = button->y;
        int area_w = button->frame->w;
        int area_h = button->frame->h;

        is_in_area = ((x >= area_x) && (y >= area_y) &&
                      (x <= (area_x+area_w)) && (y <= (area_y+area_h)));
    }
    return(is_in_area);
}

static int in_demo_panel(int x, int y)
{
    int row, n = num_demos;

    /* If it's outside the top left edge, it's not in the demo panel */
    if ( (x < DEMO_PANEL_X) || (y < DEMO_PANEL_Y) ) {
        return(0);
    }
    /* If it's outside the bottom edge, it's not in the demo panel */
    if ( y > (DEMO_PANEL_Y + ((n/MAX_PER_ROW)+1)*DEMO_PANEL_YSPACE) ) {
        return(0);
    }
    /* If it's inside the right edge for this row, it's in the demo panel */
    row = (y - DEMO_PANEL_Y)/DEMO_PANEL_YSPACE;
    if ( n > MAX_PER_ROW ) {
        if ( row == 0 ) {
            if ( x < (DEMO_PANEL_X + MAX_PER_ROW*DEMO_PANEL_XSPACE) ) {
                return(1);
            }
            return(0);
        }
        n -= MAX_PER_ROW;
    }
    if ( x < (DEMO_PANEL_X + n*DEMO_PANEL_XSPACE) ) {
        return(1);
    }
    return(0);
}

static void show_plaque(const char *image)
{
    SDL_Surface *plaque;
    SDL_Rect dst;
    char path[128];

    /* Clear the screen */
    dst.x = 0;
    dst.y = 0;
    dst.w = screen->w;
    dst.h = screen->h;
    SDL_FillSurfaceRect(screen, &dst, 0);

    /* Show the loading plaque */
    get_menu_path(image, path, sizeof(path));
    plaque = IMG_Load(path);
    if ( plaque ) {
        dst.x = (screen->w - plaque->w)/2;
        dst.y = (screen->h - plaque->h)/2;
        dst.w = screen->w;
        dst.h = screen->h;
        SDL_BlitSurface(plaque, NULL, screen, &dst);
        SDL_DestroySurface(plaque);
    }
    SDL_UpdateWindowSurface(window);
}

/* A version of system() that keeps the UI active */
static int system_ui(const char *command)
{
    pid_t child;
    int status;

    child = fork();
    switch(child) {
        case -1:
            perror("fork() failed");
            return(-1);
        case 0:
            /* Child */
            execl("/bin/sh", "sh", "-c", command, NULL);
            perror("Couldn't exec /bin/sh");
            _exit(-1);
        default:
            /* Parent */
            break;
    }
    /* Wait for the child process to return */
    while ( waitpid(child, &status, WNOHANG) != child ) {
        SDL_PumpEvents();
        SDL_Delay(500);
    }
    return(status);
}

static char *run_ui(int *done)
{
    SDL_Event event;
    int i;
    struct demo *list;
    char *command;

    command = NULL;
    while ( SDL_PollEvent(&event) ) {
        switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                /* Find out what portion of the UI is being hilited */
                if ( in_demo_panel(event.motion.x, event.motion.y) ) {
                    for ( list=demos; list; list=list->next ) {
                        if ( in_button(&list->icon,
                                       event.motion.x, event.motion.y) ) {
                            hilite_demo(list);
                        }
                    }
                } else {
                    hilite_demo(current_demo);
                }
                for ( i=0; i<DEMOS; ++i ) {
                    if ( in_button(&images[i],
                                   event.motion.x, event.motion.y) ) {
                        hilite_button(&images[i]);
                    } else {
                        reset_button(&images[i]);
                    }
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                /* Find out what portion of the UI is being selected */
                for ( i=0; i<DEMOS; ++i ) {
                    if ( in_button(&images[i],
                                   event.button.x, event.button.y) ) {
                        select_button(&images[i]);
                        break;
                    }
                }
                if ( i == DEMOS ) {
                    for ( list=demos; list; list=list->next ) {
                        if ( in_button(&list->icon,
                                       event.button.x, event.button.y) ) {
                            select_button(&list->icon);
                        }
                    }
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                /* Find out what portion of the UI is being activated */
                if ( hilited_button && (hilited_button->state == CLICKED) ) {
                    for ( i=0; i<DEMOS; ++i ) {
                        if ( in_button(&images[i],
                                       event.button.x, event.button.y) ) {
                            activate_button(&images[i]);
                            switch (i) {
                                case LOGO:
                                    loki_launchURL(LOGO_URL);
                                    break;
                                case UPDATE:
                                    *done = 2;
                                    break;
                                case TRAILER:
                                    play_movie(current_demo->trailer);
                                    break;
                                case PLAY:
                                    command = strdup(current_demo->name);
                                    show_plaque(LAUNCH_PLAQUE);
                                    break;
                                case OPTIONS:
                                    show_plaque(CONFIG_PLAQUE);
                                    { char commandline[1024];
                                        sprintf(commandline, "%s %s",
                                            CONFIG_APPLET, current_demo->name);
                                        system_ui(commandline);
                                    }
                                    draw_ui();
                                    break;
                                case WEBSITE:
                                    loki_launchURL(STORE_URL);
                                    break;
                                case QUIT:
                                    *done = 1;
                                    break;
                            }
                            break;
                        }
                    }
                    if ( i == DEMOS ) {
                        for ( list=demos; list; list=list->next ) {
                            if ( in_button(&list->icon,
                                           event.button.x, event.button.y) ) {
                                activate_button(&list->icon);
                                activate_demo(list);
                            }
                        }
                        if ( in_button(&current_demo->box,
                                       event.button.x, event.button.y) ) {
                            loki_launchURL(current_demo->website);
                        }
                    }
                }
                break;
            case SDL_EVENT_KEY_UP:
                if ( event.key.key == SDLK_ESCAPE ) {
                    *done = 1;
                }
                break;
            case SDL_EVENT_QUIT:
                *done = 1;
                break;
        }
    }
    show_dirty_rects();

    /* Wait for any exiting URL processes */
    waitpid(-1, NULL, WNOHANG);

    return(command);
}

int main(int argc, char *argv[])
{
    int use_sound;
    int done;
    char *demo;

    /* Go to the directory where we are installed, for our data files */
    goto_installpath(argv[0]);

    /* Handle command line arguments */
    use_sound = 1;
    if ( argv[1] && ((strcmp(argv[1], "--version") == 0) ||
                     (strcmp(argv[1], "-V") == 0)) ) {
        printf("Loki Demo CD " VERSION "\n");
        return(1);
    }
    if ( argv[1] && ((strcmp(argv[1], "--nosound") == 0) ||
                     (strcmp(argv[1], "-s") == 0)) ) {
        use_sound = 0;
    }

    /* Run the demo play loop */
    done = 0;
    demo = NULL;
    while ( ! done ) {
        /* Initialize everything */
        if ( init_ui(use_sound) < 0 ) {
            return(-1);
        }

        /* Wait for the user to either quit or select a demo */
        while ( ! done && ! demo ) {
            /* Be nice and don't hog the CPU */
            SDL_Delay(20);

            demo = run_ui(&done);
        }

        /* Clean up and play selected demo, if any */
        quit_ui();
        if ( demo ) {
            char launch_path[PATH_MAX];
            char commandline[PATH_MAX*2];
            FILE *fp;

            /* Load the demo launch command */
            sprintf(launch_path, "%s/.loki/loki_demos/%s/launch.txt",
                    getenv("HOME"), demo);
            fp = fopen(launch_path, "r");
            if ( ! fp ) {   /* Need to run the preferences applet */
                sprintf(launch_path, "demos/%s/launch/launch.txt", demo);
                fp = fopen(launch_path, "r");
            }
            /* Read the command line from the launch.txt file */
            commandline[0] = '\0';
            if ( fp ) {
                if ( fgets(commandline, sizeof(commandline), fp) ) {
                    commandline[strlen(commandline)-1] = '\0';
                }
                fclose(fp);
            }
            /* If we succeeded, run the command line */
            if ( commandline[0] ) {
                system_ui(commandline);
            } else {
                fprintf(stderr, "Unable to read launch.txt for %s\n", demo);
            }
            free(demo);
            demo = NULL;
        }
    }
    SDL_Quit();
    if ( done > 1 ) { /* Perform auto-update */
        int i;
        const char *args[32];

        args[0] = "loki_update";
        args[1] = PRODUCT;
        args[2] = "--";
        if ( argc > (32-4) ) {
            argc = (32-4);
        }
        for ( i=0; i<argc; ++i ) {
            args[i+3] = argv[i];
        }
        args[i+3] = NULL;
        execvp(args[0], (char * const*) args);
        fprintf(stderr, "Couldn't exec %s, restarting\n", args[0]);
        execvp(argv[0], argv);
    }
    return(0);    
}
