from itertools import product
import matplotlib.pyplot as plt

# a - счетчик внешнего цикла (5 бит)
# b - счетчик внутреннего цикла (5 бит) 
# c - дополнительный счетчик точной подстройки (5 бит)

def decompose_cycles(n, max_val=31):
    best_error = float('inf')
    best_combo = None

    for a in range(0, max_val + 1):          # Внешний цикл
        for b in range(1, max_val + 1):      # Внутренний цикл
            for c in range(0, 32):           # Остаточные такты: строго 5 бит → [0, 31]
                total = a * b + c
                error = abs(n - total)
                if error < best_error:
                    best_error = error
                    best_combo = (a, b, c)
                    if error == 0:
                        return best_combo, 0  # Точное попадание

    return best_combo, best_error

def decompose_cycles_optimized(n, max_val=31):
    """
    Оптимизирует выбор параметров a, b, c под реальное количество PIO циклов
    """
    best_error = float('inf')
    best_combo = None

    for a in range(0, max_val + 1):
        for b in range(1, max_val + 1):
            for c in range(0, 32):
                # Рассчитываем реальное количество циклов PIO
                cycles = simulate_pio_cycles(a, b, c)
                error = abs(n - cycles)
                
                if error < best_error:
                    best_error = error
                    best_combo = (a, b, c)
                    if error == 0:
                        return best_combo, 0  # Точное попадание

    return best_combo, best_error

def decompose_cycles_hybrid(n, max_val=31, search_radius=5):
    """
    Двухэтапный алгоритм: 
    1) Быстрый поиск по формуле a*b+c
    2) Точный поиск в окрестности найденного решения
    """
    # Этап 1: Быстрый поиск
    (a_approx, b_approx, c_approx), _ = decompose_cycles(n)
    
    # Этап 2: Точный поиск в окрестности
    best_error = float('inf')
    best_combo = None
    
    # Определяем диапазоны поиска с защитой от выхода за границы
    a_range = range(max(0, a_approx - search_radius), min(max_val + 1, a_approx + search_radius + 1))
    b_range = range(max(1, b_approx - search_radius), min(max_val + 1, b_approx + search_radius + 1))
    c_range = range(max(0, c_approx - search_radius), min(32, c_approx + search_radius + 1))
    
    for a in a_range:
        for b in b_range:
            for c in c_range:
                cycles = simulate_pio_cycles(a, b, c)
                error = abs(n - cycles)
                
                if error < best_error:
                    best_error = error
                    best_combo = (a, b, c)
                    if error == 0:
                        return best_combo, 0
    
    return best_combo, best_error

def decompose_cycles_with_constraints(n, max_val=31):
    """Учитывает аппаратные ограничения счетчиков"""
    best_error = float('inf')
    best_combo = None

    # Применение специфических ограничений для счетчиков
    for a in range(0, max_val + 1):
        for b in range(1, max_val + 1):
            for c in range(0, 32):
                cycles = simulate_pio_cycles(a, b, c)
                error = abs(n - cycles)
                
                # Дополнительные критерии оптимизации:
                # Предпочитаем меньшее общее число инструкций
                # для экономии места в PIO памяти
                instruction_count = 7  # Общее число инструкций в программе
                
                # Можно добавить весовой коэффициент для ошибки
                weighted_error = error
                
                if weighted_error < best_error:
                    best_error = weighted_error
                    best_combo = (a, b, c)
                    if error == 0:
                        return best_combo, 0
    
    return best_combo, best_error

def simulate_pio_cycles(a, b, c):
    """
    Симулирует выполнение PIO программы и рассчитывает количество тактов
    
    PIO программа:
    .program delay_cycles
    .wrap_target
        pull block        ; 1 такт - Получаем значение a
        mov x, osr        ; 1 такт - x = a (счетчик внешнего цикла)
        jmp x_zero, end   ; 1 такт - Если a=0, пропускаем внешний цикл
    
    outer_loop:
        pull block        ; 1 такт - Получаем значение b
        mov y, osr        ; 1 такт - y = b (счетчик внутреннего цикла)
    
    inner_loop:           ; Внутренний цикл выполняется b раз
        jmp y--, inner_loop ; 1 такт * b
        
        jmp x--, outer_loop ; 1 такт * a
    
    end:
        pull block        ; 1 такт - Получаем значение c
        mov x, osr        ; 1 такт - x = c (дополнительные такты)
        
    extra_delay:          ; Дополнительная задержка c тактов
        jmp x--, extra_delay ; 1 такт * c
    """
    if a == 0:
        # Если a=0, выполняем только extra_delay
        cycles = 4 + c  # pull+mov+jmp x_zero+pull+mov + c тактов цикла
    else:
        # Начальные инструкции: pull+mov+jmp
        setup_cycles = 3
        
        # Для каждой итерации a: pull+mov + b итераций внутреннего цикла + jmp
        outer_loop_cycles = a * (2 + b + 1)
        
        # Финальные инструкции: pull+mov + c тактов
        final_cycles = 2 + c
        
        cycles = setup_cycles + outer_loop_cycles + final_cycles
    
    return cycles

