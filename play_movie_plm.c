/*
* based on https://github.com/phoboslab/pl_mpeg/blob/master/pl_mpeg_player_sdl.c
*
* SPDX-License-Identifier: MIT
*/

#include <stdlib.h>
#include <stdio.h>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#include <SDL3/SDL.h>

typedef struct {
    plm_t *plm;
    double last_time;
    int wants_to_quit;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_FRect rectangle;
    SDL_AudioStream *audio_stream;
} app_t;

void app_on_video(plm_t *mpeg, plm_frame_t *frame, void *user) {
    (void)mpeg;
    app_t *self = (app_t *)user;

    if (!SDL_UpdateYUVTexture(
        self->texture,
        NULL,
        frame->y.data, frame->y.width,
        frame->cb.data, frame->cb.width,
        frame->cr.data, frame->cr.width
    )) {
        SDL_Log("SDL_UpdateYUVTexture failed: %s", SDL_GetError());
    }
}

void app_on_audio(plm_t *mpeg, plm_samples_t *samples, void *user) {
    (void)mpeg;
    app_t *self = (app_t *)user;

    if (!self->audio_stream) {
        return;
    }

    const int size = (int)(sizeof(float) * samples->count * 2);
    if (!SDL_PutAudioStreamData(self->audio_stream, samples->interleaved, size)) {
        SDL_Log("SDL_PutAudioStreamData failed: %s", SDL_GetError());
    }
}

void app_update(app_t *self) {
    double seek_to = -1;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (
            ev.type == SDL_EVENT_QUIT ||
            (ev.type == SDL_EVENT_KEY_UP && ev.key.key == SDLK_ESCAPE) ||
            (ev.type == SDL_EVENT_KEY_UP && ev.key.key == SDLK_Q)
        ) {
            self->wants_to_quit = TRUE;
        }

        // Seek 3sec forward/backward using arrow keys.
        if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_RIGHT) {
            seek_to = plm_get_time(self->plm) + 3;
        }
        else if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_LEFT) {
            seek_to = plm_get_time(self->plm) - 3;
        }
        else if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_F) {
            SDL_WindowFlags flags = SDL_GetWindowFlags(self->window);
            bool isFullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
            SDL_SetWindowFullscreen(self->window, !isFullscreen);
        }
    }

    // Clear renderer, copy texture and present.
    SDL_RenderClear(self->renderer);
    SDL_RenderTexture(self->renderer, self->texture, NULL, &self->rectangle);
    SDL_RenderPresent(self->renderer);

    // Compute delta time since the last app_update(), limit max step to 1/30s.
    const double current_time = (double)SDL_GetTicks() / 1000.0;
    double elapsed_time = current_time - self->last_time;
    if (elapsed_time > 1.0 / 30.0) {
        elapsed_time = 1.0 / 30.0;
    }
    self->last_time = current_time;

    // Seek using mouse position.
    float mouse_x, mouse_y;
    if (SDL_GetMouseState(&mouse_x, &mouse_y) & SDL_BUTTON_LMASK) {
        int sx, sy;
        SDL_GetWindowSize(self->window, &sx, &sy);
        if (sx > 0) {
            seek_to = plm_get_duration(self->plm) * (mouse_x / (float)sx);
        }
    }

    // Seek or advance decode.
    if (seek_to != -1) {
        if (self->audio_stream) {
            SDL_ClearAudioStream(self->audio_stream);
        }
        plm_seek(self->plm, seek_to, FALSE);
    }
    else {
        plm_decode(self->plm, elapsed_time);
    }

    if (plm_has_ended(self->plm)) {
        self->wants_to_quit = TRUE;
    }
}

