/**
 * PPM Encoder для Raspberry Pi Pico (аудио через лазер)
 * 
 * - Тактовая частота PIO: 133 МГц (для коротких импульсов)
 * - Частота дискретизации аудио: 48 кГц
 * - Кодировка значений: 0-1024
 * - Фиксированная минимальная задержка между импульсами: 3 мкс
 * - Каждый фрейм всегда содержит ровно два импульса
 */

 #include <stdio.h>
 #include "pico/stdlib.h"
 #include "hardware/pio.h"
 #include "hardware/clocks.h"
 #include "hardware/irq.h"
 
 // Определение PIO ассемблерной программы
 // Это официальный формат ассемблера для PIO Raspberry Pi Pico
 const char *ppm_encoder_program = 
     ".program ppm_encoder\n"
     ".side_set 1 opt\n"
     "\n"
     "    pull           side 0      ; Получить первое значение - внешний счетчик для мин. задержки (3 мкс)\n"
     "    mov x, osr     side 0      ; Сохранить в X\n"
     "    pull           side 0      ; Получить второе значение - внутренний счетчик для мин. задержки\n"
     "    mov y, osr     side 0      ; Сохранить в Y\n"
     "\n"
     "    ; Генерация первого импульса (минимальная длительность)\n"
     "    nop            side 1      ; Начало первого импульса\n"
     "    nop            side 0      ; Конец первого импульса\n"
     "\n"
     "    ; Задержка минимум 3 мкс (внешний + внутренний циклы)\n"
     "outer_loop1:\n"
     "    mov y, isr     side 0      ; Восстановить внутренний счетчик\n"
     "inner_loop1:\n"
     "    jmp y--, inner_loop1 side 0 ; Внутренний цикл (до 31)\n"
     "    jmp x--, outer_loop1 side 0 ; Внешний цикл\n"
     "\n"
     "    ; Загрузить значение задержки для кода (0-1024, разделенное на части)\n"
     "    pull           side 0      ; Получить внешний счетчик для значения кода\n"
     "    mov x, osr     side 0      ; Сохранить в X\n"
     "    pull           side 0      ; Получить внутренний счетчик для значения кода\n"
     "    mov y, osr     side 0      ; Сохранить в Y\n"
     "\n"
     "    ; Задержка для кодового значения\n"
     "outer_loop2:\n"
     "    mov y, isr     side 0      ; Восстановить внутренний счетчик\n"
     "inner_loop2:\n"
     "    jmp y--, inner_loop2 side 0 ; Внутренний цикл (до 31)\n"
     "    jmp x--, outer_loop2 side 0 ; Внешний цикл (может быть 0, если значение кода мало)\n"
     "\n"
     "    ; Генерация второго импульса (минимальная длительность)\n"
     "    nop            side 1      ; Начало второго импульса\n"
     "    nop            side 0      ; Конец второго импульса\n"
     "\n"
     "    ; Задержка до следующего фрейма (для обеспечения частоты 48 кГц)\n"
     "    pull           side 0      ; Получить внешний счетчик задержки до след. фрейма\n"
     "    mov x, osr     side 0      ; Сохранить в X\n"
     "    pull           side 0      ; Получить внутренний счетчик задержки до след. фрейма\n"
     "    mov y, osr     side 0      ; Сохранить в Y\n"
     "\n"
     "outer_loop3:\n"
     "    mov y, isr     side 0      ; Восстановить внутренний счетчик\n"
     "inner_loop3:\n"
     "    jmp y--, inner_loop3 side 0 ; Внутренний цикл (до 31)\n"
     "    jmp x--, outer_loop3 side 0 ; Внешний цикл\n"
     "\n"
     "    jmp 0          side 0      ; Вернуться в начало";
 
 // Функция для загрузки программы в PIO
 static inline PIO setup_ppm_encoder(const uint pin) {
     // Выбор PIO блока (0 или 1)
     PIO pio = pio0;
     
     // Получение состояния PIO
     int sm = pio_claim_unused_sm(pio, true);
     if (sm == -1) {
         printf("Ошибка: нет свободных state machines в PIO\n");
         return NULL;
     }
     
     // Компиляция и загрузка программы в память инструкций PIO
     struct pio_program ppm_program = {
         .instructions = NULL,  // Будет установлено при компиляции
         .length = 0,           // Будет установлено при компиляции
         .origin = -1           // Автоматический выбор адреса загрузки
     };
     
     // Буфер для скомпилированных инструкций
     uint16_t compiled_instructions[64];
     ppm_program.instructions = compiled_instructions;
     
     // Компиляция программы
     pio_assemble_program_with_options(
         &ppm_program,          // Структура программы для заполнения
         ppm_encoder_program,   // Исходный код ассемблера
         compiled_instructions, // Буфер для скомпилированных инструкций
         64,                    // Размер буфера
         NULL,                 // Опции компиляции (по умолчанию)
         0                      // Число опций
     );
     
     // Загрузка программы в память инструкций PIO
     uint offset = pio_add_program(pio, &ppm_program);
     
     // Настройка state machine
     pio_sm_config config = pio_get_default_sm_config();
     
     // Настройка пинов
     pio_gpio_init(pio, pin);
     pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
     sm_config_set_sideset_pins(&config, pin);
     
     // Настройка FIFO
     sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);
     
     // Настройка сдвигов и порогов
     sm_config_set_out_shift(&config, false, true, 32);
     sm_config_set_in_shift(&config, false, true, 32);
     
     // Применение конфигурации
     pio_sm_init(pio, sm, offset, &config);
     
     // Запуск state machine
     pio_sm_set_enabled(pio, sm, true);
     
     return pio;
 }
 
 // Константы для расчета временных интервалов
 #define PIO_FREQ 133000000      // 133 МГц
 #define AUDIO_SAMPLE_RATE 48000 // 48 кГц
 #define MIN_GAP_US 3            // Минимальная задержка между импульсами (мкс)
 #define MAX_CODE 1024           // Максимальное кодовое значение
 
 // Структура для хранения разделенных счетчиков циклов
 typedef struct {
     uint32_t outer_count;    // Внешний счетчик (для циклов по 31)
     uint32_t inner_count;    // Внутренний счетчик (0-31)
 } cycle_counter_t;
 
 // Функция для разделения больших значений циклов на внешний и внутренний счетчики
 cycle_counter_t split_cycles(uint32_t total_cycles) {
     cycle_counter_t result;
     
     // Максимальное значение внутреннего счетчика (5 бит)
     const uint32_t max_inner = 31;
     
     // Разделение на внешний и внутренний циклы
     result.outer_count = total_cycles / (max_inner + 1);
     result.inner_count = total_cycles % (max_inner + 1);
     
     return result;
 }
 
 int main() {
     // Установка максимальной частоты для RP2040
     set_sys_clock_khz(133000, true);
     
     // Инициализация stdio
     stdio_init_all();
     printf("PPM Encoder для Raspberry Pi Pico (аудио через лазер)\n");
     sleep_ms(2000);  // Пауза для подключения терминала
     
     // Пин для вывода PPM сигнала
     const uint PPM_PIN = 0;
     
     // Настройка PIO
     PIO pio = setup_ppm_encoder(PPM_PIN);
     if (pio == NULL) {
         printf("Ошибка инициализации PIO\n");
         return -1;
     }
     
     // Расчет значений для временных интервалов
     uint32_t cycles_per_us = PIO_FREQ / 1000000;
     uint32_t min_gap_cycles = cycles_per_us * MIN_GAP_US;  // Минимальная задержка 3 мкс в циклах
     
     // Расчет циклов на один аудиосэмпл при 48 кГц
     uint32_t cycles_per_sample = PIO_FREQ / AUDIO_SAMPLE_RATE;
     
     printf("Инициализация завершена.\n");
     printf("Частота PIO: %u МГц\n", PIO_FREQ / 1000000);
     printf("Циклы на 3 мкс: %u\n", min_gap_cycles);
     printf("Циклы на аудиосэмпл (48 кГц): %u\n", cycles_per_sample);
     
     // Разделение минимальной задержки (3 мкс) на внешний и внутренний счетчики
     cycle_counter_t min_delay = split_cycles(min_gap_cycles);
     printf("Мин. задержка: внешний=%u, внутренний=%u\n", min_delay.outer_count, min_delay.inner_count);
     
     uint16_t code = 0;
     int8_t direction = 1;  // 1 - увеличение, -1 - уменьшение
     
     printf("Начинаем отправку PPM сигналов.\n");
     
     while (true) {
         // Расчет задержки для текущего кода (линейное масштабирование)
         // Гарантируем минимальную задержку 3 мкс плюс кодовое значение
         // Даже если код равен 0, минимальная задержка между импульсами будет 3 мкс
         uint32_t base_cycles = min_gap_cycles;  // Базовая задержка в 3 мкс
         uint32_t code_cycles = (uint32_t)((float)code / MAX_CODE * (cycles_per_sample - min_gap_cycles * 2 - 4));
         cycle_counter_t code_delay = split_cycles(base_cycles + code_cycles);
         
         // Расчет задержки до следующего фрейма (для обеспечения частоты 48 кГц)
         uint32_t remaining_cycles = cycles_per_sample - (base_cycles + code_cycles) - min_gap_cycles - 4;
         cycle_counter_t frame_delay = split_cycles(remaining_cycles);
         
         // Отправка значений в FIFO PIO
         // 1. Задержка минимум 3 мкс (внешний и внутренний счетчики)
         pio_sm_put_blocking(pio0, 0, min_delay.outer_count);
         pio_sm_put_blocking(pio0, 0, min_delay.inner_count);
         
         // 2. Задержка для кодового значения
         pio_sm_put_blocking(pio0, 0, code_delay.outer_count);
         pio_sm_put_blocking(pio0, 0, code_delay.inner_count);
         
         // 3. Задержка до следующего фрейма
         pio_sm_put_blocking(pio0, 0, frame_delay.outer_count);
         pio_sm_put_blocking(pio0, 0, frame_delay.inner_count);
         
         // Обновление кода для демонстрации
         code += direction;
         
         // Изменение направления при достижении границ
         if (code >= MAX_CODE - 1) {
             direction = -1;
         } else if (code <= 1) {
             direction = 1;
         }
         
         // Вывод информации о текущем коде
         if (code % 100 == 0) {
             printf("Код: %4d, полная задержка: %u циклов (3 мкс + кодовое значение)\r", 
                    code, base_cycles + code_cycles);
         }
         
         // Для настоящего аудио здесь будет чтение данных из АЦП или из буфера
         // Обратите внимание, что вместо sleep_ms нужно будет реализовать
         // корректную синхронизацию с частотой дискретизации 48 кГц
         
         // Для демо используем задержку
         sleep_ms(1);
     }
     
     return 0;
 }