// =============================================================================
//  Desert Runner - an endless runner written in C++ with SDL3
//
//  Fully self-contained: the only dependency is SDL3 itself. No fonts, images,
//  or other assets are required. All graphics (including the score readout) are
//  drawn procedurally with filled rectangles.
//
//  Controls:
//     Space / Up / W  ..... jump  (also restarts after game over)
//     Down / S ............ duck  (crouch on the ground, dive while airborne)
//     R ................... restart
//     Esc ................. quit
// =============================================================================

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <vector>
#include <random>
#include <cmath>
#include <cstdint>
#include <algorithm>

// ---------------------------------------------------------------------------
//  Tunable constants
// ---------------------------------------------------------------------------
namespace cfg {
    constexpr int   WIN_W = 960;
    constexpr int   WIN_H = 540;

    constexpr float GROUND_Y = 452.0f;   // y-coordinate of the ground surface
    constexpr float PLAYER_X = 150.0f;   // player never moves horizontally

    constexpr float PLAYER_W = 46.0f;
    constexpr float PLAYER_H = 62.0f;
    constexpr float DUCK_H   = 34.0f;

    constexpr float GRAVITY           = 2600.0f;  // px / s^2
    constexpr float FAST_FALL_GRAVITY = 4600.0f;  // stronger pull while ducking mid-air
    constexpr float JUMP_VELOCITY     = -1010.0f; // px / s (negative == up)

    constexpr float DIFFICULTY_RAMP   = 55.0f;    // seconds to reach max difficulty
}

// ---------------------------------------------------------------------------
//  Small helpers
// ---------------------------------------------------------------------------
static std::mt19937 g_rng{ std::random_device{}() };

static float randf(float lo, float hi) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(g_rng);
}

static float lerp(float a, float b, float t) { return a + (b - a) * t; }

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

struct Color { Uint8 r, g, b, a = 255; };

