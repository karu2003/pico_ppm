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
  static constexpr uint16_t FRAME_TIME_US = 15;
  // Константы с компенсацией накладных расходов
  static constexpr uint16_t MIN_PULSE_PERIOD_US = 3;
  static constexpr float PIO_FREQ = 133000000.0f;
  // Добавляем компенсацию ~42-45 циклов 
  static constexpr uint16_t MIN_INTERVAL_CYCLES = (MIN_PULSE_PERIOD_US * 133) + 45;
  static constexpr uint32_t AUDIO_SAMPLE_RATE = 48000; // 48 кГц для тестового режима
  static constexpr uint32_t AUDIO_FRAME_TIME_US = 1000000 / AUDIO_SAMPLE_RATE; // ~20.83 мкс
private:
  // Константы
  static constexpr uint8_t PPM_PIN = 0; // GPIO пин для выхода PPM
  static constexpr uint16_t IMPULSE_CYCLES = 4; // 4 цикла на два импульса (2 nop для каждого)

  // Переменные для PIO
  PIO pio;
  uint sm;
  uint16_t currentCode;          // Текущее значение кода
  absolute_time_t nextFrameTime; // Время следующего фрейма
  bool testMode;                 // Режим тестирования
  int8_t testDirection;          // Направление изменения в тестовом режиме (1 или -1)
  uint32_t testOutputCounter;    // Счетчик для периодического вывода статистики

  // Масштабирование кода для вписывания в фрейм в обычном режиме
  uint16_t scaleCodeForFrame(uint16_t code) {
    // Максимальное количество циклов для кода при 15 мкс фрейме
    static constexpr uint32_t MAX_CODE_CYCLES = (FRAME_TIME_US * 133) - 
                                                (MIN_INTERVAL_CYCLES);

    // Линейное масштабирование от 0 до MAX_CODE_CYCLES
    return ((uint32_t)code * MAX_CODE_CYCLES) / 1024;
  }

  // Расчет циклов для режима тестирования (аудио 48 кГц)
  uint16_t calculateTestModeDelay(uint16_t code) {
    // Общее число циклов на один фрейм при 48 кГц
    static constexpr uint32_t CYCLES_PER_SAMPLE = PIO_FREQ / AUDIO_SAMPLE_RATE;
    
    // Базовая минимальная задержка
    uint32_t base_cycles = MIN_INTERVAL_CYCLES;
    
    // Вычисляем задержку для текущего кода, используя оставшиеся циклы
    // после вычета минимальных задержек и импульсов
    uint32_t available_cycles = CYCLES_PER_SAMPLE - (MIN_INTERVAL_CYCLES * 2) - IMPULSE_CYCLES;
    
    // Масштабируем код в диапазон доступных циклов
    return (uint32_t)((float)code / 1024 * available_cycles);
  }

