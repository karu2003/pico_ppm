from itertools import product
import matplotlib.pyplot as plt

def decompose_cycles(n, max_val=32):
    best_error = float('inf')
    best_combo = None

    for a in range(1, max_val + 1):          # Внешний цикл
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

# Сбор данных для графиков
N = 1024
outer_list = []
inner_list = []
rem_list = []
reconstructed = []
errors = []

for n in range(N):
    (a, b, c), err = decompose_cycles(n)
    total = a * b + c
    outer_list.append(a)
    inner_list.append(b)
    rem_list.append(c)
    reconstructed.append(total)
    errors.append(total - n)

# Графики
plt.figure(figsize=(14, 8))

plt.subplot(2, 1, 1)
plt.plot(range(N), outer_list, label='Внешний')
plt.plot(range(N), inner_list, label='Внутренний')
plt.plot(range(N), rem_list, label='Остаток')
plt.title('Значения счетчиков')
plt.xlabel('n')
plt.ylabel('Значение')
plt.legend()
plt.grid(True)

plt.subplot(2, 1, 2)
plt.plot(range(N), errors, label='Ошибка (total - n)')
plt.title('Ошибка разложения')
plt.xlabel('n')
plt.ylabel('Ошибка')
plt.legend()
plt.grid(True)

plt.tight_layout()
plt.show()

