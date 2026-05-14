#include <math.h>
#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

#include "sf_locator.h"
#include "sf_locator_mpu.h"
#include "sf_locator_telegram.h"

#define I2C_PORT                I2C_NUM_0
#define I2C_FREQ_HZ             100000
#define I2C_TIMEOUT_MS          50

#define MPU_ADDR_PRIMARY        0x68
#define MPU_ADDR_ALT            0x69

#define MPU_REG_SMPLRT_DIV      0x19
#define MPU_REG_CONFIG          0x1A
#define MPU_REG_GYRO_CONFIG     0x1B
#define MPU_REG_ACCEL_CONFIG    0x1C
#define MPU_REG_ACCEL_XOUT_H    0x3B
#define MPU_REG_TEMP_OUT_H      0x41
#define MPU_REG_GYRO_XOUT_H     0x43
#define MPU_REG_PWR_MGMT_1      0x6B
#define MPU_REG_WHO_AM_I        0x75

#define ACCEL_FS_8G_LSB_PER_G   4096.0f      /* +/-8g range */
#define GRAVITY_MPS2            9.80665f

#define MPU_READ_INTERVAL_MS        100
#define LIFT_EVENT_COOLDOWN_MS      3000
#define LIFT_DEBUG_INTERVAL_MS      1000
#define MOTION_WINDOW_MS            60000
#define OVERHEAT_ALERT_COOLDOWN_MS  300000
#define LIFT_DELTA_THRESHOLD        2.5f
#define MOTION_DELTA_THRESHOLD      1.2f
#define MPU_OVERHEAT_TEMP_C         60.0f
#define MPU_RECOVER_TEMP_C          55.0f
#define MPU_CALIBRATION_SAMPLES     100

static const char *TAG = "SF_LOCATOR_MPU";

static i2c_master_bus_handle_t  s_bus = NULL;
static i2c_master_dev_handle_t  s_dev = NULL;
static uint8_t                  s_addr = MPU_ADDR_PRIMARY;
static bool                     s_ready = false;
static float                    s_baseline_az = GRAVITY_MPS2;

static uint32_t s_last_read_ms = 0;
static uint32_t s_last_lift_event_ms = 0;
static uint32_t s_last_lift_debug_ms = 0;
static uint32_t s_motion_window_start_ms = 0;
static uint32_t s_last_overheat_ms = 0;
static bool     s_lift_latched = false;
static bool     s_overheat_latched = false;
static bool     s_motion_prev = false;
static uint16_t s_motion_events = 0;
static float    s_motion_peak_delta = 0.0f;
static float    s_motion_peak_gyro = 0.0f;
static float    s_motion_peak_temp_c = NAN;
static float    s_last_temp_c = NAN;

static esp_err_t mpu_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

static esp_err_t mpu_read_regs(uint8_t reg, uint8_t *out, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, out, len, I2C_TIMEOUT_MS);
}

static bool probe_addr(uint8_t addr)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = I2C_FREQ_HZ,
    };

    if (s_dev) {
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    if (i2c_master_bus_add_device(s_bus, &cfg, &s_dev) != ESP_OK) {
        return false;
    }
    s_addr = addr;

    uint8_t who = 0;
    if (mpu_read_regs(MPU_REG_WHO_AM_I, &who, 1) != ESP_OK) {
        return false;
    }
    ESP_LOGI(TAG, "WHO_AM_I @ 0x%02X = 0x%02X", addr, who);
    return (who == MPU_ADDR_PRIMARY || who == MPU_ADDR_ALT || who != 0x00);
}

static esp_err_t mpu_configure(void)
{
    esp_err_t err = mpu_write_reg(MPU_REG_PWR_MGMT_1, 0x00);   /* wake */
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(10));

    err = mpu_write_reg(MPU_REG_SMPLRT_DIV, 0x07);
    if (err != ESP_OK) return err;
    err = mpu_write_reg(MPU_REG_CONFIG, 0x04);                  /* DLPF ~21Hz */
    if (err != ESP_OK) return err;
    err = mpu_write_reg(MPU_REG_GYRO_CONFIG, 0x08);             /* +-500 dps */
    if (err != ESP_OK) return err;
    err = mpu_write_reg(MPU_REG_ACCEL_CONFIG, 0x10);            /* +-8g */
    return err;
}

