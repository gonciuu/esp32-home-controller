#include "bsp.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_check.h"

#define LCD_H_RES 800
#define LCD_V_RES 480

#define I2C_SDA_GPIO   8
#define I2C_SCL_GPIO   9
#define TOUCH_INT_GPIO 4

/* CH422G IO expander: each I2C address acts as a register and takes one data byte.
 *   0x24 = config (0x01 enables push-pull output on IO0..IO7)
 *   0x38 = output levels for IO0..IO7
 * On this board EXIO1 = touch reset, EXIO2 = LCD backlight. */
#define CH422G_ADDR_CONFIG 0x24
#define CH422G_ADDR_OUTPUT 0x38
#define CH422G_TP_RST_BIT  (1 << 1)
#define CH422G_BL_BIT      (1 << 2)

static const char *TAG = "bsp";
static i2c_master_bus_handle_t s_i2c_bus;

static esp_err_t i2c_init(void)
{
    i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&cfg, &s_i2c_bus);
}

static esp_err_t ch422g_write(uint8_t addr, uint8_t value)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t dev;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &dev), TAG, "add dev");
    esp_err_t err = i2c_master_transmit(dev, &value, 1, 1000);
    i2c_master_bus_rm_device(dev);
    return err;
}

/* Leaves touch held in reset; gt911_reset() releases it with the right timing. */
static esp_err_t backlight_on(void)
{
    ESP_RETURN_ON_ERROR(ch422g_write(CH422G_ADDR_CONFIG, 0x01), TAG, "ch422g cfg");
    return ch422g_write(CH422G_ADDR_OUTPUT, CH422G_BL_BIT);
}

/* The GT911 samples INT on the rising edge of RST to choose its I2C address:
 * INT low -> 0x5D, INT high -> 0x14.
 *
 * esp_lcd_touch_gt911 does this itself, but only when rst_gpio_num is a real
 * GPIO (see the guard at the top of esp_lcd_touch_gt911_init). Ours is behind
 * the CH422G expander and therefore GPIO_NUM_NC, so the driver skips the whole
 * sequence and we have to drive it here — otherwise the address is whatever the
 * pin happened to float to at power-on and every I2C read fails silently. */
static esp_err_t gt911_reset(void)
{
    const gpio_config_t int_output = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT64(TOUCH_INT_GPIO),
    };
    ESP_RETURN_ON_ERROR(gpio_config(&int_output), TAG, "int as output");
    ESP_RETURN_ON_ERROR(gpio_set_level(TOUCH_INT_GPIO, 0), TAG, "int low");

    ESP_RETURN_ON_ERROR(ch422g_write(CH422G_ADDR_OUTPUT, CH422G_BL_BIT), TAG, "tp rst low");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(ch422g_write(CH422G_ADDR_OUTPUT, CH422G_BL_BIT | CH422G_TP_RST_BIT),
                        TAG, "tp rst high");

    /* INT must stay held for >5ms past the rising edge for the latch to take. */
    vTaskDelay(pdMS_TO_TICKS(10));

    const gpio_config_t int_input = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = BIT64(TOUCH_INT_GPIO),
    };
    ESP_RETURN_ON_ERROR(gpio_config(&int_input), TAG, "int as input");

    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

static esp_err_t gt911_probe_address(uint8_t *out_addr)
{
    const uint8_t candidates[] = {
        ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
        ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP,
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (i2c_master_probe(s_i2c_bus, candidates[i], 100) == ESP_OK) {
            *out_addr = candidates[i];
            ESP_LOGI(TAG, "GT911 found at 0x%02X", candidates[i]);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "GT911 did not answer at 0x%02X or 0x%02X",
             ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP);
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t rgb_panel_init(esp_lcd_panel_handle_t *out_panel)
{
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_width = 16,
        .num_fbs = 1,
        .bounce_buffer_size_px = 10 * LCD_H_RES,
        .dma_burst_size = 64,
        .hsync_gpio_num = 46,
        .vsync_gpio_num = 3,
        .de_gpio_num = 5,
        .pclk_gpio_num = 7,
        .disp_gpio_num = -1,
        .data_gpio_nums = {
            14, 38, 18, 17, 10,        /* B3..B7 */
            39,  0, 45, 48, 47, 21,    /* G2..G7 */
             1,  2, 42, 41, 40,        /* R3..R7 */
        },
        .timings = {
            .pclk_hz = 16 * 1000 * 1000,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .hsync_pulse_width = 4,
            .vsync_back_porch = 8,
            .vsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .flags.pclk_active_neg = true,
        },
        .flags.fb_in_psram = true,
    };

    esp_lcd_panel_handle_t panel;
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&cfg, &panel), TAG, "new rgb panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "panel init");

    *out_panel = panel;
    return ESP_OK;
}

static esp_err_t touch_init(esp_lcd_touch_handle_t *out_touch)
{
    ESP_RETURN_ON_ERROR(gt911_reset(), TAG, "gt911 reset");

    uint8_t addr;
    ESP_RETURN_ON_ERROR(gt911_probe_address(&addr), TAG, "gt911 probe");

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.dev_addr = addr;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &io_cfg, &io), TAG, "touch io");

    esp_lcd_touch_config_t cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,    /* reset is behind the CH422G */
        .int_gpio_num = TOUCH_INT_GPIO,
    };
    return esp_lcd_touch_new_i2c_gt911(io, &cfg, out_touch);
}

static lv_display_t *lvgl_init(esp_lcd_panel_handle_t panel, esp_lcd_touch_handle_t touch)
{
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = panel,
        .buffer_size = LCD_H_RES * 100,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .flags = { .buff_spiram = true },
    };
    lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = { .bb_mode = true },
    };
    lv_display_t *disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);

    lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = touch,
    };
    lvgl_port_add_touch(&touch_cfg);
    return disp;
}

esp_err_t bsp_display_start(void)
{
    esp_lcd_panel_handle_t panel;
    esp_lcd_touch_handle_t touch;

    ESP_RETURN_ON_ERROR(i2c_init(), TAG, "i2c");
    ESP_RETURN_ON_ERROR(backlight_on(), TAG, "backlight");
    ESP_RETURN_ON_ERROR(rgb_panel_init(&panel), TAG, "rgb");
    ESP_RETURN_ON_ERROR(touch_init(&touch), TAG, "touch");
    lvgl_init(panel, touch);

    ESP_LOGI(TAG, "display ready (%dx%d)", LCD_H_RES, LCD_V_RES);
    return ESP_OK;
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    lvgl_port_unlock();
}

esp_err_t bsp_display_backlight(bool on)
{
    return ch422g_write(CH422G_ADDR_OUTPUT, CH422G_TP_RST_BIT | (on ? CH422G_BL_BIT : 0));
}