static void setColor(SDL_Renderer* renderer, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

// Fill a rectangle using float coordinates.
static void fill(SDL_Renderer* renderer, float x, float y, float w, float h) {
    SDL_FRect r{ x, y, w, h };
    SDL_RenderFillRect(renderer, &r);
}

static bool aabb(const SDL_FRect& a, const SDL_FRect& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x &&
           a.y < b.y + b.h && a.y + a.h > b.y;
}

// ---------------------------------------------------------------------------
//  Seven-segment number rendering (so no font asset is needed)
// ---------------------------------------------------------------------------
namespace seg {
    enum { A = 1, B = 2, C = 4, D = 8, E = 16, F = 32, G = 64 };

    static const uint8_t DIGIT[10] = {
        A | B | C | D | E | F,          // 0
        B | C,                          // 1
        A | B | G | E | D,              // 2
        A | B | G | C | D,              // 3
        F | G | B | C,                  // 4
        A | F | G | C | D,              // 5
        A | F | G | E | C | D,          // 6
        A | B | C,                      // 7
        A | B | C | D | E | F | G,      // 8
        A | B | C | D | F | G           // 9
    };

    static void drawDigit(SDL_Renderer* renderer, int d, float x, float y,
                          float w, float h, float t) {
        if (d < 0 || d > 9) return;
        uint8_t s = DIGIT[d];
        float half = h * 0.5f;
        float vlen = half - t * 1.5f;            // vertical segment length
        if (s & A) fill(renderer, x + t,       y,                 w - 2 * t, t);
        if (s & G) fill(renderer, x + t,       y + half - t * 0.5f, w - 2 * t, t);
        if (s & D) fill(renderer, x + t,       y + h - t,         w - 2 * t, t);
        if (s & F) fill(renderer, x,           y + t,             t, vlen);
        if (s & B) fill(renderer, x + w - t,   y + t,             t, vlen);
        if (s & E) fill(renderer, x,           y + half + t * 0.5f, t, vlen);
        if (s & C) fill(renderer, x + w - t,   y + half + t * 0.5f, t, vlen);
    }

    // Draw a value right-aligned so its right edge sits at rightX.
    static void drawNumber(SDL_Renderer* renderer, int value, float rightX, float topY,
                           float dw, float dh, float t, Color col, int minDigits = 1) {
        if (value < 0) value = 0;
        int digits[12];
        int n = 0;
        int v = value;
        do { digits[n++] = v % 10; v /= 10; } while (v > 0 && n < 12);
        while (n < minDigits && n < 12) digits[n++] = 0;

        float spacing = dw * 0.30f;
        setColor(renderer, col);
        // digits[] holds least-significant first; draw from the right.
        for (int i = 0; i < n; ++i) {
            float x = rightX - (i + 1) * dw - i * spacing;
            drawDigit(renderer, digits[i], x, topY, dw, dh, t);
        }
    }
}

// Add a function to use SDL3 to find current screen width and height AI!

// ---------------------------------------------------------------------------
//  Game objects
// ---------------------------------------------------------------------------
enum class ObstacleKind { Cactus, Bird };

struct Obstacle {
    SDL_FRect box;
    ObstacleKind kind;
    float animPhase = 0.0f;   // for bird wing flap
};

struct Cloud {
    float x, y, scale;
};

// ---------------------------------------------------------------------------
//  The game
// ---------------------------------------------------------------------------
class Game {
public:
    Game() { reset(); initClouds(); }

    void reset() {
        m_bottomY   = cfg::GROUND_Y;
        m_vy        = 0.0f;
        m_onGround  = true;
        m_ducking   = false;
        m_obstacles.clear();
        m_speed        = 380.0f;
        m_elapsed      = 0.0f;
        m_scoreAcc     = 0.0f;
        m_score        = 0;
        m_spawnTimer   = 0.9f;
        m_dashOffset   = 0.0f;
        m_runPhase     = 0.0f;
        m_gameOver     = false;
        m_deadTimer    = 0.0f;
    }

    // ---- input ----------------------------------------------------------
    void requestJump() {
        if (m_gameOver) { reset(); return; }
        if (m_onGround) {
            m_vy = cfg::JUMP_VELOCITY;
            m_onGround = false;
        }
    }

    void setDucking(bool d) { m_ducking = d; }

    // ---- simulation -----------------------------------------------------
    void update(float dt) {
        // Clouds drift regardless of game state so the world feels alive.
        for (auto& c : m_clouds) {
            c.x -= (m_gameOver ? 20.0f : m_speed * 0.16f) * dt;
            if (c.x < -140.0f * c.scale) {
                c.x = cfg::WIN_W + randf(20.0f, 240.0f);
                c.y = randf(40.0f, 200.0f);
                c.scale = randf(0.6f, 1.4f);
            }
        }

        if (m_gameOver) { m_deadTimer += dt; return; }

        m_elapsed += dt;

        // Difficulty curve (0 -> 1 over DIFFICULTY_RAMP seconds).
        float t01 = clampf(m_elapsed / cfg::DIFFICULTY_RAMP, 0.0f, 1.0f);
        m_speed = lerp(380.0f, 850.0f, t01);
        float minInterval = lerp(1.05f, 0.82f, t01);
        float maxInterval = lerp(1.70f, 1.15f, t01);
        float birdChance  = (m_elapsed > 8.0f) ? lerp(0.0f, 0.42f, t01) : 0.0f;

        // Score climbs with distance travelled.
        m_scoreAcc += m_speed * dt * 0.045f;
        m_score = static_cast<int>(m_scoreAcc);

        // Ground scroll + run animation.
        m_dashOffset = std::fmod(m_dashOffset + m_speed * dt, 40.0f);
        if (m_onGround) m_runPhase += dt * 14.0f;

        // Player physics.
        float g = (m_ducking && !m_onGround) ? cfg::FAST_FALL_GRAVITY : cfg::GRAVITY;
        m_vy += g * dt;
        m_bottomY += m_vy * dt;
        if (m_bottomY >= cfg::GROUND_Y) {
            m_bottomY  = cfg::GROUND_Y;
            m_vy       = 0.0f;
            m_onGround = true;
        }

        // Spawning.
        m_spawnTimer -= dt;
        if (m_spawnTimer <= 0.0f) {
            spawnObstacle(birdChance);
            m_spawnTimer = randf(minInterval, maxInterval);
        }

        // Move / cull obstacles + wing animation.
        for (auto& o : m_obstacles) {
            o.box.x -= m_speed * dt;
            o.animPhase += dt * 16.0f;
        }
        while (!m_obstacles.empty() && m_obstacles.front().box.x + m_obstacles.front().box.w < -20.0f)
            m_obstacles.erase(m_obstacles.begin());

        // Collision (with a little forgiveness inset on the player).
        SDL_FRect p = playerBox();
        SDL_FRect hit{ p.x + 5, p.y + 4, p.w - 10, p.h - 6 };
        for (auto& o : m_obstacles) {
            if (aabb(hit, o.box)) {
                m_gameOver = true;
                if (m_score > m_highScore) m_highScore = m_score;
                break;
            }
        }
    }

    // ---- rendering ------------------------------------------------------
    void render(SDL_Renderer* renderer) {
        drawSky(renderer);
        for (const auto& c : m_clouds) drawCloud(renderer, c);
        drawGround(renderer);
        for (const auto& o : m_obstacles) drawObstacle(renderer, o);
        drawPlayer(renderer);
        drawHud(renderer);
        if (m_gameOver) drawGameOver(renderer);
    }

private:
    // ---- state ----------------------------------------------------------
    float m_bottomY, m_vy;
    bool  m_onGround, m_ducking;
    std::vector<Obstacle> m_obstacles;
    std::vector<Cloud>    m_clouds;
    float m_speed, m_elapsed, m_scoreAcc, m_spawnTimer, m_dashOffset, m_runPhase;
    int   m_score = 0;
    int   m_highScore = 0;
    bool  m_gameOver;
    float m_deadTimer;

    // ---- helpers --------------------------------------------------------
    void initClouds() {
        m_clouds.clear();
        for (int i = 0; i < 5; ++i)
            m_clouds.push_back({ randf(0.0f, cfg::WIN_W),
                                 randf(40.0f, 200.0f),
                                 randf(0.6f, 1.4f) });
    }

    SDL_FRect playerBox() const {
        float h = m_ducking ? cfg::DUCK_H : cfg::PLAYER_H;
        return SDL_FRect{ cfg::PLAYER_X, m_bottomY - h, cfg::PLAYER_W, h };
    }

    void spawnObstacle(float birdChance) {
        Obstacle o;
        if (randf(0.0f, 1.0f) < birdChance) {
            // Bird: sits at head-height so it must be ducked under (or jumped).
            o.kind = ObstacleKind::Bird;
            float w = 46.0f, h = 26.0f;
            o.box = { cfg::WIN_W + 20.0f, cfg::GROUND_Y - 40.0f - h, w, h };
        } else {
            // Cactus (single or a small cluster).
            o.kind = ObstacleKind::Cactus;
            float w = randf(22.0f, 30.0f);
            float h = randf(42.0f, 66.0f);
            if (randf(0.0f, 1.0f) < 0.30f) w += randf(16.0f, 26.0f); // wider cluster
            o.box = { cfg::WIN_W + 20.0f, cfg::GROUND_Y - h, w, h };
        }
        m_obstacles.push_back(o);
    }

    // ---- draw pieces ----------------------------------------------------
    void drawSky(SDL_Renderer* renderer) {
        // Vertical gradient built from horizontal bands.
        const Color top{ 0x8E, 0xC5, 0xFF };
        const Color bot{ 0xE7, 0xF3, 0xFF };
        const int bands = 48;
        float bh = static_cast<float>(cfg::WIN_H) / bands;
        for (int i = 0; i < bands; ++i) {
            float t = i / static_cast<float>(bands - 1);
            setColor(renderer, { (Uint8)lerp(top.r, bot.r, t),
                            (Uint8)lerp(top.g, bot.g, t),
                            (Uint8)lerp(top.b, bot.b, t) });
            fill(renderer, 0, i * bh, cfg::WIN_W, bh + 1);
        }
        // A soft sun.
        setColor(renderer, { 0xFF, 0xF2, 0xC4 });
        fill(renderer, cfg::WIN_W - 150.0f, 60.0f, 70.0f, 70.0f);
        setColor(renderer, { 0xFF, 0xE7, 0x9A });
        fill(renderer, cfg::WIN_W - 140.0f, 70.0f, 50.0f, 50.0f);
    }

    void drawCloud(SDL_Renderer* renderer, const Cloud& c) {
        setColor(renderer, { 255, 255, 255, 220 });
        float s = c.scale;
        fill(renderer, c.x,             c.y + 14 * s, 70 * s, 22 * s);
        fill(renderer, c.x + 16 * s,    c.y,          40 * s, 30 * s);
        fill(renderer, c.x + 40 * s,    c.y + 8 * s,  34 * s, 24 * s);
    }

    void drawGround(SDL_Renderer* renderer) {
        setColor(renderer, { 0xEA, 0xDD, 0xBE });                 // sand fill
        fill(renderer, 0, cfg::GROUND_Y, cfg::WIN_W, cfg::WIN_H - cfg::GROUND_Y);
        setColor(renderer, { 0x8C, 0x7A, 0x55 });                 // surface line
        fill(renderer, 0, cfg::GROUND_Y - 3, cfg::WIN_W, 4);
        // Moving dashes to convey speed.
        setColor(renderer, { 0xB6, 0xA1, 0x74 });
        for (float x = -m_dashOffset; x < cfg::WIN_W; x += 40.0f)
            fill(renderer, x, cfg::GROUND_Y + 20.0f, 22.0f, 5.0f);
        for (float x = -m_dashOffset * 1.6f; x < cfg::WIN_W; x += 64.0f)
            fill(renderer, x, cfg::GROUND_Y + 46.0f, 30.0f, 5.0f);
    }

    void drawObstacle(SDL_Renderer* renderer, const Obstacle& o) {
        if (o.kind == ObstacleKind::Cactus) {
            const Color body{ 0x2E, 0x8B, 0x4E };
            const Color dark{ 0x24, 0x6E, 0x3E };
            setColor(renderer, body);
            float cx = o.box.x, cy = o.box.y, cw = o.box.w, ch = o.box.h;
            fill(renderer, cx + cw * 0.32f, cy, cw * 0.36f, ch);          // trunk
            // arms
            fill(renderer, cx, cy + ch * 0.35f, cw * 0.30f, ch * 0.12f);
            fill(renderer, cx, cy + ch * 0.18f, cw * 0.14f, ch * 0.30f);
            fill(renderer, cx + cw * 0.70f, cy + ch * 0.28f, cw * 0.30f, ch * 0.12f);
            fill(renderer, cx + cw * 0.86f, cy + ch * 0.14f, cw * 0.14f, ch * 0.26f);
            setColor(renderer, dark);
            fill(renderer, cx + cw * 0.32f, cy, cw * 0.10f, ch);         // shading stripe
        } else {
            // Bird: body + a flapping wing.
            const Color body{ 0x4A, 0x55, 0x68 };
            const Color wing{ 0x2F, 0x38, 0x49 };
            setColor(renderer, body);
            fill(renderer, o.box.x, o.box.y + o.box.h * 0.3f, o.box.w, o.box.h * 0.5f);
            fill(renderer, o.box.x + o.box.w - 10, o.box.y + o.box.h * 0.1f, 12, 12); // head
            setColor(renderer, { 0xF6, 0xA6, 0x2B });
            fill(renderer, o.box.x + o.box.w, o.box.y + o.box.h * 0.28f, 8, 5);       // beak
            setColor(renderer, wing);
            float flap = std::sin(o.animPhase) * (o.box.h * 0.5f);
            if (flap >= 0) fill(renderer, o.box.x + 6, o.box.y - flap, o.box.w * 0.55f, 8);
            else           fill(renderer, o.box.x + 6, o.box.y + o.box.h * 0.6f - flap, o.box.w * 0.55f, 8);
        }
    }

    void drawPlayer(SDL_Renderer* renderer) {
        SDL_FRect p = playerBox();
        Color body = { 0x2B, 0x4C, 0xC0 };
        if (m_gameOver && std::fmod(m_deadTimer, 0.4f) < 0.2f)
            body = { 0xD1, 0x3A, 0x3A };   // flash red on death

        setColor(renderer, body);
        fill(renderer, p.x, p.y, p.w, p.h);

        // Belly highlight
        setColor(renderer, { 0x4C, 0x6B, 0xE0 });
        fill(renderer, p.x + p.w * 0.14f, p.y + p.h * 0.2f, p.w * 0.4f, p.h * 0.55f);

        // Eye (faces right, the running direction)
        setColor(renderer, { 255, 255, 255 });
        float eyeY = p.y + p.h * 0.18f;
        fill(renderer, p.x + p.w - 16, eyeY, 10, 10);
        setColor(renderer, { 20, 20, 30 });
        fill(renderer, p.x + p.w - 11, eyeY + 2, 5, 6);

        // Legs: alternate while grounded, tuck while airborne.
        setColor(renderer, { 0x1E, 0x33, 0x82 });
        if (m_onGround && !m_gameOver) {
            float s = std::sin(m_runPhase);
            float legH = 12.0f;
            fill(renderer, p.x + p.w * 0.18f, p.y + p.h - legH + s * 4, 10, legH - s * 4);
            fill(renderer, p.x + p.w * 0.60f, p.y + p.h - legH - s * 4, 10, legH + s * 4);
        } else {
            fill(renderer, p.x + p.w * 0.20f, p.y + p.h - 8, 12, 8);
            fill(renderer, p.x + p.w * 0.58f, p.y + p.h - 8, 12, 8);
        }
    }

    void drawHud(SDL_Renderer* renderer) {
        // Current score, top-right.
        seg::drawNumber(renderer, m_score, cfg::WIN_W - 24.0f, 22.0f,
                        22.0f, 34.0f, 4.0f, { 0x33, 0x3A, 0x4A }, 5);
        // High score, dim, to its left with a small "HI" marker.
        int hi = std::max(m_highScore, m_score);
        setColor(renderer, { 0x9A, 0xA2, 0xB2 });
        // crude "HI" tag drawn as two little bars + dot
        fill(renderer, 24, 26, 5, 26);
        fill(renderer, 24, 37, 16, 5);
        fill(renderer, 35, 26, 5, 26);
        fill(renderer, 48, 26, 5, 26);
        seg::drawNumber(renderer, hi, 158.0f, 24.0f,
                        16.0f, 26.0f, 3.0f, { 0x9A, 0xA2, 0xB2 }, 5);
    }

    void drawGameOver(SDL_Renderer* renderer) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        setColor(renderer, { 12, 16, 28, 150 });
        fill(renderer, 0, 0, cfg::WIN_W, cfg::WIN_H);

        // Panel
        float pw = 420, ph = 220;
        float px = (cfg::WIN_W - pw) / 2, py = (cfg::WIN_H - ph) / 2;
        setColor(renderer, { 0xF7, 0xF3, 0xE8, 245 });
        fill(renderer, px, py, pw, ph);
        setColor(renderer, { 0x2B, 0x4C, 0xC0 });
        fill(renderer, px, py, pw, 8);

        // "Skull-ish" X to signal game over (two crossed bars)
        setColor(renderer, { 0xD1, 0x3A, 0x3A });
        for (int i = 0; i < 34; ++i) {
            fill(renderer, px + pw / 2 - 40 + i, py + 34 + i, 6, 6);
            fill(renderer, px + pw / 2 + 40 - i, py + 34 + i, 6, 6);
        }

        // Final score, large and centered.
        int shown = std::max(m_score, 0);
        int digits = 1; for (int v = shown; v >= 10; v /= 10) digits++;
        float dw = 34, sp = dw * 0.30f;
        float totalW = digits * dw + (digits - 1) * sp;
        float rightX = px + pw / 2 + totalW / 2;
        seg::drawNumber(renderer, shown, rightX, py + 96, dw, 52, 6, { 0x33, 0x3A, 0x4A });

        // Pulsing "press to restart" bar.
        float pulse = 0.5f + 0.5f * std::sin(m_deadTimer * 4.0f);
        setColor(renderer, { 0x2B, 0x4C, 0xC0, (Uint8)(120 + pulse * 135) });
        fill(renderer, px + 90, py + ph - 46, pw - 180, 10);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
};

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Desert Runner", cfg::WIN_W, cfg::WIN_H, SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(
        renderer, 
        cfg::WIN_W, 
        cfg::WIN_H, 
        SDL_LOGICAL_PRESENTATION_LETTERBOX
    );
    SDL_SetDefaultTextureScaleMode(renderer, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderVSync(renderer, 1);

    Game game;

    bool running = true;
    Uint64 last = SDL_GetTicks();

    while (running) {
        Uint64 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;
        if (dt > 0.05f) dt = 0.05f;   // guard against big hitches

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (!e.key.repeat) {
                        switch (e.key.scancode) {
                            case SDL_SCANCODE_ESCAPE:
                                running = false;
                                break;
                            case SDL_SCANCODE_SPACE:
                            case SDL_SCANCODE_UP:
                            case SDL_SCANCODE_W:
                                game.requestJump();
                                break;
                            case SDL_SCANCODE_R:
                                game.reset();
                                break;
                            default: break;
                        }
                    }
                    break;
                default: break;
            }
        }

        const bool* keys = SDL_GetKeyboardState(nullptr);
        game.setDucking(keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]);

        game.update(dt);

        setColor(renderer, { 0x8E, 0xC5, 0xFF });
        SDL_RenderClear(renderer);
        game.render(renderer);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
