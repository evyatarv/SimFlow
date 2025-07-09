
#include "driver/gpio.h"
#include "sf_gpio.h"


 /*
 * In binary representation,
 * 1ULL<<CONFIG_GPIO_OUTPUT_0 is equal to 0000000000000000000001000000000000000000
 * */
 #define GPIO_OUTPUT_PIN_SEL     (1ULL<<CONFIG_GPIO_OUTPUT_0) 
