#pragma once
#include <driver/i2c.h>
#include <driver/gpio.h>
#include <esp_err.h>

// Own only PORT A's controller. M5Unified retains the other controller.
class ExternalI2C {
 public:
  i2c_port_t port = I2C_NUM_0;
  esp_err_t initError = ESP_ERR_INVALID_STATE;
  bool installed = false;
  uint32_t clock = 0;
  esp_err_t configure(uint32_t hz) {
    i2c_config_t config{};
    config.mode = I2C_MODE_MASTER;
    config.sda_io_num = GPIO_NUM_2;
    config.scl_io_num = GPIO_NUM_1;
    config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    config.master.clk_speed = hz;
    const esp_err_t err = i2c_param_config(port, &config);
    if (err == ESP_OK) clock = hz;
    return err;
  }
  esp_err_t begin(i2c_port_t externalPort, uint32_t hz) {
    port = externalPort;
    initError = configure(hz);
    if (initError == ESP_OK)
      initError = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
    installed = initError == ESP_OK;
    return initError;
  }
  esp_err_t read(uint8_t address, uint8_t* bytes, size_t length, uint32_t hz) {
    if (!installed) return initError;
    if (hz != clock) {
      const esp_err_t e = configure(hz);
      if (e != ESP_OK) return e;
    }
    return i2c_master_read_from_device(port, address, bytes, length, pdMS_TO_TICKS(25));
  }
  esp_err_t probe(uint8_t address, uint32_t hz) {
    if (!installed) return initError;
    if (hz != clock) {
      const esp_err_t e = configure(hz);
      if (e != ESP_OK) return e;
    }
    auto cmd = i2c_cmd_link_create();
    if (!cmd) return ESP_ERR_NO_MEM;
    esp_err_t e = i2c_master_start(cmd);
    if (e == ESP_OK) e = i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    if (e == ESP_OK) e = i2c_master_stop(cmd);
    // Only execution failure ESP_FAIL means missing ACK. Command construction
    // failures must not be mislabeled as a device NACK.
    if (e != ESP_OK) { i2c_cmd_link_delete(cmd); return ESP_ERR_NO_MEM; }
    e = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(25));
    i2c_cmd_link_delete(cmd);
    return e;
  }
  static int sda() { return gpio_get_level(GPIO_NUM_2); }
  static int scl() { return gpio_get_level(GPIO_NUM_1); }
  static const char* outcome(esp_err_t e) {
    if (e == ESP_OK) return "ACK";
    if (e == ESP_FAIL) return "NO_ACK";
    if (e == ESP_ERR_TIMEOUT) return "TIMEOUT";
    return esp_err_to_name(e);
  }
};