static bool read_sample(float *ax, float *ay, float *az,
                        float *gx, float *gy, float *gz,
                        float *temp_c)
{
    uint8_t buf[14];
    if (mpu_read_regs(MPU_REG_ACCEL_XOUT_H, buf, sizeof(buf)) != ESP_OK) {
        return false;
    }

    int16_t raw_ax = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t raw_ay = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t raw_az = (int16_t)((buf[4] << 8) | buf[5]);
    int16_t raw_t  = (int16_t)((buf[6] << 8) | buf[7]);
    int16_t raw_gx = (int16_t)((buf[8] << 8) | buf[9]);
    int16_t raw_gy = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t raw_gz = (int16_t)((buf[12] << 8) | buf[13]);

    *ax = (raw_ax / ACCEL_FS_8G_LSB_PER_G) * GRAVITY_MPS2;
    *ay = (raw_ay / ACCEL_FS_8G_LSB_PER_G) * GRAVITY_MPS2;
    *az = (raw_az / ACCEL_FS_8G_LSB_PER_G) * GRAVITY_MPS2;
    *temp_c = (raw_t / 340.0f) + 36.53f;

    /* gyro at +-500 dps -> 65.5 LSB/(deg/s); convert to rad/s */
    const float dps_to_rads = 3.14159265f / 180.0f;
    *gx = (raw_gx / 65.5f) * dps_to_rads;
    *gy = (raw_gy / 65.5f) * dps_to_rads;
    *gz = (raw_gz / 65.5f) * dps_to_rads;
    return true;
}

static void reset_motion_window(uint32_t now_ms)
{
    s_motion_window_start_ms = now_ms;
    s_motion_events = 0;
    s_motion_peak_delta = 0.0f;
    s_motion_peak_gyro = 0.0f;
    s_motion_peak_temp_c = NAN;
    s_motion_prev = false;
}

static void check_overheat(uint32_t now_ms, float temp_c)
{
    char msg[128];
    if (isnan(temp_c)) {
        return;
    }

    if (!s_overheat_latched && temp_c >= MPU_OVERHEAT_TEMP_C &&
        (s_last_overheat_ms == 0 ||
         (now_ms - s_last_overheat_ms) >= OVERHEAT_ALERT_COOLDOWN_MS)) {
        snprintf(msg, sizeof(msg),
                 "WARNING: MPU6050 temperature high (%.1f C)", temp_c);
        ESP_LOGW(TAG, "%s", msg);
        sf_locator_telegram_send(msg);
        s_overheat_latched = true;
        s_last_overheat_ms = now_ms;
    } else if (s_overheat_latched && temp_c <= MPU_RECOVER_TEMP_C) {
        snprintf(msg, sizeof(msg),
                 "MPU6050 temperature recovered (%.1f C <= %.1f C)",
                 temp_c, MPU_RECOVER_TEMP_C);
        ESP_LOGI(TAG, "%s", msg);
        sf_locator_telegram_send(msg);
        s_overheat_latched = false;
    }
}

static void notify_motion_window(uint32_t now_ms)
{
    char msg[224];
    if (s_motion_window_start_ms == 0) {
        return;
    }
    if ((now_ms - s_motion_window_start_ms) < MOTION_WINDOW_MS) {
        return;
    }
    if (s_motion_events > 0) {
        if (isnan(s_motion_peak_temp_c)) {
            snprintf(msg, sizeof(msg),
                     "Motion summary (last 1 min): %u events | peak delta %.2f m/s^2 | peak gyro %.2f rad/s",
                     s_motion_events, s_motion_peak_delta, s_motion_peak_gyro);
        } else {
            snprintf(msg, sizeof(msg),
                     "Motion summary (last 1 min): %u events | peak delta %.2f m/s^2 | peak gyro %.2f rad/s | peak temp %.1f C",
                     s_motion_events, s_motion_peak_delta, s_motion_peak_gyro, s_motion_peak_temp_c);
        }
        ESP_LOGI(TAG, "%s", msg);
        sf_locator_telegram_send(msg);
    }
    reset_motion_window(now_ms);
}

sf_err_t sf_locator_mpu_init(void)
{
    sf_err_t status = SF_FAIL;
    esp_err_t err;

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port   = I2C_PORT,
        .sda_io_num = SF_LOCATOR_I2C_SDA_GPIO,
        .scl_io_num = SF_LOCATOR_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = 1 },
    };

    err = i2c_new_master_bus(&bus_cfg, &s_bus);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end, "i2c_new_master_bus with status: %d", err);

    if (!probe_addr(MPU_ADDR_PRIMARY)) {
        ESP_LOGW(TAG, "MPU6050 not at 0x%02X, trying 0x%02X", MPU_ADDR_PRIMARY, MPU_ADDR_ALT);
        if (!probe_addr(MPU_ADDR_ALT)) {
            ESP_LOGE(TAG, "MPU6050 not detected on bus");
            goto end;
        }
    }

    err = mpu_configure();
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end, "mpu_configure with status: %d", err);

    s_ready = true;
    ESP_LOGI(TAG, "MPU6050 ready at 0x%02X", s_addr);

    sf_locator_mpu_calibrate(0);

    status = SF_OK;
