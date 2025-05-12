# 133 MHz Pico clock
# 1/133e6 = 7.52 ns
# duty_cycle = 0.1%
# laser on time = 3 ns

# длителность между импилсами

PICO_CLK = 133e6
DUTY_CYCLE = 0.001
LASER_ON_TIME = 3e-9


# Расчет общего периода
total_period = LASER_ON_TIME / DUTY_CYCLE

# Расчет длительности между импульсами
time_between_pulses = total_period - LASER_ON_TIME

print(f"Длительность импульса: {LASER_ON_TIME*1e9:.0f} нс")
print(f"Рабочий цикл: {DUTY_CYCLE*100}%")
print(f"Общий период: {total_period*1e9:.0f} нс ({total_period*1e6:.3f} мкс)")
print(f"Минимальная длительность между импульсами: {time_between_pulses*1e9:.0f} нс ({time_between_pulses*1e6:.3f} мкс)")

pico_tack = 1 / PICO_CLK
# Расчет длительности импульса
pulse_duration = pico_tack * 1024
print(f"Длительность max code: {pulse_duration*1e9:.0f} нс")

min_frame_duration = time_between_pulses + pulse_duration + time_between_pulses
print(f"Минимальная длительность между кадрами: {min_frame_duration*1e9:.0f} нс ({min_frame_duration*1e6:.3f} мкс)")

# Новый блок: расчет частоты кадров и минимальной паузы
frame_rate = 1 / min_frame_duration
print(f"Частота кадров: {frame_rate:.3f} Гц")

print(f"Длительность одного такта Pico: {pico_tack*1e9:.2f} нс")

# Минимально возможная пауза между импульсами в тактах Pico (например, 3 мкс)
min_pause_us = 3
min_pause_ticks = int(min_pause_us * 1e-6 * PICO_CLK)
print(f"Минимальная пауза между импульсами: {min_pause_ticks} тактов Pico ({min_pause_us} мкс)")
