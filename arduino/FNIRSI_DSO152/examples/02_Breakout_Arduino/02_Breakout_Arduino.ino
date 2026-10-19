/**
 * @file 02_Breakout_Arduino.ino
 * @brief 60 FPS Retro Breakout Arcade Game for FNIRSI DSO-152 in Arduino IDE
 *
 * Controls:
 *   - Move Left:  LEFT (PC3), RUN (PA10), or UP (PC6)
 *   - Move Right: RIGHT (PC5), AUTO (PC8), or DOWN (PA9)
 *   - Power Off:  Hold POWER (PC11) for 1 second
 *   - Bootloader: Hold OK/MENU (PC4) for 2 seconds
 */

#include <FNIRSI_DSO152.h>

#define COLOR_PRIMARY_DARK  0x4016
#define GAMES_NUMBER        16
#define SCORE_SIZE          30

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
    uint16_t ballx, bally, ballxold, ballyold;
    int velx, vely;
    int playerx, playerxold;
    int wallState[8];
    int score, remainingLives;
    int top, bottom, walltop, wallbottom, brickheight, brickwidth;
} game_state_type;

static const game_type* game;
static game_state_type state;
static int level = 0;
static uint32_t lastFrameTime = 0;

static inline void unsetBrick(int wall[], uint8_t x, uint8_t y) { wall[y] &= ~BIT_MASK[x]; }
static inline bool isBrickIn(const int wall[], uint8_t x, uint8_t y) { return (wall[y] & BIT_MASK[x]) != 0; }

static void drawBoxedString(int16_t x, int16_t y, const char* str, uint8_t size, uint16_t fg, uint16_t bg) {
    int16_t len = strlen(str);
    dso.fillRect(x, y, len * 8 * size, 8 * size, bg);
    dso.drawString(x, y, str, fg, bg, size);
}

static void clearDialog() {
    dso.fillScreen(COLOR_BLACK);
    dso.fillRect(0, 0, dso.width(), SCORE_SIZE, COLOR_PRIMARY_DARK);
}

static void updateLives(int lives, int remainingLives) {
    for (int i = 0; i < lives; i++) {
        uint16_t color = (i < remainingLives) ? COLOR_ORANGE : COLOR_PRIMARY_DARK;
        dso.fillRect(dso.width() - 15 - (15 * i), 10, 10, 10, color);
    }
}

static void updateScore(int score) {
    char buf[7];
    snprintf(buf, sizeof(buf), "%5d", score);
    dso.drawString(5, 7, buf, COLOR_WHITE, COLOR_PRIMARY_DARK, 2);
}

static uint16_t colorForRow(uint8_t row) {
    switch (row) {
        case 0: case 1: return COLOR_RED;
        case 2: case 3: return COLOR_ORANGE;
        case 4: case 5: return COLOR_GREEN;
        default:        return COLOR_YELLOW;
    }
}

static void drawBrick(const game_type* g, const game_state_type* s, uint8_t x, uint8_t y, uint16_t color) {
    dso.fillRect((x * s->brickwidth) + g->brickGap,
                 s->walltop + (y * s->brickheight) + g->brickGap,
                 s->brickwidth - g->brickGap,
                 s->brickheight - g->brickGap, color);
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
    for (uint8_t y = 0; y < g->rows; y++) s->wallState[y] = g->wall[y];
    s->remainingLives = g->lives;
    s->bottom = dso.height() - 30;
    s->top = SCORE_SIZE;
    s->walltop = g->top;
    s->brickheight = 10;
    s->brickwidth = dso.width() / g->columns;
    s->wallbottom = s->walltop + (g->rows * s->brickheight);
    s->playerx = (dso.width() - g->playerwidth) / 2;
    s->playerxold = s->playerx;
    s->ballx = (s->playerx + (g->playerwidth / 2)) << g->exponent;
    s->bally = (s->bottom - g->ballsize - 2) << g->exponent;
    s->ballxold = s->ballx;
    s->ballyold = s->bally;
    s->velx = g->initVelx;
    s->vely = g->initVely;
}

static void waitForPress(const char* title) {
    drawBoxedString(24, 180, title, 3, COLOR_YELLOW, COLOR_BLACK);
    drawBoxedString(16, 230, " PRESS TO START", 2, COLOR_RED, COLOR_BLACK);
    while (dso.readButtons().anyPressed()) { dso.updatePowerButton(); delay(10); }
    while (!dso.readButtons().anyPressed()) { dso.updatePowerButton(); delay(15); }
}

static void newGame(const game_type* new_game, game_state_type* s, const char* banner) {
    game = new_game;
    setupState(game, s);
    clearDialog();
    updateLives(game->lives, s->remainingLives);
    updateScore(s->score);
    setupWall(game, s);
    waitForPress(banner);
    clearDialog();
    updateLives(game->lives, s->remainingLives);
    updateScore(s->score);
    setupWall(game, s);
}

