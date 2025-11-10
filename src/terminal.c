// src/terminal.c
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "terminal.h"
#include "parser.h"
#include "exec.h"
#include "builtins.h"
#include "common.h"
#include "history.h"

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600
#define FONT_SIZE     16
#define MAX_INPUT_LEN 1024
#define MAX_LINES     8192
#define PROMPT        "$ "

typedef struct {
    char *text;
    SDL_Texture *texture;
    SDL_Color color;
    int width;
    int height;
} Line;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static TTF_Font *font = NULL;

static Line lines[MAX_LINES];
static int line_count = 0;
static char input_buffer[MAX_INPUT_LEN] = {0};
static int cursor_pos = 0;
static int scroll_offset = 0; // how many lines from the bottom we are scrolled up

// ---------- Helpers ----------
static void add_line_color(const char *text, SDL_Color color) {
    if (line_count >= MAX_LINES) return;

    Line *line = &lines[line_count++];
    line->text = strdup(text);
    line->color = color;

    SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(font, text, color, WINDOW_WIDTH - 20);
    line->texture = SDL_CreateTextureFromSurface(renderer, surface);
    line->width = surface->w;
    line->height = surface->h;
    SDL_FreeSurface(surface);
}

static void add_line(const char *text) {
    SDL_Color normal = {200, 200, 200, 255};
    add_line_color(text, normal);
}

static void add_output_text(const char *output, SDL_Color color) {
    const char *start = output;
    const char *newline;
    while ((newline = strchr(start, '\n')) != NULL) {
        char line[1024];
        size_t len = newline - start;
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        strncpy(line, start, len);
        line[len] = '\0';
        add_line_color(line, color);
        start = newline + 1;
    }
    if (*start) add_line_color(start, color);
}

static int text_width(const char *text) {
    int w = 0, h = 0;
    if (TTF_SizeUTF8(font, text, &w, &h) == 0) return w;
    return 0;
}

// ---------- Rendering ----------
static void render_terminal(void) {
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);

    int line_height = TTF_FontHeight(font) + 4;
    int visible_lines = WINDOW_HEIGHT / line_height - 2;
    if (visible_lines < 1) visible_lines = 1;

    int start_line = (line_count > visible_lines + scroll_offset)
                     ? line_count - visible_lines - scroll_offset
                     : 0;
    int end_line = (start_line + visible_lines < line_count)
                   ? start_line + visible_lines
                   : line_count;

    int y = 10;
    for (int i = start_line; i < end_line; ++i) {
        Line *line = &lines[i];
        SDL_Rect dest = {10, y, line->width, line->height};
        SDL_RenderCopy(renderer, line->texture, NULL, &dest);
        y += line->height + 4;
    }

    // Input line
    char display[MAX_INPUT_LEN + 32];
    snprintf(display, sizeof(display), "%s%s", PROMPT, input_buffer);

    SDL_Color input_color = {255, 255, 255, 255};
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, display, input_color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dest = {10, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dest);

    // Solid block cursor
    char prefix[MAX_INPUT_LEN + 32];
    size_t cp = (size_t)cursor_pos;
    if (cp > strlen(input_buffer)) cp = strlen(input_buffer);
    snprintf(prefix, sizeof(prefix), "%s%.*s", PROMPT, (int)cp, input_buffer);
    int px = text_width(prefix);
    int cursor_x = 10 + px;
    int cursor_y = dest.y;
    int cursor_h = dest.h;
    int cursor_w = (cursor_h > 10) ? (cursor_h / 2) : 8;

    SDL_Rect cursor_rect = {cursor_x, cursor_y, cursor_w, cursor_h};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &cursor_rect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    SDL_RenderPresent(renderer);
}

