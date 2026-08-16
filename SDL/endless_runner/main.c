#include "SDL3/SDL.h"
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PLAYER_WIDTH 50
#define PLAYER_HEIGHT 50
#define GROUND_HEIGHT 100
#define OBSTACLE_WIDTH 30
#define OBSTACLE_HEIGHT 50
#define GRAVITY 0.8
#define JUMP_FORCE -15
#define GAME_SPEED 5

typedef struct {
    float x, y;
    float vel_y;
    SDL_Texture* texture;
    SDL_Rect rect;
} Player;

typedef struct {
    float x, y;
    SDL_Texture* texture;
    SDL_Rect rect;
    int passed;
} Obstacle;

typedef struct {
    float x;
    SDL_Texture* texture;
    SDL_Rect rect;
} Background;

typedef struct {
    Player player;
    Obstacle* obstacles;
    int obstacle_count;
    int max_obstacles;
    Background background;
    Background background2;
    int score;
    int game_over;
    int ground_y;
    SDL_Renderer* renderer;
    int screen_width;
    int screen_height; 
} Game;

// Function prototypes
void init_game(Game* game);
void update_player(Game* game);
void update_obstacles(Game* game);
void add_obstacle(Game* game);
void render_game(Game* game);
void handle_events(Game* game);
void cleanup_game(Game* game);
void update_background(Game* game);

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow("Endless Runner",
        0,
        0,
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS  | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MOUSE_GRABBED | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);

    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, "Main Renderer");
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Initialize game
    Game game;
    game.renderer = renderer;
    init_game(&game);

    // Main game loop
    SDL_Event e;
    int quit = 0;
    Uint32 last_time = SDL_GetTicks();

    while (!quit && !game.game_over) {
        // Handle events
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                quit = 1;
            } else {
                handle_events(&game);
            }
        }

        // Calculate delta time
        Uint32 current_time = SDL_GetTicks();
        float delta_time = (current_time - last_time) / 1000.0f;
        last_time = current_time;

        // Update game state
        update_player(&game);
        update_obstacles(&game);
        update_background(&game);

        // Add obstacles randomly
        static float obstacle_timer = 0;
        obstacle_timer += delta_time;
        if (obstacle_timer > 1.5f) { // Add obstacle every 1.5 seconds
            add_obstacle(&game);
            obstacle_timer = 0;
        }

        // Render
        SDL_SetRenderDrawColor(renderer, 135, 206, 235, 255); // Sky blue
        SDL_RenderClear(renderer);
        render_game(&game);
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    cleanup_game(&game);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

void init_game(Game* game) {
    // Initialize player
    SDL_Window *window = SDL_GetRenderWindow(game->renderer);
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialise game, reason: the renderer has an invalid window");
        return;
    }
    SDL_GetWindowSize(window, &game->screen_width, &game->screen_height);
    game->player.x = 100;
    game->player.y = game->screen_height - GROUND_HEIGHT - PLAYER_HEIGHT;
    game->player.vel_y = 0;
    game->player.rect.w = PLAYER_WIDTH;
    game->player.rect.h = PLAYER_HEIGHT;

    // Initialize obstacles
    game->obstacles = malloc(sizeof(Obstacle) * 100);
    game->obstacle_count = 0;
    game->max_obstacles = 100;
    game->score = 0;
    game->game_over = 0;
    game->ground_y = game->screen_height - GROUND_HEIGHT;

    // Initialize background
    game->background.x = 0;
    game->background2.x = game->screen_width;

    // Create simple player texture (red rectangle)
    SDL_Surface* player_surface = SDL_CreateSurface(PLAYER_WIDTH, PLAYER_HEIGHT, SDL_PIXELFORMAT_RGBA8888);
    SDL_FillRect(player_surface, NULL, SDL_MapSurfaceRGB(player_surface, 255, 0, 0));
    game->player.texture = SDL_CreateTextureFromSurface(game->renderer, player_surface);
    SDL_FreeSurface(player_surface);

    // Create simple obstacle texture (green rectangle)
    SDL_Surface* obstacle_surface = SDL_CreateSurface(OBSTACLE_WIDTH, OBSTACLE_HEIGHT, SDL_PIXELFORMAT_RGBA8888);
    SDL_FillRect(obstacle_surface, NULL, SDL_MapSurfaceRGB(obstacle_surface, 0, 255, 0));
    game->obstacles[0].texture = SDL_CreateTextureFromSurface(game->renderer, obstacle_surface);
    SDL_FreeSurface(obstacle_surface);

    // Create simple background texture (brown rectangle for ground)
    SDL_Surface* ground_surface = SDL_CreateSurface(game->screen_width, GROUND_HEIGHT, SDL_PIXELFORMAT_RGBA8888);
    SDL_FillRect(ground_surface, NULL, SDL_MapSurfaceRGB(ground_surface, 139, 69, 19));
    game->background.texture = SDL_CreateTextureFromSurface(game->renderer, ground_surface);
    SDL_FreeSurface(ground_surface);

    game->background2.texture = SDL_CreateTextureFromSurface(game->renderer, ground_surface);
    SDL_FreeSurface(ground_surface);
}

