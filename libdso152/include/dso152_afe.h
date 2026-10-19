#ifndef DSO152_AFE_H
#define DSO152_AFE_H

#include <stdint.h>
#include <stdbool.h>

/* Vertical Sensitivity Ranges */
typedef enum {
    AFE_RANGE_10mV  = 0,
    AFE_RANGE_20mV  = 1,
    AFE_RANGE_50mV  = 2,
    AFE_RANGE_100mV = 3,
    AFE_RANGE_200mV = 4,
    AFE_RANGE_500mV = 5,
    AFE_RANGE_1V    = 6,
    AFE_RANGE_2V    = 7,
    AFE_RANGE_5V    = 8,
    AFE_RANGE_10V   = 9,
    AFE_RANGE_20V   = 10
} dso152_range_t;

void dso152_afe_init(void);
void dso152_afe_set_range(dso152_range_t range);
void dso152_afe_set_coupling(bool ac_coupling);
uint16_t dso152_adc_read_single(void);
void dso152_adc_start_dma(uint16_t *buffer, uint16_t length);
bool dso152_adc_dma_is_ready(void);

#endif /* DSO152_AFE_H */
