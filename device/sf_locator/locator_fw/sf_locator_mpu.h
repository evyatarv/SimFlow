#ifndef SF_LOCATOR_MPU_H
#define SF_LOCATOR_MPU_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "sf_err.h"

typedef struct {
    bool        ready;
    float       baseline_az;
    float       last_temperature_c;
} sf_mpu_status_t;

sf_err_t    sf_locator_mpu_init(void);

/* Sample MPU + run lift/motion/overheat state machines. Call from main loop. */
void        sf_locator_mpu_tick(uint32_t now_ms);

/* Force a baseline-az calibration. Returns SF_OK on success. */
sf_err_t    sf_locator_mpu_calibrate(uint32_t now_ms);

void        sf_locator_mpu_get_status(sf_mpu_status_t *out);

#endif /* SF_LOCATOR_MPU_H */
