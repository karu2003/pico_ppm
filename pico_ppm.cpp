#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include <bsp/board_api.h>
#include <pico/stdio.h>
#include <tusb.h>

// Подключаем сгенерированный заголовочный файл для PIO
#include "ppm.pio.h"

class PPMController {
public:
  static constexpr uint16_t FRAME_TIME_US = 15; // Длительность фрейма 15 мкс
  static constexpr uint16_t MIN_PULSE_PERIOD_US = 3; // минимальный период между импульсами (3 мкс)
private:
  // Константы
  static constexpr uint8_t PPM_PIN = 0; // GPIO пин для выхода PPM
  static constexpr float PIO_FREQ =
      133000000.0f; // Максимальная частота 133 МГц для RP2040
  static constexpr uint16_t MIN_INTERVAL_CYCLES = MIN_PULSE_PERIOD_US * 133; // ~3 мкс при 133 МГц
  static constexpr uint16_t IMPULSE_OVERHEAD_CYCLES =
      20; // Примерно для импульсов и накладных расходов

  // Переменные для PIO
  PIO pio;
  uint sm;
  uint16_t currentCode;          // Текущее значение кода
  absolute_time_t nextFrameTime; // Время следующего фрейма

  // Масштабирование кода для вписывания в 15 мкс фрейм
  uint16_t scaleCodeForFrame(uint16_t code) {
    // Максимальное количество циклов для кода при 15 мкс фрейме
    // (15 мкс * 133 МГц) - (2 * MIN_INTERVAL_CYCLES) - IMPULSE_OVERHEAD_CYCLES
    static constexpr uint32_t MAX_CODE_CYCLES = (FRAME_TIME_US * 133) -
                                                (2 * MIN_INTERVAL_CYCLES) -
                                                IMPULSE_OVERHEAD_CYCLES;

    // Новый диапазон: от MIN_INTERVAL_CYCLES до MIN_INTERVAL_CYCLES + MAX_CODE_CYCLES
    return MIN_INTERVAL_CYCLES + ((code * MAX_CODE_CYCLES) / 1024);
  }

public:
  PPMController() : pio(nullptr), sm(0), currentCode(0) {
    // Инициализация времени для регулярной отправки фреймов
    nextFrameTime = get_absolute_time();
  }

  // Инициализация PIO для PPM
  void init() {
    pio = pio0;
    sm = 0;

    uint offset = pio_add_program(pio, &ppm_program);
    ppm_program_init(pio, sm, offset, PPM_PIN, PIO_FREQ);
  }

  // Отправка кода в PPM
  void sendCode(uint16_t code) {
    if (code > 1024)
      code = 1024; // Ограничение до 1024

    currentCode = code;

    // Масштабируем код для 15 мкс фрейма и отправляем
    uint16_t scaledCode = scaleCodeForFrame(code);
    send_ppm_code(pio, sm, scaledCode);
  }

  // Метод для отправки фреймов с регулярным интервалом
  void update() {
    // Отправка фрейма каждые 15 мкс
    if (absolute_time_diff_us(get_absolute_time(), nextFrameTime) <= 0) {
      // Отправляем текущий код
      uint16_t scaledCode = scaleCodeForFrame(currentCode);
      send_ppm_code(pio, sm, scaledCode);

      // Устанавливаем время следующего фрейма
      nextFrameTime = make_timeout_time_us(FRAME_TIME_US);
    }
  }

  // Парсинг входной команды остаётся без изменений
  bool parseCommand(const std::string &cmd, uint16_t &code) {
    // Простой формат "C:123" - где C это команда для кода, 123 - значение
    if (cmd.length() >= 3 && cmd[0] == 'C' && cmd[1] == ':') {
      try {
        code = std::stoi(cmd.substr(2));
        return true;
      } catch (...) {
        return false;
      }
    }
    return false;
  }

public:
  uint16_t getCurrentCode() const { return currentCode; }
};

#define LED_TIME 500

int main() {
  // Установить системную частоту 133 МГц для корректной работы PIO
  set_sys_clock_khz(125000, true); //133000

  // Initialize TinyUSB stack
  board_init();
  tusb_init();

  const uint LED_PIN = PICO_DEFAULT_LED_PIN;
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  static uint8_t led_state = 0;
  absolute_time_t next_led_toggle_time = make_timeout_time_ms(LED_TIME);

  // TinyUSB board init callback after init
  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  // let pico sdk use the CDC interface for std io
  stdio_init_all();

//   // Тестирование GPIO 0 напрямую
//   gpio_init(0);              // Инициализируем GPIO 0
//   gpio_set_dir(0, GPIO_OUT); // Настраиваем как выход

//   // Тест пина - мигаем несколько раз
//   for (int i = 0; i < 10; i++) {
//     gpio_put(0, 1);
//     sleep_ms(100);
//     gpio_put(0, 0);
//     sleep_ms(100);
//   }

  // Создаем объект контроллера PPM
  PPMController ppmCtrl;
  ppmCtrl.init();

  // Отправляем начальный код (нулевой)
  ppmCtrl.sendCode(0);

  // main run loop
  absolute_time_t next_frame_time = make_timeout_time_us(PPMController::FRAME_TIME_US);

  while (true) {
    tud_task(); // USB

    // Отправка PPM фрейма каждые 15 мкс
    if (absolute_time_diff_us(get_absolute_time(), next_frame_time) <= 0) {
        ppmCtrl.sendCode(ppmCtrl.getCurrentCode());
        next_frame_time = make_timeout_time_us(PPMController::FRAME_TIME_US);
    }

    // Остальной код без изменений (светодиод, USB CDC)
    // Неблокирующее мигание светодиодом
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
          buf[count] = 0; // Null-терминатор

          // Эхо данных
          tud_cdc_write(buf, count);
          tud_cdc_write_flush();

          // Парсим команду
          uint16_t code;
          std::string cmdStr(reinterpret_cast<char *>(buf));
          if (ppmCtrl.parseCommand(cmdStr, code)) {
            ppmCtrl.sendCode(code);

            // Подтверждение отправки кода
            std::string response =
                "PPM code sent: " + std::to_string(code) + "\r\n";
            tud_cdc_write(response.c_str(), response.length());
            tud_cdc_write_flush();
          }
        }
      }
    } else {
      // Небольшая пауза при отсутствии подключения
      sleep_ms(10);
    }
  }

  return 0;
}