end:
    return status;
}

void sf_locator_mpu_tick(uint32_t now_ms)
{
    float ax, ay, az, gx, gy, gz, temp_c;
    float delta_from_rest;
    float motion_delta;
    float gyro_peak_axis;
    bool lifted;
    bool moving;
    char msg[96];

    if (!s_ready) {
        return;
    }
    if ((now_ms - s_last_read_ms) < MPU_READ_INTERVAL_MS) {
        return;
    }
    s_last_read_ms = now_ms;

    if (s_motion_window_start_ms == 0) {
        s_motion_window_start_ms = now_ms;
    }

    if (!read_sample(&ax, &ay, &az, &gx, &gy, &gz, &temp_c)) {
        ESP_LOGW(TAG, "MPU sample read failed");
        return;
    }
    s_last_temp_c = temp_c;

    delta_from_rest = s_baseline_az - az;
    motion_delta = fabsf(delta_from_rest);
    gyro_peak_axis = fmaxf(fabsf(gx), fmaxf(fabsf(gy), fabsf(gz)));
    lifted = (delta_from_rest > LIFT_DELTA_THRESHOLD);
    moving = (motion_delta > MOTION_DELTA_THRESHOLD);

    if (moving && !s_motion_prev) {
        s_motion_events++;
    }
    s_motion_prev = moving;

    if (moving) {
        s_motion_peak_delta = fmaxf(s_motion_peak_delta, motion_delta);
        s_motion_peak_gyro = fmaxf(s_motion_peak_gyro, gyro_peak_axis);
        if (isnan(s_motion_peak_temp_c) || temp_c > s_motion_peak_temp_c) {
            s_motion_peak_temp_c = temp_c;
        }
    }

    if ((now_ms - s_last_lift_debug_ms) > LIFT_DEBUG_INTERVAL_MS) {
        s_last_lift_debug_ms = now_ms;
        ESP_LOGD(TAG, "az=%.2f baseline=%.2f delta=%.2f lifted=%d moving=%d",
                 az, s_baseline_az, delta_from_rest, lifted ? 1 : 0, moving ? 1 : 0);
    }

    if (lifted && !s_lift_latched &&
        (now_ms - s_last_lift_event_ms) > LIFT_EVENT_COOLDOWN_MS) {
        s_last_lift_event_ms = now_ms;
        snprintf(msg, sizeof(msg), "Lift detected: device was picked up");
        ESP_LOGI(TAG, "%s", msg);
        sf_locator_telegram_send(msg);
    }

    check_overheat(now_ms, temp_c);
    s_lift_latched = lifted;

    notify_motion_window(now_ms);
}

sf_err_t sf_locator_mpu_calibrate(uint32_t now_ms)
{
    sf_err_t status = SF_FAIL;
    float sum_az = 0.0f;
    float ax, ay, az, gx, gy, gz, temp_c;
    int i;
    int samples_read = 0;
    char msg[96];

    if (!s_ready) {
        ESP_LOGW(TAG, "Calibration requested but MPU not ready");
        goto end;
    }

    ESP_LOGI(TAG, "Calibration started - keep device still...");

    for (i = 0; i < MPU_CALIBRATION_SAMPLES; i++) {
        if (!read_sample(&ax, &ay, &az, &gx, &gy, &gz, &temp_c)) {
            continue;
        }
        sum_az += az;
        samples_read++;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, samples_read == 0, end,
                       "Calibration read 0 samples with status: %d", samples_read);

    float old_baseline = s_baseline_az;
    s_baseline_az = sum_az / (float)samples_read;
    s_lift_latched = false;
    s_last_lift_event_ms = now_ms;
    s_last_lift_debug_ms = 0;
    reset_motion_window(0);

    snprintf(msg, sizeof(msg),
             "MPU6050 calibrated. Baseline az=%.2f m/s^2", s_baseline_az);
    ESP_LOGI(TAG, "%s (was %.2f)", msg, old_baseline);
    sf_locator_telegram_send(msg);

    status = SF_OK;
end:
    return status;
}

void sf_locator_mpu_get_status(sf_mpu_status_t *out)
{
    if (out == NULL) {
        return;
    }
    out->ready = s_ready;
    out->baseline_az = s_baseline_az;
    out->last_temperature_c = s_last_temp_c;
}