static int checkCornerCollision(const game_type* g, game_state_type* s, uint16_t x, uint16_t y) {
    if ((y >= s->walltop) && (y <= s->wallbottom)) {
        int yBrickRow = (y - s->walltop) / s->brickheight;
        int xBrickColumn = (x / s->brickwidth);
        if (xBrickColumn < g->columns && yBrickRow < g->rows && isBrickIn(s->wallState, xBrickColumn, yBrickRow)) {
            unsetBrick(s->wallState, xBrickColumn, yBrickRow);
            drawBrick(g, s, xBrickColumn, yBrickRow, COLOR_BLACK);
            s->score += pointsForRow[yBrickRow];
            updateScore(s->score);
            return 1;
        }
    }
    return 0;
}

void setup() {
    // Initialize DSO-152 in Portrait Orientation (0 = 240x320)
    dso.begin(0);
    level = 0;
    state.score = 0;
    newGame(&games[level], &state, "  BREAKOUT");
    lastFrameTime = millis();
}

void loop() {
    uint32_t now = millis();
    if (now - lastFrameTime < 16) {
        delay(1);
        return;
    }
    lastFrameTime = now;

    dso.updatePowerButton();
    DSO152_Buttons btns = dso.readButtons();

    if (btns.left || btns.run || btns.up)       state.playerx -= 3;
    if (btns.right || btns.auto_btn || btns.down) state.playerx += 3;
    if (state.playerx >= (dso.width() - game->playerwidth)) state.playerx = dso.width() - game->playerwidth;
    if (state.playerx < 0) state.playerx = 0;

    // Draw paddle
    if (state.playerxold != state.playerx) {
        if (state.playerxold < state.playerx) {
            dso.fillRect(state.playerxold, state.bottom, state.playerx - state.playerxold, game->playerheight, COLOR_BLACK);
        } else {
            dso.fillRect(state.playerx + game->playerwidth, state.bottom, state.playerxold - state.playerx, game->playerheight, COLOR_BLACK);
        }
    }
    dso.fillRect(state.playerx, state.bottom, game->playerwidth, game->playerheight, COLOR_WHITE);
    state.playerxold = state.playerx;

    // Update ball position
    state.ballx += state.velx;
    state.bally += state.vely;

    uint16_t bx = state.ballx >> game->exponent;
    uint16_t by = state.bally >> game->exponent;

    // Brick collisions
    int collisions = 0;
    collisions += checkCornerCollision(game, &state, bx, by);
    collisions += checkCornerCollision(game, &state, bx + game->ballsize, by + game->ballsize);
    collisions += checkCornerCollision(game, &state, bx, by + game->ballsize);
    collisions += checkCornerCollision(game, &state, bx + game->ballsize, by);
    if (collisions > 0) {
        state.vely = -state.vely;
        if ((((bx % state.brickwidth) == 0) && (state.velx < 0)) ||
            ((((bx + game->ballsize) % state.brickwidth) == 0) && (state.velx > 0))) {
            state.velx = -state.velx;
        }
    }

    // Border & paddle collisions
    if (bx + game->ballsize >= dso.width()) state.velx = -abs(state.velx);
    if ((int16_t)bx <= 0) state.velx = abs(state.velx);
    if (by <= SCORE_SIZE) state.vely = abs(state.vely);
    if (((by + game->ballsize) >= state.bottom) &&
        ((by + game->ballsize) <= (state.bottom + game->playerheight)) &&
        (bx >= state.playerx) && (bx <= (state.playerx + game->playerwidth))) {
        state.vely = -abs(state.vely);
    }

    // Bottom exit
    if ((by + game->ballsize) >= dso.height()) {
        state.remainingLives--;
        updateLives(game->lives, state.remainingLives);
        delay(500);
        state.ballx = (state.playerx + game->playerwidth / 2) << game->exponent;
        state.bally = (state.bottom - game->ballsize - 2) << game->exponent;
        state.vely = -abs(state.vely);
    }

    // Draw ball
    uint16_t bxold = state.ballxold >> game->exponent;
    uint16_t byold = state.ballyold >> game->exponent;
    if (bxold != bx || byold != by) {
        dso.fillRect(bxold, byold, game->ballsize, game->ballsize, COLOR_BLACK);
    }
    dso.fillRect(bx, by, game->ballsize, game->ballsize, COLOR_YELLOW);
    state.ballxold = state.ballx;
    state.ballyold = state.bally;

    // Check level clear or game over
    bool cleared = true;
    for (int r = 0; r < game->rows; r++) {
        if (state.wallState[r] != 0) { cleared = false; break; }
    }
    if (cleared && (level + 1) < GAMES_NUMBER) {
        level++;
        newGame(&games[level], &state, " NEXT LEVEL");
    } else if (state.remainingLives <= 0) {
        state.score = 0;
        level = 0;
        newGame(&games[level], &state, " GAME OVER");
    }
}