void update_player(Game* game) {
    // Apply gravity
    game->player.vel_y += GRAVITY;
    game->player.y += game->player.vel_y;

    // Ground collision
    if (game->player.y > game->ground_y) {
        game->player.y = game->ground_y;
        game->player.vel_y = 0;
    }

    // Update player rect
    game->player.rect.x = (int)game->player.x;
    game->player.rect.y = (int)game->player.y;
}

void update_obstacles(Game* game) {
    int i, j;
    int new_count = 0;

    for (i = 0; i < game->obstacle_count; i++) {
        // Move obstacle
        game->obstacles[i].x -= GAME_SPEED;

        // Update rect
        game->obstacles[i].rect.x = (int)game->obstacles[i].x;
        game->obstacles[i].rect.y = (int)game->obstacles[i].y;

        // Check if obstacle passed player
        if (!game->obstacles[i].passed && game->obstacles[i].x + OBSTACLE_WIDTH < game->player.x) {
            game->obstacles[i].passed = 1;
            game->score++;
        }

        // Keep obstacle if still on screen
        if (game->obstacles[i].x + OBSTACLE_WIDTH > 0) {
            game->obstacles[new_count] = game->obstacles[i];
            new_count++;
        }
    }

    game->obstacle_count = new_count;

    // Check collisions
    for (i = 0; i < game->obstacle_count; i++) {
        if (SDL_HasIntersection(&game->player.rect, &game->obstacles[i].rect)) {
            game->game_over = 1;
        }
    }
}

void add_obstacle(Game* game) {
    if (game->obstacle_count >= game->max_obstacles) return;

    Obstacle* obstacle = &game->obstacles[game->obstacle_count];

    // Random height for obstacle
    int obstacle_height = OBSTACLE_HEIGHT + rand() % 30;
    obstacle->x = game->screen_width;
    obstacle->y = game->ground_y - obstacle_height;
    obstacle->rect.w = OBSTACLE_WIDTH;
    obstacle->rect.h = obstacle_height;
    obstacle->passed = 0;

    game->obstacle_count++;
}

void handle_events(Game* game) {
    const Uint8* keyboard_state = SDL_GetKeyboardState(NULL);

    if (keyboard_state[SDL_SCANCODE_SPACE] || keyboard_state[SDL_SCANCODE_UP]) {
        if (game->player.y >= game->ground_y) {
            game->player.vel_y = JUMP_FORCE;
        }
    }
}

void render_game(Game* game) {
    // Render ground
    SDL_Rect ground_rect = {0, game->ground_y, game->screen_width, GROUND_HEIGHT};
    SDL_RenderCopy(game->renderer, game->background.texture, NULL, &ground_rect);

    // Render background
    SDL_Rect bg_rect1 = {0, 0, game->screen_width, game->screen_height};
    SDL_RenderCopy(game->renderer, game->background.texture, NULL, &bg_rect1);

    // Render obstacles
    for (int i = 0; i < game->obstacle_count; i++) {
        SDL_RenderCopy(game->renderer, game->obstacles[i].texture, NULL, &game->obstacles[i].rect);
    }

    // Render player
    SDL_RenderCopy(game->renderer, game->player.texture, NULL, &game->player.rect);

    // Render score
    SDL_SetRenderDrawColor(game->renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(game->renderer, &game->player.rect);

    // Render score text
    char score_text[50];
    sprintf(score_text, "Score: %d", game->score);
    SDL_Surface* text_surface = SDL_CreateSurface(200, 50, SDL_PIXELFORMAT_RGBA8888);
    SDL_FillRect(text_surface, NULL, SDL_MapSurfaceRGB(text_surface, 0, 0, 0));
    SDL_Surface* text = SDL_CreateSurface(100, 30, SDL_PIXELFORMAT_RGBA8888);
    SDL_FillRect(text, NULL, SDL_MapSurfaceRGB(text, 255, 255, 255));
    SDL_Surface* score_surf = TTF_RenderText_Solid(NULL, score_text, (SDL_Color){255, 255, 255});
    SDL_Texture* score_tex = SDL_CreateTextureFromSurface(game->renderer, score_surf);
    SDL_Rect score_rect = {10, 10, 100, 30};
    SDL_RenderCopy(game->renderer, score_tex, NULL, &score_rect);
    SDL_FreeSurface(score_surf);
    SDL_DestroyTexture(score_tex);
}

void update_background(Game* game) {
    game->background.x -= GAME_SPEED * 0.5;
    game->background2.x -= GAME_SPEED * 0.5;

    if (game->background.x <= -game->screen_width) {
        game->background.x = game->screen_width;
    }
    if (game->background2.x <= -game->screen_width) {
        game->background2.x = game->screen_width;
    }
}

void cleanup_game(Game* game) {
    // Free textures
    if (game->player.texture) {
        SDL_DestroyTexture(game->player.texture);
    }
    for (int i = 0; i < game->obstacle_count; i++) {
        if (game->obstacles[i].texture) {
            SDL_DestroyTexture(game->obstacles[i].texture);
        }
    }

    // Free memory
    free(game->obstacles);
}
