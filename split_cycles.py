import matplotlib.pyplot as plt
import numpy as np

def split_cycles(total_cycles):
    """
    Разделяет общее количество циклов на внешний и внутренний счетчики.
    
    Args:
        total_cycles: общее количество циклов
        
    Returns:
        tuple(outer_count, inner_count): внешний и внутренний счетчики
    """
    max_inner = 31  # Максимальное значение внутреннего счетчика (5 бит)
    
    # Разделение на внешний и внутренний циклы
    outer_count = total_cycles // (max_inner + 1)  # Целочисленное деление
    inner_count = total_cycles % (max_inner + 1)   # Остаток от деления
    
    return (outer_count, inner_count)

# Проверка линейности
def check_linearity():
    """
    Проверяет линейность разбиения для последовательности значений.
    """
    print("Total Cycles | Outer Count | Inner Count | Reconstructed")
    print("-" * 60)
    
    # Проверяем серию значений для 10-битного диапазона
    for tc in range(0, 1024, 30):  # От 0 до 1023 с шагом 64
        outer, inner = split_cycles(tc)
        # Восстанавливаем исходное значение
        reconstructed = outer * 32 + inner
        print(f"{tc:11d} | {outer:10d} | {inner:10d} | {reconstructed:12d}")
        
        # Проверяем соответствие
        if tc != reconstructed:
            print(f"ERROR: Mismatch for {tc} != {reconstructed}")

    # Проверка граничных случаев (10-битные значения)
    print("\nГраничные случаи:")
    test_values = [0, 31, 32, 63, 64, 511, 512, 1023]
    for tc in test_values:
        outer, inner = split_cycles(tc)
        reconstructed = outer * 32 + inner
        print(f"{tc:11d} | {outer:10d} | {inner:10d} | {reconstructed:12d}")

def plot_cycle_separation():
    """
    Рисует графики для иллюстрации разбиения циклов
    """
    # Создаем массив значений
    total_cycles = np.arange(0, 1024)
    
    # Получаем внешние и внутренние счетчики
    outer_counts = []
    inner_counts = []
    reconstructed = []
    errors = []  # Массив для ошибок
    
    for tc in total_cycles:
        outer, inner = split_cycles(tc)
        outer_counts.append(outer)
        inner_counts.append(inner)
        recon = outer * 32 + inner
        reconstructed.append(recon)
        # Вычисляем ошибку как разницу между исходным и восстановленным значением
        errors.append(recon - tc)
    
    # Создаем графики
    plt.figure(figsize=(12, 15))  # Увеличиваем высоту для 3 графиков
    
    # График 1: Внешний и внутренний счетчики с прозрачностью
    plt.subplot(3, 1, 1)
    # Внешний счетчик - синий, толстая линия, без заливки
    plt.plot(total_cycles, outer_counts, 'b-', linewidth=2, alpha=0.7, label='Внешний счётчик')
    # Внутренний счетчик - красный, тонкая пунктирная линия с маркерами
    plt.plot(total_cycles, inner_counts, 'r--', linewidth=1, marker='.',
             markersize=3, markevery=32, alpha=0.6, label='Внутренний счётчик')
    plt.title('Разбиение на внешний и внутренний счётчики')
    plt.xlabel('Общее количество циклов')
    plt.ylabel('Значение счётчика')
    plt.grid(True, alpha=0.3)
    plt.legend()
    
    # График 2: Оригинальное vs реконструированное значение с прозрачностью
    plt.subplot(3, 1, 2)
    # Идеальная линейность (сплошная линия)
    plt.plot(total_cycles, total_cycles, 'g-', linewidth=2, alpha=0.7, label='Идеальная линейность')
    # Добавляем пунктирную линию для реконструированных значений
    plt.plot(total_cycles, reconstructed, 'r--', linewidth=1, alpha=0.5, label='Линия реконструированных значений')
    # Точки для реконструированных значений (для наглядности)
    # plt.plot(total_cycles, reconstructed, 'o', color='red', markersize=3, markerfacecolor='none', 
    #          markeredgecolor='red', alpha=0.4, label='Отдельные точки')
    plt.title('Проверка линейности реконструкции')
    plt.xlabel('Исходное значение')
    plt.ylabel('Реконструированное значение')
    plt.grid(True, alpha=0.3)
    plt.legend()
    
    # График 3: Ошибка реконструкции (новый)
    plt.subplot(3, 1, 3)
    plt.plot(total_cycles, errors, 'r-', alpha=0.7)
    plt.fill_between(total_cycles, errors, 0, color='r', alpha=0.3)
    plt.axhline(y=0, color='k', linestyle='-', alpha=0.5)
    plt.title('Ошибка реконструкции (восст. - исходное)')
    plt.xlabel('Исходное значение')
    plt.ylabel('Величина ошибки')
    plt.grid(True, alpha=0.3)
    
    # Добавляем метки значений для ошибок
    if any(errors):  # Если есть ненулевые ошибки
        plt.text(50, max(errors)/2, f'Макс. ошибка: {max(errors)}', 
                 bbox=dict(facecolor='white', alpha=0.7))
        plt.text(50, min(errors)/2, f'Мин. ошибка: {min(errors)}', 
                 bbox=dict(facecolor='white', alpha=0.7))
    
    plt.tight_layout()
    plt.savefig('split_cycles_visualization.png')
    plt.show()

