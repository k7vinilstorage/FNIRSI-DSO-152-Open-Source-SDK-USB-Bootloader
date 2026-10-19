#include "breakout.h"
#include "dso152.h"

#define abs(x) ((x) < 0 ? -(x) : (x))

#define COLOR_PRIMARY_DARK  0x4016
#define GAMES_NUMBER        16
#define SCORE_SIZE          30

#define lcd_width  st7789_get_width()
#define lcd_height st7789_get_height()

typedef struct {
    int ballsize;
    int playerwidth;
    int playerheight;
    int exponent;
    int top;
    int rows;
    int columns;
    int brickGap;
    int lives;
    int wall[8];
    int initVelx;
    int initVely;
} game_type;

static const game_type games[GAMES_NUMBER] = {
    { 10, 60, 8, 6, 40, 8, 8, 3, 3, { 0x18, 0x66, 0xFF, 0xDB, 0xFF, 0x7E, 0x24, 0x3C }, 28, -28 },
    { 10, 50, 8, 6, 40, 8, 8, 3, 3, { 0xFF, 0x99, 0xFF, 0xE7, 0xBD, 0xDB, 0xE7, 0xFF }, 28, -28 },
    { 10, 50, 8, 6, 40, 8, 8, 3, 3, { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 }, 28, -28 },
    {  8, 50, 8, 6, 40, 8, 8, 3, 3, { 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF }, 34, -34 },
    { 10, 40, 8, 6, 40, 8, 8, 3, 3, { 0xFF, 0xAA, 0xAA, 0xFF, 0xFF, 0xAA, 0xAA, 0xFF }, 28, -28 },
    { 10, 40, 8, 6, 40, 8, 8, 3, 3, { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA }, 28, -28 },
    { 12, 64, 8, 6, 60, 4, 2, 3, 4, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 20, -20 },
    { 12, 60, 8, 6, 60, 5, 3, 3, 4, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 22, -22 },
    { 10, 56, 8, 6, 30, 6, 4, 3, 4, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 24, -24 },
    { 10, 52, 8, 6, 30, 7, 5, 3, 4, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 26, -26 },
    {  8, 48, 8, 6, 30, 8, 6, 3, 3, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 28, -28 },
    {  8, 44, 8, 6, 30, 8, 7, 3, 3, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 30, -30 },
    {  8, 40, 8, 6, 30, 8, 8, 3, 3, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 32, -32 },
    {  8, 36, 8, 6, 40, 8, 8, 3, 3, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 34, -34 },
    {  8, 36, 8, 6, 40, 8, 8, 3, 3, { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA }, 34, -34 },
    {  8, 36, 8, 6, 40, 8, 8, 3, 3, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 36, -36 }
};