def simulate_pio_cycles_exact(a, b, c):
    """
    Более точная формула расчёта циклов PIO
    """
    # Основа такая же, как в simulate_pio_cycles
    if a == 0:
        cycles = 4 + c  # Базовые инструкции + c тактов цикла
    else:
        cycles = 3 + a * (2 + b + 1) + 2 + c
    
    # Здесь можно добавить эмпирические корректировки
    # На основе измерений реального времени исполнения
    
    return cycles

def generate_pio_code(a, b, c):
    """Генерирует PIO код для создания задержки"""
    code = f"""\
; Задержка {a}*{b}+{c} = {a*b+c} циклов
.program delay_{a}_{b}_{c}
.wrap_target
    set x, {a}       ; Установка счетчика внешнего цикла
    jmp !x, skip_outer ; Пропустить внешний цикл если a=0
outer_loop:
    set y, {b}       ; Установка счетчика внутреннего цикла
inner_loop:
    jmp y--, inner_loop ; Внутренний цикл (b итераций)
    jmp x--, outer_loop ; Внешний цикл (a итераций)
skip_outer:
    set x, {c}       ; Установка счетчика дополнительных тактов
extra_delay:
    jmp x--, extra_delay ; Дополнительная задержка (c тактов)
.wrap
"""
    return code

def build_lookup_table(max_n=1024):
    """Создаёт таблицу поиска оптимальных a,b,c для каждого n"""
    lookup_table = []
    
    for n in range(max_n):
        (a, b, c), error = decompose_cycles_optimized(n)
        lookup_table.append((a, b, c, error))
    
    return lookup_table

# Сбор данных для графиков
N = 1024
outer_list = []
inner_list = []
rem_list = []
reconstructed = []
errors = []
pio_cycles = []  # Добавляем список для хранения фактического числа PIO циклов

for n in range(N):
    (a, b, c), err = decompose_cycles_optimized(n)
    # (a, b, c), err = decompose_cycles_hybrid(n)
    total = a * b + c
    outer_list.append(a)
    inner_list.append(b)
    rem_list.append(c)
    reconstructed.append(total)
    
    # Симуляция PIO циклов
    cycles = simulate_pio_cycles(a, b, c)
    pio_cycles.append(cycles)
    
    # Расчёт ошибки на основе реальных циклов PIO
    errors.append(cycles - n)

print("\nГраничные случаи:")
test_values = [0, 1, 2, 3, 4, 31, 32, 63, 64, 511, 512, 1023]
for tc in test_values:
    # (a, b, c), err = decompose_cycles(tc)
    (a, b, c), err = decompose_cycles_optimized(tc)
    # (a, b, c), err = decompose_cycles_hybrid(tc)
    total = a * b + c
    cycles = simulate_pio_cycles(a, b, c)
    print(f"{tc:5d} | a={a:2d} | b={b:2d} | c={c:2d} | err={err:2d} | total={total:5d} | PIO cycles={cycles:5d}")
    
    # Вывести PIO код для нескольких примеров
    # if tc in [31, 63, 512]:
    #     print("\nPIO код для задержки {}:".format(tc))
    #     print(generate_pio_code(a, b, c))

# Графики
plt.figure(figsize=(14, 12))

plt.subplot(4, 1, 1)
plt.plot(range(N), outer_list, label='Внешний (a)')
plt.plot(range(N), inner_list, label='Внутренний (b)')
plt.plot(range(N), rem_list, label='Остаток (c)')
plt.title('Значения счетчиков')
plt.xlabel('n')
plt.ylabel('Значение')
plt.legend()
plt.grid(True)

plt.subplot(4, 1, 2)
plt.plot(range(N), errors, label='Ошибка (cycles - n)')
plt.title('Ошибка разложения')
plt.xlabel('n')
plt.ylabel('Ошибка')
plt.legend()
plt.grid(True)

plt.subplot(4, 1, 3)
plt.plot(range(N), pio_cycles, label='PIO циклы')
plt.plot(range(N), range(N), 'r--', label='Идеальная линия')
plt.title('Фактическое количество PIO циклов')
plt.xlabel('Требуемое значение n')
plt.ylabel('Циклы')
plt.legend()
plt.grid(True)

plt.tight_layout()
plt.show()

