#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/structs/timer.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"
#include <bsp/board_api.h>
#include <pico/stdio.h>
#include <tusb.h>

#include "ppm.pio.h"

#define LED_TIME 500
#define MAX_CODE 1024
#define SYS_FREQ 133000
// #define SYS_FREQ 250000
class PPMController {
public:
  static constexpr float MIN_PULSE_PERIOD_US = 3.0f;    // 3.0 microseconds
  static constexpr float PIO_FREQ = SYS_FREQ * 1000.0f; // 133000000.0f;
  static constexpr uint16_t MIN_INTERVAL_CYCLES =
      MIN_PULSE_PERIOD_US * (SYS_FREQ / 1000);
  static constexpr float AUDIO_SAMPLE_RATE = 48000.0f; // 50 kHz
  static constexpr uint32_t AUDIO_FRAME_TICKS =
      SYS_FREQ * 10.0 / AUDIO_SAMPLE_RATE;

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
  uint32_t audio_sample_ticks;

public:
  PPMController()
      : pio(nullptr), sm(0), currentCode(0), testMode(false), testDirection(1),
        testOutputCounter(0), testUpdateCounter(0),
        testUpdatePeriodSeconds(0.001f) {}

  void init() {
    pio = pio0;
    sm = 0;
    uint offset = pio_add_program(pio, &ppm_program);
    ppm_program_init(pio, sm, offset, PPM_PIN, PIO_FREQ);
  }

  void sendCode(uint16_t code) {
    if (code > MAX_CODE)
      code = MAX_CODE;
    currentCode = code;
  }

  void test_mode_update() {
    static absolute_time_t next_test_update_time = {0};
    if (testMode) {
      if (absolute_time_diff_us(get_absolute_time(), next_test_update_time) <=
          0) {

        currentCode += testDirection;

        if (currentCode >= MAX_CODE - 1) {
          currentCode = MAX_CODE - 1;
          testDirection = -1;
        } else if (currentCode <= 1) {
          currentCode = 1;
          testDirection = 1;
        }
        int update_ms = (int)(testUpdatePeriodSeconds * 1000.0f);
        next_test_update_time = make_timeout_time_ms(update_ms);
      }
    } else {
      next_test_update_time = get_absolute_time();
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

// Глобальные переменные для таймера
alarm_id_t audio_timer_id = 0;

// PIO и SM для импульсов
PIO ppm_pio = nullptr;
uint ppm_sm = 0;

// Указатель на PPMController для использования в прерываниях
PPMController *ppm_controller = nullptr;

// Функция отправки значения в PIO (передаем текущий код задержки)
void send_ppm_value(uint32_t value) {
  if (ppm_pio != nullptr) {
    pio_sm_put_blocking(ppm_pio, ppm_sm, value);
  }
}

void timer0_irq_handler() {
  if (timer_hw->intr & (1u << 0)) {
    timer_hw->intr = 1u << 0;

    uint32_t delay_value = PPMController::MIN_INTERVAL_CYCLES;
    if (ppm_controller) {
      delay_value += ppm_controller->getCurrentCode();
    }
    send_ppm_value(delay_value);
    timer_hw->alarm[0] = timer_hw->timerawl + PPMController::AUDIO_FRAME_TICKS;
  }
}

int main() {
  set_sys_clock_khz(SYS_FREQ, true);
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
  ppm_controller = &ppmCtrl; // Сохраняем для использования в прерываниях

  // Сохраняем PIO и SM для использования в прерываниях
  ppm_pio = pio0;
  ppm_sm = 0;

  ppmCtrl.sendCode(0);

  irq_set_exclusive_handler(TIMER_IRQ_0, timer0_irq_handler);

  hw_set_bits(&timer_hw->inte, (1u << 0));
  irq_set_enabled(TIMER_IRQ_0, true);

  timer_hw->alarm[0] = timer_hw->timerawl + PPMController::AUDIO_FRAME_TICKS;

  std::string command_buffer;

  while (true) {
    tud_task();
    ppmCtrl.test_mode_update();

    if (absolute_time_diff_us(get_absolute_time(), next_led_toggle_time) <= 0) {
      led_state = !led_state;
      gpio_put(LED_PIN, led_state);
      next_led_toggle_time = make_timeout_time_ms(LED_TIME);
    }

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