def plot_random_samples():
    """
    Рисует график для случайных чисел от 0 до 1024
    и проверяет точность их разбиения и реконструкции.
    """
    # Генерируем случайные числа в диапазоне от 0 до 1024
    np.random.seed(42)  # Для воспроизводимости результатов
    num_samples = 100   # Количество случайных точек
    random_values = np.random.randint(0, 1024, size=num_samples)
    
    # Вычисляем счетчики и восстановленные значения
    outer_counts = []
    inner_counts = []
    reconstructed = []
    errors = []
    
    for val in random_values:
        outer, inner = split_cycles(val)
        outer_counts.append(outer)
        inner_counts.append(inner)
        recon = outer * 32 + inner
        reconstructed.append(recon)
        errors.append(recon - val)
    
    # Создаем фигуру с двумя графиками
    plt.figure(figsize=(12, 10))
    
    # График 1: Исходные vs реконструированные значения
    plt.subplot(2, 1, 1)
    # Идеальная линейность (диагональная линия)
    x_range = np.array([0, 1024])
    plt.plot(x_range, x_range, 'g-', linewidth=2, alpha=0.7, label='Идеальная линейность')
    # Случайные точки
    plt.scatter(random_values, reconstructed, color='blue', s=30, alpha=0.7, 
                edgecolor='darkblue', label='Случайные значения')
    plt.title('Проверка линейности на случайных числах')
    plt.xlabel('Случайное исходное значение')
    plt.ylabel('Реконструированное значение')
    plt.grid(True, alpha=0.3)
    plt.legend()
    
    # График 2: Ошибки для случайных значений
    plt.subplot(2, 1, 2)
    plt.bar(range(len(random_values)), errors, alpha=0.7, color='purple')
    plt.axhline(y=0, color='k', linestyle='-', alpha=0.5)
    plt.title('Ошибки реконструкции для случайных значений')
    plt.xlabel('Индекс случайного значения')
    plt.ylabel('Ошибка (восст. - исходное)')
    plt.grid(True, alpha=0.3)
    
    # Статистика ошибок
    if any(errors):
        min_err = min(errors)
        max_err = max(errors)
        mean_err = sum(errors) / len(errors)
        plt.figtext(0.15, 0.3, f'Мин. ошибка: {min_err}\nМакс. ошибка: {max_err}\nСредняя ошибка: {mean_err:.4f}',
                   bbox=dict(facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig('random_samples_verification.png')
    plt.show()

# Запускаем проверку с визуализацией
if __name__ == "__main__":
    check_linearity()
    plot_cycle_separation()
    plot_random_samples()  # Добавляем проверку на случайных значениях