static void cleanup(void) {
    for (int i = 0; i < line_count; ++i) {
        if (lines[i].texture) SDL_DestroyTexture(lines[i].texture);
        free(lines[i].text);
    }
    if (font) TTF_CloseFont(font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    history_save();
    history_cleanup();
}

// ---------- Execute and Capture Output ----------
static void run_command_and_capture(const char *cmdline) {
    int tokcount = 0, ncmds = 0;
    char **tokens = tokenize(cmdline, &tokcount);
    command_t *cmds = parse_pipeline(tokens, tokcount, &ncmds);

    for (int i = 0; i < tokcount; ++i) free(tokens[i]);
    free(tokens);

    if (ncmds <= 0) {
        if (cmds) free(cmds);
        return;
    }

    if (try_builtin(&cmds[0])) {
        for (int i = 0; i < ncmds; ++i) free_command(&cmds[i]);
        free(cmds);
        return;
    }

    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0) {
        perror("pipe");
        execute_pipeline(cmds, ncmds);
        goto cleanup_cmds;
    }

    int stdout_backup = dup(STDOUT_FILENO);
    int stderr_backup = dup(STDERR_FILENO);
    dup2(out_pipe[1], STDOUT_FILENO);
    dup2(err_pipe[1], STDERR_FILENO);
    close(out_pipe[1]);
    close(err_pipe[1]);

    execute_pipeline(cmds, ncmds);

    fflush(stdout);
    fflush(stderr);
    dup2(stdout_backup, STDOUT_FILENO);
    dup2(stderr_backup, STDERR_FILENO);
    close(stdout_backup);
    close(stderr_backup);

    char buffer[8192];
    ssize_t nread;

    SDL_Color normal = {200, 200, 200, 255};
    SDL_Color error  = {255, 80, 80, 255};

    while ((nread = read(out_pipe[0], buffer, sizeof(buffer)-1)) > 0) {
        buffer[nread] = '\0';
        add_output_text(buffer, normal);
    }
    while ((nread = read(err_pipe[0], buffer, sizeof(buffer)-1)) > 0) {
        buffer[nread] = '\0';
        add_output_text(buffer, error);
    }

    close(out_pipe[0]);
    close(err_pipe[0]);

cleanup_cmds:
    for (int i = 0; i < ncmds; ++i) free_command(&cmds[i]);
    free(cmds);
}

// ---------- Main ----------
int terminal_run(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() < 0) {
        fprintf(stderr, "SDL_ttf init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    window = SDL_CreateWindow("Custom Shell Terminal",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", FONT_SIZE);
    if (!font) {
        fprintf(stderr, "Font load failed: %s\n", TTF_GetError());
        cleanup();
        return 1;
    }

    history_init();

    add_line("Welcome to Custom Shell (SDL Terminal)");
    add_line("-------------------------------------");

    SDL_StartTextInput();
    int running = 1;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;

            else if (e.type == SDL_TEXTINPUT) {
                size_t len = strlen(input_buffer);
                if (len + strlen(e.text.text) < MAX_INPUT_LEN - 1) {
                    memmove(input_buffer + cursor_pos + strlen(e.text.text),
                            input_buffer + cursor_pos,
                            len - cursor_pos + 1);
                    memcpy(input_buffer + cursor_pos, e.text.text, strlen(e.text.text));
                    cursor_pos += (int)strlen(e.text.text);
                }
            }

            else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_RETURN) {
                    char with_prompt[MAX_INPUT_LEN + 8];
                    snprintf(with_prompt, sizeof(with_prompt), "%s%s", PROMPT, input_buffer);
                    add_line(with_prompt);
                    if (strlen(input_buffer) > 0) {
                        history_add(input_buffer);
                        run_command_and_capture(input_buffer);
                    }
                    input_buffer[0] = '\0';
                    cursor_pos = 0;
                    scroll_offset = 0; // auto-scroll to bottom
                } else if (k == SDLK_BACKSPACE) {
                    if (cursor_pos > 0) {
                        size_t len = strlen(input_buffer);
                        memmove(input_buffer + cursor_pos - 1,
                                input_buffer + cursor_pos,
                                len - cursor_pos + 1);
                        cursor_pos--;
                    }
                } else if (k == SDLK_UP) {
                    const char *h = history_prev();
                    if (h) {
                        strncpy(input_buffer, h, MAX_INPUT_LEN - 1);
                        input_buffer[MAX_INPUT_LEN - 1] = '\0';
                        cursor_pos = (int)strlen(input_buffer);
                    }
                } else if (k == SDLK_DOWN) {
                    const char *h = history_next();
                    if (h) {
                    strncpy(input_buffer, h, MAX_INPUT_LEN - 1);
                    input_buffer[MAX_INPUT_LEN - 1] = '\0';
                    cursor_pos = (int)strlen(input_buffer);
                    }
                } else if (k == SDLK_LEFT) {
                    if (cursor_pos > 0) cursor_pos--;
                } else if (k == SDLK_RIGHT) {
                    if ((size_t)cursor_pos < strlen(input_buffer)) cursor_pos++;
                } else if (k == SDLK_PAGEUP) {
                    scroll_offset += 5;
                    if (scroll_offset > line_count - 1) scroll_offset = line_count - 1;
                } else if (k == SDLK_PAGEDOWN) {
                    scroll_offset -= 5;
                    if (scroll_offset < 0) scroll_offset = 0;
                } else if (k == SDLK_ESCAPE) {
                    running = 0;
                }
            } else if (e.type == SDL_MOUSEWHEEL) {
                scroll_offset += e.wheel.y;
                if (scroll_offset < 0) scroll_offset = 0;
                if (scroll_offset > line_count - 1) scroll_offset = line_count - 1;
            }
        }

        render_terminal();
        SDL_Delay(16);
    }

    cleanup();
    return 0;
}
