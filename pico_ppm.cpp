#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"
#include <bsp/board_api.h>
#include <pico/stdio.h>
#include <tusb.h>

#include "ppm.pio.h"

#define LED_TIME 500
#define MAX_CODE 1024
class PPMController {
public:
  static constexpr uint16_t MIN_PULSE_PERIOD_US = 3;
  static constexpr float PIO_FREQ = 133000000.0f;
  static constexpr uint16_t MIN_INTERVAL_CYCLES = MIN_PULSE_PERIOD_US * 133;
  static constexpr uint32_t AUDIO_SAMPLE_RATE = 50000;
  static constexpr uint32_t AUDIO_FRAME_TIME_US = 1000000 / AUDIO_SAMPLE_RATE;

private:
  static constexpr uint8_t PPM_PIN = 0;

  PIO pio;
  uint sm;
  uint16_t currentCode;
  bool testMode;
  int8_t testDirection;
  uint32_t testOutputCounter;
  uint32_t testUpdateCounter;
  float testUpdatePeriodSeconds;

public:
  PPMController()
      : pio(nullptr), sm(0), currentCode(0), testMode(false), testDirection(1),
        testOutputCounter(0), testUpdateCounter(0),
        testUpdatePeriodSeconds(0.1f) {}

  void init() {
    pio = pio0;
    sm = 0;
    uint offset = pio_add_program(pio, &ppm_program);
    ppm_program_init(pio, sm, offset, PPM_PIN, PIO_FREQ);
  }

  // Теперь просто сохраняем код, вычисления задержки вне класса
  void sendCode(uint16_t code) {
    if (code > MAX_CODE)
      code = MAX_CODE;
    currentCode = code;
  }

  // Переименовано: update -> test_mode_update
  void test_mode_update() {
    static absolute_time_t next_test_update_time = {0};
    if (testMode) {
      if (absolute_time_diff_us(get_absolute_time(), next_test_update_time) <=
          0) {
        currentCode += testDirection;
        if (currentCode >= MAX_CODE - 1) {
          testDirection = -1;
        } else if (currentCode <= 1) {
          testDirection = 1;
        }
        next_test_update_time =
            make_timeout_time_ms((int)(testUpdatePeriodSeconds * 1000));
      }
    }
  }

  bool parseCommand(const std::string &cmd, uint16_t &code) {
    // Команда тест (T/t)
    if (cmd.length() == 1 && (cmd[0] == 'T' || cmd[0] == 't')) {
      testMode = !testMode;

      if (testMode) {
        // При включении тестового режима сбросить счетчики
        currentCode = 0;
        testDirection = 1;
        testUpdateCounter = 0;
      }

      code = testMode ? 1 : 0;
      return true;
    }

    // Команда установки периода обновления в секундах (P:число или p:число)
    if (cmd.length() >= 3 && (cmd[0] == 'P' || cmd[0] == 'p') &&
        cmd[1] == ':') {
      try {
        float period = std::stof(cmd.substr(2));
        setTestUpdatePeriod(period);
        code = 0;
        return true;
      } catch (...) {
        return false;
      }
    }

    // Обработка команды кода (C:число или c:число)
    if (cmd.length() >= 3 && (cmd[0] == 'C' || cmd[0] == 'c') &&
        cmd[1] == ':') {
      try {
        code = std::stoi(cmd.substr(2));
        return true;
      } catch (...) {
        return false;
      }
    }

    return false;
  }

  void setTestUpdatePeriod(float seconds) {
    if (seconds > 0.01f) {
      testUpdatePeriodSeconds = seconds;
      testUpdateCounter = 0;
    }
  }

  bool isTestMode() const { return testMode; }
  uint16_t getCurrentCode() const { return currentCode; }
  float getTestUpdatePeriod() const { return testUpdatePeriodSeconds; }
};

// Глобальные переменные для таймеров
alarm_id_t audio_timer_id = 0;
alarm_id_t delay_timer_id = 0;
volatile uint32_t next_delay_ticks = 0;

// PIO и SM для импульсов
PIO ppm_pio = nullptr;
uint ppm_sm = 0;

// функция формирования короткого импульса через PIO
void ppm_pulse() { send_ppm_pulse(ppm_pio, ppm_sm); }

int64_t delay_timer_callback(alarm_id_t id, void *user_data) {
  // ppm_pulse();
  return 0;
}

