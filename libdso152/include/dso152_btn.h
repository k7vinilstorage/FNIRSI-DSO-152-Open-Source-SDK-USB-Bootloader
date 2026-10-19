#ifndef DSO152_BTN_H
#define DSO152_BTN_H

#include <stdint.h>
#include <stdbool.h>

/* Button Pin Definitions */
#define BTN_DOWN_PIN    9   // PA9
#define BTN_RUN_PIN     10  // PA10

#define BTN_LEFT_PIN    3   // PC3
#define BTN_OK_PIN      4   // PC4 (Menu / OK)
#define BTN_RIGHT_PIN   5   // PC5
#define BTN_UP_PIN      6   // PC6
#define BTN_MODE_PIN    7   // PC7
#define BTN_AUTO_PIN    8   // PC8
#define BTN_PWR_PIN     11  // PC11

typedef struct {
    bool mode;
    bool up;
    bool down;
    bool run;
    bool auto_btn;
    bool pwr;
    bool left;
    bool right;
    bool ok;
} dso152_buttons_t;

void dso152_btn_init(void);
dso152_buttons_t dso152_btn_read(void);
void dso152_btn_check_power_hold(void);

#endif /* DSO152_BTN_H */
