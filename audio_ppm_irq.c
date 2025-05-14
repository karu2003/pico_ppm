/**
 * PPM Encoder для Raspberry Pi Pico (аудио через лазер)
 * 
 * - Тактовая частота PIO: 133 МГц (для коротких импульсов)
 * - Частота дискретизации аудио: 48 кГц
 * - Кодировка значений: 0-1024
 * - Фиксированная минимальная задержка между импульсами: 3 мкс
 * - Каждый фрейм всегда содержит ровно два импульса
 * 
 * Улучшенная архитектура:
 * - PIO отвечает только за генерацию коротких лазерных импульсов
 * - Таймер и прерывания управляют задержками между импульсами
 * - Устранены ограничения на 5-битные счетчики
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/timer.h"

// Определение PIO ассемблерной программы (упрощенная версия)
// Программа только генерирует короткий импульс по запросу
const char *pulse_generator_program = 
    ".program pulse_generator\n"
    ".side_set 1 opt\n"
    "\n"
    "    pull           side 0      ; Ожидать команду на генерацию импульса\n"
    "    nop            side 1      ; Генерация импульса (высокий уровень)\n"
    "    nop            side 0      ; Завершение импульса (низкий уровень)\n"
    "    irq 0          side 0      ; Уведомление CPU, что импульс отправлен\n";

// Глобальные переменные для управления состоянием PPM генератора
static PIO g_pio;
static int g_sm;
static uint g_ppm_pin;
static bool g_second_pulse = false;  // Флаг для отслеживания текущего импульса в фрейме
static uint16_t g_current_code = 0;  // Текущее кодовое значение (0-1024)
static int8_t g_direction = 1;       // Направление изменения кода для демо
static absolute_time_t g_next_frame_time; // Время начала следующего фрейма

// Константы для расчета временных интервалов
#define PIO_FREQ 133000000      // 133 МГц
#define AUDIO_SAMPLE_RATE 48000 // 48 кГц
#define MIN_GAP_US 3            // Минимальная задержка между импульсами (мкс)
#define MAX_CODE 1024           // Максимальное кодовое значение

// Обработчик прерывания от PIO
void __isr pio_irq_handler() {
    // Очистка флага прерывания
    if (pio_interrupt_get(g_pio, 0)) {
        pio_interrupt_clear(g_pio, 0);
        
        // Расчет времени для следующего импульса
        if (!g_second_pulse) {
            // Был отправлен первый импульс, нужно запланировать второй
            // Задержка = минимальная задержка (3 мкс) + кодовое значение
            uint32_t delay_us = MIN_GAP_US + ((uint32_t)g_current_code * 20 / MAX_CODE);
            
            // Планируем следующее срабатывание таймера
            add_alarm_in_us(delay_us, alarm_callback, NULL, false);
            
            // Отмечаем, что следующий импульс будет вторым
            g_second_pulse = true;
        } else {
            // Был отправлен второй импульс
            // Фрейм завершен, нужно дождаться времени следующего фрейма
            
            // Вычисляем время до следующего фрейма (для поддержания частоты 48 кГц)
            absolute_time_t current_time = get_absolute_time();
            int64_t time_until_next_frame = absolute_time_diff_us(current_time, g_next_frame_time);
            
            if (time_until_next_frame > 0) {
                // Планируем срабатывание таймера на начало следующего фрейма
                add_alarm_at(g_next_frame_time, alarm_callback, NULL, false);
            } else {
                // Мы уже опоздали к следующему фрейму, запускаем сразу
                alarm_callback(0, NULL);
            }
            
            // Обновление кода для демонстрации
            g_current_code += g_direction;
            
            // Изменение направления при достижении границ
            if (g_current_code >= MAX_CODE - 1) {
                g_direction = -1;
            } else if (g_current_code <= 1) {
                g_direction = 1;
            }
            
            // Вывод информации о текущем коде
            if (g_current_code % 100 == 0) {
                printf("Код: %4d, задержка: %u мкс\r", 
                       g_current_code, MIN_GAP_US + ((uint32_t)g_current_code * 20 / MAX_CODE));
            }
            
            // Сбрасываем флаг для следующего фрейма
            g_second_pulse = false;
        }
    }
}

// Обработчик сигнала таймера
bool alarm_callback(repeating_timer_t *rt) {
    // Если это начало нового фрейма, обновляем время следующего фрейма
    if (!g_second_pulse) {
        // Обновление времени следующего фрейма (каждые 1/48000 секунды)
        g_next_frame_time = delayed_by_us(g_next_frame_time, 1000000 / AUDIO_SAMPLE_RATE);
    }
    
    // Отправка команды в PIO для генерации импульса
    pio_sm_put_blocking(g_pio, g_sm, 1);  // Значение не имеет значения, просто сигнал
    
    return false;  // Не повторять таймер автоматически
}

// Функция для загрузки программы в PIO
static inline void setup_pulse_generator(PIO pio, uint sm, uint pin) {
    // Сохраняем глобальные параметры
    g_pio = pio;
    g_sm = sm;
    g_ppm_pin = pin;
    
    // Компиляция и загрузка программы в память инструкций PIO
    uint offset = pio_add_program(pio, &(pio_program_t){
        .instructions = (const uint16_t[]){
            0x80a0, // pull           side 0
            0xa0e1, // nop            side 1
            0xa0a0, // nop            side 0
            0xc040  // irq 0          side 0
        },
        .length = 4,
        .origin = -1 // Автоматический выбор адреса загрузки
    });
    
    // Настройка state machine
    pio_sm_config config = pio_get_default_sm_config();
    
    // Настройка пинов
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    sm_config_set_sideset_pins(&config, pin);
    
    // Настройка FIFO
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);
    
    // Настройка IRQ
    pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
    
    // Применение конфигурации
    pio_sm_init(pio, sm, offset, &config);
    
    // Настройка обработчика прерываний
    irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);
    irq_set_enabled(PIO0_IRQ_0, true);
    
    // Запуск state machine
    pio_sm_set_enabled(pio, sm, true);
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
    PIO pio = pio0;
    int sm = pio_claim_unused_sm(pio, true);
    if (sm == -1) {
        printf("Ошибка: нет свободных state machines в PIO\n");
        return -1;
    }
    
    // Настройка генератора импульсов
    setup_pulse_generator(pio, sm, PPM_PIN);
    
    // Установка времени начала первого фрейма
    g_next_frame_time = make_timeout_time_ms(10);  // Через 10 мс
    
    printf("Инициализация завершена.\n");
    printf("Частота PIO: %u МГц\n", PIO_FREQ / 1000000);
    printf("Частота аудио: %u Гц\n", AUDIO_SAMPLE_RATE);
    printf("Минимальная задержка между импульсами: %u мкс\n", MIN_GAP_US);
    
    printf("Начинаем отправку PPM сигналов.\n");
    
    // Запуск первого фрейма
    add_alarm_at(g_next_frame_time, alarm_callback, NULL, false);
    
    // Основной цикл - в реальном приложении здесь может быть обработка аудио данных
    while (true) {
        // Для настоящего аудио здесь будет чтение данных из АЦП или из буфера
        tight_loop_contents();
    }
    
    return 0;
}