public:
  PPMController() : pio(nullptr), sm(0), currentCode(0), testMode(false), 
                    testDirection(1), testOutputCounter(0) {
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

    // Расчет минимальной задержки
    uint32_t min_gap_cycles = MIN_INTERVAL_CYCLES;
    uint32_t code_cycles;

    if (testMode) {
      // В режиме тестирования используем логику из audio_ppm.c
      code_cycles = calculateTestModeDelay(code) + min_gap_cycles;
    } else {
      // В обычном режиме используем стандартную логику
      code_cycles = scaleCodeForFrame(code);
    }
    
    // Отправляем значения для минимальной задержки и кода
    send_ppm_code(pio, sm, min_gap_cycles, code_cycles);
  }

  // Метод для отправки фреймов с регулярным интервалом
  void update() {
    if (absolute_time_diff_us(get_absolute_time(), nextFrameTime) <= 0) {
      // В режиме тестирования имитируем логику из audio_ppm.c
      if (testMode) {
        // Обновляем код для демонстрации, как в audio_ppm.c
        currentCode += testDirection;
        
        // Изменение направления при достижении границ
        if (currentCode >= 1023) {
          testDirection = -1;
        } else if (currentCode <= 1) {
          testDirection = 1;
        }
        
        // Выводим статистику каждые 100 обновлений (или другое значение)
        if (++testOutputCounter % 100 == 0) {
          uint32_t min_gap_cycles = MIN_INTERVAL_CYCLES;
          uint32_t code_cycles = calculateTestModeDelay(currentCode);
          
          // Вычисляем разбиение на внешний/внутренний счётчики
          uint32_t outer_min = min_gap_cycles / 32;
          uint32_t inner_min = min_gap_cycles % 32;
          uint32_t outer_code = code_cycles / 32;
          uint32_t inner_code = code_cycles % 32;
          
          // Отправляем в терминал с форматированием
          std::string stats = "\r\n[TEST] Code: " + std::to_string(currentCode) + 
                              " / Min. cycles with compensation: " + std::to_string(MIN_INTERVAL_CYCLES) +
                              " / Calculated pause: " + std::to_string(MIN_INTERVAL_CYCLES / 133.0f) + " us\r\n";
          
          tud_cdc_write(stats.c_str(), stats.length());
          tud_cdc_write_flush();
        }
        
        // Устанавливаем время следующего фрейма согласно частоте 48 кГц
        nextFrameTime = make_timeout_time_us(AUDIO_FRAME_TIME_US);
      } else {
        // В обычном режиме используем стандартную частоту фреймов
        nextFrameTime = make_timeout_time_us(FRAME_TIME_US);
      }
      
      // Отправляем текущий код
      sendCode(currentCode);
    }
  }

  // Парсинг входной команды - поддерживает C/c и T/t
  bool parseCommand(const std::string &cmd, uint16_t &code) {
    // Команда тест (T/t)
    if (cmd.length() == 1 && (cmd[0] == 'T' || cmd[0] == 't')) {
      testMode = !testMode;
      
      if (testMode) {
        // При включении тестового режима сбросить счетчик
        currentCode = 0;
        testDirection = 1;
      }
      
      code = testMode ? 1 : 0;
      return true;
    }
    
    // Обработка команды кода (C:число или c:число)
    if (cmd.length() >= 3 && (cmd[0] == 'C' || cmd[0] == 'c') && cmd[1] == ':') {
      try {
        code = std::stoi(cmd.substr(2));
        return true;
      } catch (...) {
        return false;
      }
    }
    
    return false;
  }

  // Геттеры
  bool isTestMode() const { return testMode; }
  uint16_t getCurrentCode() const { return currentCode; }
};

#define LED_TIME 500

int main() {
  // Установить системную частоту 133 МГц для корректной работы PIO
  set_sys_clock_khz(133000, true);

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

  // Создаем объект контроллера PPM
  PPMController ppmCtrl;
  ppmCtrl.init();  // без параметра калибровки

  // Отправляем начальный код (нулевой)
  ppmCtrl.sendCode(0);

  // Буфер для накопления команды до нажатия Enter
  std::string command_buffer;
  
  while (true) {
    tud_task(); // USB

    // Регулярная отправка PPM фреймов
    ppmCtrl.update();

    // Неблокирующее мигание светодиодом
    if (absolute_time_diff_us(get_absolute_time(), next_led_toggle_time) <= 0) {
      led_state = !led_state;
      gpio_put(LED_PIN, led_state);
      next_led_toggle_time = make_timeout_time_ms(LED_TIME);
    }

    // Обработка USB CDC
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
                    std::string mode = ppmCtrl.isTestMode() ? "включен" : "выключен";
                    std::string response = "\r\nРежим тестирования " + mode + "\r\n";
                    tud_cdc_write(response.c_str(), response.length());
                    tud_cdc_write_flush();
                  } 
                  else {
                    // Обычная команда кода
                    ppmCtrl.sendCode(code);
                    std::string response = "\r\nPPM code sent: " + std::to_string(code) + "\r\n";
                    tud_cdc_write(response.c_str(), response.length());
                    tud_cdc_write_flush();
                  }
                } else {
                  // Если команда не распознана
                  std::string error = "\r\nНераспознанная команда: " + command_buffer + "\r\n";
                  tud_cdc_write(error.c_str(), error.length());
                  tud_cdc_write_flush();
                }
                
                // Очищаем буфер после обработки команды
                command_buffer.clear();
              }
            } 
            else if (c == 127 || c == 8) {
              // Обработка Backspace или Delete
              if (!command_buffer.empty()) {
                command_buffer.pop_back();
              }
            }
            else {
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