int64_t audio_timer_callback(alarm_id_t id, void *user_data) {
  ppm_pulse();
  if (next_delay_ticks > 0) {
    delay_timer_id =
        add_alarm_in_us(next_delay_ticks, delay_timer_callback, NULL, false);
  }
  return PPMController::AUDIO_FRAME_TIME_US;
}

int main() {
  set_sys_clock_khz(133000, true);
  board_init();
  tusb_init();

  const uint LED_PIN = PICO_DEFAULT_LED_PIN;
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  static uint8_t led_state = 0;
  absolute_time_t next_led_toggle_time = make_timeout_time_ms(LED_TIME);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }
  stdio_init_all();

  PPMController ppmCtrl;
  ppmCtrl.init();

  // Сохраняем PIO и SM для использования в прерываниях
  ppm_pio = pio0;
  ppm_sm = 0;

  ppmCtrl.sendCode(0);

  audio_timer_id = add_alarm_in_us(1000000 / PPMController::AUDIO_SAMPLE_RATE,
                                   audio_timer_callback, NULL, true);

  std::string command_buffer;

  while (true) {
    tud_task();
    ppmCtrl.test_mode_update();

    if (absolute_time_diff_us(get_absolute_time(), next_led_toggle_time) <= 0) {
      led_state = !led_state;
      gpio_put(LED_PIN, led_state);
      next_led_toggle_time = make_timeout_time_ms(LED_TIME);
    }

    uint32_t delay_ticks = 0;
    {
      uint16_t code = ppmCtrl.getCurrentCode();
      delay_ticks = PPMController::MIN_INTERVAL_CYCLES + code;
    }
    next_delay_ticks = delay_ticks;

    if (tud_cdc_connected()) {
      if (tud_cdc_available()) {
        uint8_t buf[64];
        uint32_t count = tud_cdc_read(buf, sizeof(buf));

        if (count > 0) {
          // Эхо данных
          tud_cdc_write(buf, count);
          tud_cdc_write_flush();

          // Обработка входных символов
          for (uint32_t i = 0; i < count; i++) {
            char c = static_cast<char>(buf[i]);

            // Проверяем, нажат ли Enter
            if (c == '\r' || c == '\n') {
              // Обрабатываем команду, если буфер не пустой
              if (!command_buffer.empty()) {
                uint16_t code;
                if (ppmCtrl.parseCommand(command_buffer, code)) {
                  // Проверяем тип команды
                  if (command_buffer[0] == 'T' || command_buffer[0] == 't') {
                    // Команда режима тестирования
                    std::string mode =
                        ppmCtrl.isTestMode() ? "включен" : "выключен";
                    std::string response =
                        "\r\nРежим тестирования " + mode + "\r\n";
                    tud_cdc_write(response.c_str(), response.length());
                    tud_cdc_write_flush();
                  } else if (command_buffer[0] == 'P' ||
                             command_buffer[0] == 'p') {
                    // Команда установки периода обновления
                    std::string response =
                        "\r\nПериод обновления установлен: " +
                        std::to_string(ppmCtrl.getTestUpdatePeriod()) +
                        " сек\r\n";
                    tud_cdc_write(response.c_str(), response.length());
                    tud_cdc_write_flush();
                  } else {
                    // Обычная команда кода
                    ppmCtrl.sendCode(code);
                    std::string response =
                        "\r\nPPM code sent: " + std::to_string(code) + "\r\n";
                    tud_cdc_write(response.c_str(), response.length());
                    tud_cdc_write_flush();
                  }
                } else {
                  // Если команда не распознана
                  std::string error =
                      "\r\nНераспознанная команда: " + command_buffer + "\r\n";
                  tud_cdc_write(error.c_str(), error.length());
                  tud_cdc_write_flush();
                }

                // Очищаем буфер после обработки команды
                command_buffer.clear();
              }
            } else if (c == 127 || c == 8) {
              // Обработка Backspace или Delete
              if (!command_buffer.empty()) {
                command_buffer.pop_back();
              }
            } else {
              // Добавляем символ в буфер
              command_buffer.push_back(c);
            }
          }
        }
      }
    } else {
      // Небольшая пауза при отсутствии подключения
      sleep_ms(10);
      command_buffer.clear(); // Очистить буфер, если соединение пропало
    }
  }

  return 0;
}