static const uint8_t BIT_MASK[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
static const uint8_t pointsForRow[] = { 7, 7, 5, 5, 3, 3, 1, 1 };

typedef struct {
    uint16_t ballx;
    uint16_t bally;
    uint16_t ballxold;
    uint16_t ballyold;
    int velx;
    int vely;
    int playerx;
    int playerxold;
    int wallState[8];
    int score;
    int remainingLives;
    int top;
    int bottom;
    int walltop;
    int wallbottom;
    int brickheight;
    int brickwidth;
} game_state_type;

static const game_type* game;
static game_state_type state;
static int level = 0;

static inline void setBrick(int wall[], uint8_t x, uint8_t y) {
    wall[y] |= BIT_MASK[x];
}

static inline void unsetBrick(int wall[], uint8_t x, uint8_t y) {
    wall[y] &= ~BIT_MASK[x];
}

static inline bool isBrickIn(const int wall[], uint8_t x, uint8_t y) {
    return (wall[y] & BIT_MASK[x]) != 0;
}

static void drawBoxedString(int16_t x, int16_t y, const char* str, uint8_t size, uint16_t fg, uint16_t bg) {
    int16_t len = 0;
    const char *p = str;
    while (*p++) len++;
    int16_t w = len * 8 * size;
    int16_t h = 8 * size;
    st7789_fill_rect(x, y, w, h, bg);
    st7789_draw_string(x, y, str, fg, bg, size);
}

static void clearDialog(void) {
    st7789_fill(COLOR_BLACK);
    st7789_fill_rect(0, 0, lcd_width, SCORE_SIZE, COLOR_PRIMARY_DARK);
}

static void updateLives(int lives, int remainingLives) {
    for (int i = 0; i < lives; i++) {
        uint16_t color = (i < remainingLives) ? COLOR_ORANGE : COLOR_PRIMARY_DARK;
        st7789_fill_rect(lcd_width - 15 - (15 * i), 10, 10, 10, color);
    }
}

static void updateScore(int score) {
    char scoreStr[6];
    scoreStr[0] = (score >= 10000) ? ('0' + (score / 10000) % 10) : ' ';
    scoreStr[1] = (score >= 1000)  ? ('0' + (score / 1000) % 10)  : ' ';
    scoreStr[2] = (score >= 100)   ? ('0' + (score / 100) % 10)   : ' ';
    scoreStr[3] = (score >= 10)    ? ('0' + (score / 10) % 10)    : ' ';
    scoreStr[4] = '0' + (score % 10);
    scoreStr[5] = '\0';
    st7789_draw_string(5, 7, scoreStr, COLOR_WHITE, COLOR_PRIMARY_DARK, 2);
}

static uint16_t colorForRow(uint8_t row) {
    switch (row) {
        case 0: return COLOR_RED;
        case 1: return COLOR_RED;
        case 2: return COLOR_ORANGE;
        case 3: return COLOR_ORANGE;
        case 4: return COLOR_GREEN;
        case 5: return COLOR_GREEN;
        case 6: return COLOR_YELLOW;
        case 7: return COLOR_YELLOW;
        default: return COLOR_CYAN;
    }
}

static void drawBrick(const game_type* g, const game_state_type* s, uint8_t x, uint8_t y, uint16_t color) {
    st7789_fill_rect(
        (x * s->brickwidth) + g->brickGap,
        s->walltop + (y * s->brickheight) + g->brickGap,
        s->brickwidth - g->brickGap,
        s->brickheight - g->brickGap,
        color
    );
}

static void setupWall(const game_type* g, game_state_type* s) {
    for (uint8_t y = 0; y < g->rows; y++) {
        for (uint8_t x = 0; x < g->columns; x++) {
            if (isBrickIn(s->wallState, x, y)) {
                drawBrick(g, s, x, y, colorForRow(y));
            }
        }
    }
}

static void setupState(const game_type* g, game_state_type* s) {
    for (uint8_t y = 0; y < g->rows; y++) {
        s->wallState[y] = g->wall[y];
    }
    s->remainingLives = g->lives;
    s->bottom = lcd_height - 30;
    s->top = SCORE_SIZE;
    s->walltop = g->top;
    s->brickheight = 10;
    s->brickwidth = lcd_width / g->columns;
    s->wallbottom = s->walltop + (g->rows * s->brickheight);
    s->playerx = (lcd_width - g->playerwidth) / 2;
    s->playerxold = s->playerx;
    s->ballx = (s->playerx + (g->playerwidth / 2)) << g->exponent;
    s->bally = (s->bottom - g->ballsize - 2) << g->exponent;
    s->ballxold = s->ballx;
    s->ballyold = s->bally;
    s->velx = g->initVelx;
    s->vely = g->initVely;
}

static void drawPlayer(const game_type* g, const game_state_type* s) {
    if (s->playerxold != s->playerx) {
        if (s->playerxold < s->playerx) {
            st7789_fill_rect(s->playerxold, s->bottom, s->playerx - s->playerxold, g->playerheight, COLOR_BLACK);
        } else {
            st7789_fill_rect(s->playerx + g->playerwidth, s->bottom, s->playerxold - s->playerx, g->playerheight, COLOR_BLACK);
        }
    }
    st7789_fill_rect(s->playerx, s->bottom, g->playerwidth, g->playerheight, COLOR_WHITE);
}

static void drawBall(int x, int y, int xold, int yold, int ballsize) {
    if (xold != x || yold != y) {
        st7789_fill_rect(xold, yold, ballsize, ballsize, COLOR_BLACK);
    }
    st7789_fill_rect(x, y, ballsize, ballsize, COLOR_YELLOW);
}

static bool noBricks(const game_type* g, const game_state_type* s) {
    for (int y = 0; y < g->rows; y++) {
        if (s->wallState[y] != 0) return false;
    }
    return true;
}

static void hitBrick(const game_type* g, game_state_type* s, uint8_t x, uint8_t y) {
    unsetBrick(s->wallState, x, y);
    drawBrick(g, s, x, y, COLOR_BLACK);
    s->score += pointsForRow[y];
    updateScore(s->score);
}

static int checkCornerCollision(const game_type* g, game_state_type* s, uint16_t x, uint16_t y) {
    if ((y >= s->walltop) && (y <= s->wallbottom)) {
        int yBrickRow = (y - s->walltop) / s->brickheight;
        int xBrickColumn = (x / s->brickwidth);
        if (xBrickColumn < g->columns && yBrickRow < g->rows && isBrickIn(s->wallState, xBrickColumn, yBrickRow)) {
            hitBrick(g, s, xBrickColumn, yBrickRow);
            return 1;
        }
    }
    return 0;
}

static void checkBrickCollision(const game_type* g, game_state_type* s, uint16_t x, uint16_t y) {
    int x1 = x + g->ballsize;
    int y1 = y + g->ballsize;
    int collisions = 0;
    collisions += checkCornerCollision(g, s, x, y);
    collisions += checkCornerCollision(g, s, x1, y1);
    collisions += checkCornerCollision(g, s, x, y1);
    collisions += checkCornerCollision(g, s, x1, y);
    if (collisions > 0) {
        s->vely = -s->vely;
        if ((((x % s->brickwidth) == 0) && (s->velx < 0)) ||
            ((((x + g->ballsize) % s->brickwidth) == 0) && (s->velx > 0))) {
            s->velx = -s->velx;
        }
    }
}

static void checkBorderCollision(const game_type* g, game_state_type* s, uint16_t x, uint16_t y) {
    if (x + g->ballsize >= lcd_width) {
        s->velx = -abs(s->velx);
    }
    if ((int16_t)x <= 0) {
        s->velx = abs(s->velx);
    }
    if (y <= SCORE_SIZE) {
        s->vely = abs(s->vely);
    }
    if (((y + g->ballsize) >= s->bottom) &&
        ((y + g->ballsize) <= (s->bottom + g->playerheight)) &&
        (x >= s->playerx) &&
        (x <= (s->playerx + g->playerwidth))) {
        if (x > (s->playerx + g->playerwidth - 6)) {
            s->velx = s->velx - 1;
        } else if (x < s->playerx + 6) {
            s->velx = s->velx + 1;
        }
        s->vely = -abs(s->vely);
    }
}

static void checkBallCollisions(const game_type* g, game_state_type* s, uint16_t x, uint16_t y) {
    checkBrickCollision(g, s, x, y);
    checkBorderCollision(g, s, x, y);
}

static void checkBallExit(const game_type* g, game_state_type* s, uint16_t x, uint16_t y) {
    (void)x;
    if ((y + g->ballsize) >= lcd_height) {
        s->remainingLives--;
        updateLives(g->lives, s->remainingLives);
        delay_ms(500);
        s->ballx = (s->playerx + g->playerwidth / 2) << g->exponent;
        s->bally = (s->bottom - g->ballsize - 2) << g->exponent;
        s->vely = -abs(s->vely);
    }
}

static bool anyButtonPressed(void) {
    dso152_buttons_t b = dso152_btn_read();
    return (b.left || b.right || b.run || b.auto_btn ||
            b.up || b.down || b.ok || b.mode);
}

static void touchToStart(void) {
    drawBoxedString(24, 180, "  BREAKOUT", 3, COLOR_YELLOW, COLOR_BLACK);
    drawBoxedString(16, 230, " PRESS TO START", 2, COLOR_RED, COLOR_BLACK);
    while (anyButtonPressed()) {
        dso152_btn_check_power_hold();
        delay_ms(10);
    }
    while (!anyButtonPressed()) {
        dso152_btn_check_power_hold();
        delay_ms(15);
    }
}

static void gameOverTouchToStart(void) {
    drawBoxedString(24, 180, " GAME OVER", 3, COLOR_YELLOW, COLOR_BLACK);
    drawBoxedString(16, 230, " PRESS TO START", 2, COLOR_RED, COLOR_BLACK);
    while (anyButtonPressed()) {
        dso152_btn_check_power_hold();
        delay_ms(10);
    }
    while (!anyButtonPressed()) {
        dso152_btn_check_power_hold();
        delay_ms(15);
    }
}

static void newGame(const game_type* new_game, game_state_type* s) {
    game = new_game;
    setupState(game, s);

    clearDialog();
    updateLives(game->lives, s->remainingLives);
    updateScore(s->score);
    setupWall(game, s);

    touchToStart();

    clearDialog();
    updateLives(game->lives, s->remainingLives);
    updateScore(s->score);
    setupWall(game, s);
}

static int readUiSelection(const game_type* g, game_state_type* s) {
    dso152_buttons_t btns = dso152_btn_read();
    dso152_btn_check_power_hold();

    // Move left: PC3 (Left), PA10 (Run), PC6 (Up)
    if (btns.left || btns.run || btns.up) {
        s->playerx -= 3;
    }
    // Move right: PC5 (Right), PC8 (Auto), PA9 (Down)
    if (btns.right || btns.auto_btn || btns.down) {
        s->playerx += 3;
    }

    if (s->playerx >= (lcd_width - g->playerwidth)) {
        s->playerx = lcd_width - g->playerwidth;
    }
    if (s->playerx < 0) {
        s->playerx = 0;
    }
    return 1;
}

void breakout_run(void) {
    // Set LCD to Portrait orientation (240x320)
    st7789_set_rotation(0);

    level = 0;
    state.score = 0;
    newGame(&games[level], &state);

    uint32_t last_time = get_ticks();

    while (1) {
        uint32_t now = get_ticks();
        if ((now - last_time) < 16) { // ~60 FPS rate limiter
            delay_ms(1);
            continue;
        }
        last_time = now;

        readUiSelection(game, &state);

        drawPlayer(game, &state);
        state.playerxold = state.playerx;

        // Check max speed
        int max_speed = (1 << game->exponent) - 1;
        if (abs(state.vely) > max_speed) {
            state.vely = max_speed * ((state.vely > 0) - (state.vely < 0));
        }
        if (abs(state.velx) > max_speed) {
            state.velx = max_speed * ((state.velx > 0) - (state.velx < 0));
        }

        state.ballx += state.velx;
        state.bally += state.vely;

        // Check collisions
        checkBallCollisions(game, &state, state.ballx >> game->exponent, state.bally >> game->exponent);
        checkBallExit(game, &state, state.ballx >> game->exponent, state.bally >> game->exponent);

        // Draw ball in new position
        drawBall(state.ballx >> game->exponent, state.bally >> game->exponent,
                 state.ballxold >> game->exponent, state.ballyold >> game->exponent,
                 game->ballsize);

        state.ballxold = state.ballx;
        state.ballyold = state.bally;

        // Increment velocity gradually with score
        int base_v = 20 + (state.score >> 3);
        state.velx = base_v * ((state.velx > 0) - (state.velx < 0));
        state.vely = base_v * ((state.vely > 0) - (state.vely < 0));

        // Advance level if no bricks left
        if (noBricks(game, &state) && (level + 1) < GAMES_NUMBER) {
            level++;
            newGame(&games[level], &state);
        } else if (state.remainingLives <= 0) {
            gameOverTouchToStart();
            state.score = 0;
            level = 0;
            newGame(&games[level], &state);
        }
    }
}
