#include "dso152.h"
#include "breakout.h"

int main(void) {
    // 1. Initialize DSO152 hardware, LCD, and buttons
    dso152_init();

    // 2. Launch Breakout arcade game
    breakout_run();

    return 0;
}