void play_movie(const char *filename) {
    app_t *self = (app_t *)malloc(sizeof(app_t));
    if (!self) {
        return;
    }
    SDL_memset(self, 0, sizeof(app_t));

    // Initialize plmpeg, load the video file
    self->plm = plm_create_with_filename(filename);
    if (!self->plm) {
        SDL_Log("Couldn't open %s", filename);
        goto app_destroy;
    }

    if (!plm_probe(self->plm, 5000 * 1024)) {
        SDL_Log("No MPEG video or audio streams found in %s", filename);
        goto app_destroy;
    }

    const int samplerate = plm_get_samplerate(self->plm);

    SDL_Log(
        "Opened %s - framerate: %f, samplerate: %d, duration: %f",
        filename,
        plm_get_framerate(self->plm),
        samplerate,
        plm_get_duration(self->plm)
    );

    // Install decode callbacks
    plm_set_video_decode_callback(self->plm, app_on_video, self);
    plm_set_audio_decode_callback(self->plm, app_on_audio, self);

    plm_set_loop(self->plm, FALSE);
    plm_set_audio_enabled(self->plm, TRUE);
    plm_set_audio_stream(self->plm, 0);

    if (plm_get_num_audio_streams(self->plm) > 0) {
        const SDL_AudioSpec audio_spec = {
            SDL_AUDIO_F32,
            2,
            samplerate
        };

        self->audio_stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
            &audio_spec,
            NULL,
            NULL
        );

        if (!self->audio_stream) {
            SDL_Log("Failed to open audio device stream: %s", SDL_GetError());
        }
        else {
            if (!SDL_ResumeAudioStreamDevice(self->audio_stream)) {
                SDL_Log("Failed to resume audio device stream: %s", SDL_GetError());
            }

            plm_set_audio_lead_time(self->plm, 4096.0 / (double)samplerate);
        }
    }

    // Create SDL window.
    self->window = SDL_CreateWindow(
        "Loki Demo - Trailer",
        plm_get_width(self->plm),
        plm_get_height(self->plm),
        SDL_WINDOW_RESIZABLE
    );
    if (!self->window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        goto app_destroy;
    }
    // Center the window & set fullscreen
    SDL_SetWindowPosition(self->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_SetWindowFullscreen(self->window, TRUE);

    // Create renderer.
    self->renderer = SDL_CreateRenderer(self->window, NULL);
    if (!self->renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        goto app_destroy;
    }
    if (!SDL_SetRenderVSync(self->renderer, 1)) {
        SDL_Log("Warning: failed to enable vsync: %s", SDL_GetError());
    }

    // Preserve aspect ratio and center/letterbox
    if (!SDL_SetRenderLogicalPresentation(
        self->renderer,
        plm_get_width(self->plm),
        plm_get_height(self->plm),
        SDL_LOGICAL_PRESENTATION_LETTERBOX
    )) {
        SDL_Log("Failed to set logical presentation: %s", SDL_GetError());
        goto app_destroy;
    }

    // Create texture with planar YUV format.
    self->texture = SDL_CreateTexture(
        self->renderer,
        SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        plm_get_width(self->plm),
        plm_get_height(self->plm)
    );
    if (!self->texture) {
        SDL_Log("Failed to create texture: %s", SDL_GetError());
        goto app_destroy;
    }
    SDL_SetTextureScaleMode(self->texture, SDL_SCALEMODE_LINEAR);

    self->rectangle.x = 0.0f;
    self->rectangle.y = 0.0f;
    self->rectangle.w = (float)plm_get_width(self->plm);
    self->rectangle.h = (float)plm_get_height(self->plm);
    self->last_time = (double)SDL_GetTicks() / 1000.0;

    while (!self->wants_to_quit) {
        app_update(self);
    }

app_destroy:
    if (!self) {
        return;
    }
    if (self->plm) {
        plm_destroy(self->plm);
    }
    if (self->audio_stream) {
        SDL_DestroyAudioStream(self->audio_stream);
    }
    if (self->texture) {
        SDL_DestroyTexture(self->texture);
    }
    if (self->renderer) {
        SDL_DestroyRenderer(self->renderer);
    }
    if (self->window) {
        SDL_DestroyWindow(self->window);
    }
    free(self);
}
