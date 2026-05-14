#ifndef SF_LOCATOR_H
#define SF_LOCATOR_H

#include "sf_err.h"

/* Loop period for the locator background task in ms. */
#define SF_LOCATOR_LOOP_PERIOD_MS   10

/* Pin assignments (ESP32-S3 DevKitC-1). */
#define SF_LOCATOR_LED_GPIO         48
#define SF_LOCATOR_BUTTON_GPIO      0
#define SF_LOCATOR_I2C_SDA_GPIO     1
#define SF_LOCATOR_I2C_SCL_GPIO     2

#endif /* SF_LOCATOR_H */
