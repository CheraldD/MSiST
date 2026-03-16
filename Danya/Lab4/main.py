import math
import random
import numpy as np
import matplotlib.pyplot as plt

class Config:
    def __init__(self):
        # Настройки модели из C++ (Вариант 16)
        self.N = 2              # Количество приборов
        self.L = 6              # Максимальная длина очереди
        self.Q = 3              # Количество приоритетов
        self.W = 3              # Порог включения абсолютного приоритета
        
        self.t_mod = 86400.0    # Время моделирования (1 день в секундах)
        self.dt = 0.1           # Шаг моделирования
        
        self.server_mus = [8.5, 5.0]
        self.initial_Z = [0.5, 1.0, 1.5]
        
        # Параметры статистики (Лабораторная 4)
        self.V_base = 17
        self.P_dov = 0.90
        self.delta_dop_rel = 0.12 # 12%
        self.t_stud = 1.7450
        
        # Шаг записи данных для графика (каждые 15 минут)
        self.record_interval = 900.0 

class SMO:
    def __init__(self, cfg):
        self.cfg = cfg
        self.t = 0.0
        self.Z = cfg.initial_Z.copy()
        
        self.U = [0.0] * cfg.N
        self.D =[-1] * cfg.N  # -1 означает, что прибор свободен
        
        self.B =[0] * cfg.Q
        self.P = [0] * cfg.Q
        self.j = [0] * cfg.Q
        
        # Сбор данных во времени
        self.time_points =[]
        self.history = [[] for _ in range(cfg.Q)]
        self.next_record = cfg.record_interval

    def get_uniform_0_1(self):
        return random.uniform(0.00001, 0.99999)

    def gen_quadratic(self, M, D):
        x = self.get_uniform_0_1()
        cbrt_val = np.cbrt(2.0 * x - 1.0)
        return math.sqrt((5.0 * D) / 3.0) * cbrt_val + M

    def gen_uniform_custom(self, M, D):
        b = (math.sqrt(12.0 * D) + 2.0 * M) / 2.0
        c = 2.0 * M - b
        x = self.get_uniform_0_1()
        return c + x * (b - c)

    def get_valid_exponential(self, mean):
        while True:
            # Эквивалент C++ генератора экспоненциального закона
            val = -math.log(self.get_uniform_0_1()) * mean
            if val > 0: return val

    def get_arrival_interval(self, priority, current_time):
        h = (current_time % 86400.0) / 3600.0
        is_day = (8.0 <= h < 20.0)

        while True:
            if priority == 0:
                M = 4.0 if is_day else 4.5455
                D_val = 16.0 if is_day else 20.6612
                val = self.gen_quadratic(M, D_val)
            elif priority == 1:
                M = 4.807692 if is_day else 2.0
                D_val = 24.02922 if is_day else 4.0
                val = self.gen_uniform_custom(M, D_val)
            else:
                M = 4.807692 if is_day else 2.0
                D_val = 24.02922 if is_day else 4.0
                val = self.gen_uniform_custom(M, D_val)
                
            if val > 0: return val

    def check_task(self, k_index):
        if sum(self.j) < self.cfg.L:
            self.j[k_index] += 1
        else:
            displaced = False
            for low_prio in range(self.cfg.Q - 1, k_index, -1):
                if self.j[low_prio] > 0:
                    self.j[low_prio] -= 1
                    self.P[low_prio] += 1
                    self.j[k_index] += 1
                    displaced = True
                    break
            if not displaced:
                self.P[k_index] += 1

    def free_finished_servers(self):
        for i in range(self.cfg.N):
            if self.D[i] != -1 and self.t >= self.U[i]:
                self.D[i] = -1

    def handle_arrivals(self):
        for k in range(self.cfg.Q):
            if self.t >= self.Z[k]:
                self.B[k] += 1
                placed = False
                for i in range(self.cfg.N):
                    if self.D[i] == -1:
                        self.D[i] = k
                        self.U[i] = self.t + self.get_valid_exponential(self.cfg.server_mus[i])
                        placed = True
                        break
                if not placed:
                    self.check_task(k)
                self.Z[k] += self.get_arrival_interval(k, self.t)

    def process_queue_relative(self):
        for i in range(self.cfg.N):
            if self.D[i] == -1:
                for k in range(self.cfg.Q):
                    if self.j[k] > 0:
                        self.j[k] -= 1
                        self.D[i] = k
                        self.U[i] = self.t + self.get_valid_exponential(self.cfg.server_mus[i])
                        break

    def process_queue_absolute(self):
        while self.j[0] >= self.cfg.W:
            highest_q = -1
            for q in range(self.cfg.Q):
                if self.j[q] > 0:
                    highest_q = q
                    break
            
            lowest_s, server_idx = -1, -1
            for i in range(self.cfg.N):
                if self.D[i] > lowest_s:
                    lowest_s = self.D[i]
                    server_idx = i
                    
            if highest_q != -1 and lowest_s != -1 and highest_q < lowest_s:
                interrupted_prio = self.D[server_idx]
                self.D[server_idx] = highest_q
                self.j[highest_q] -= 1
                self.U[server_idx] = self.t + self.get_valid_exponential(self.cfg.server_mus[server_idx])
                self.check_task(interrupted_prio)
            else:
                break

    def run(self):
        # Добавляем начальную точку
        self.time_points.append(0.0)
        for k in range(self.cfg.Q): self.history[k].append(0.0)

        while self.t <= self.cfg.t_mod:
            self.t += self.cfg.dt
            
            self.free_finished_servers()
            self.handle_arrivals()
            self.process_queue_relative()
            self.process_queue_absolute()

            if self.t >= self.next_record:
                self.time_points.append(self.t / 3600.0)
                for k in range(self.cfg.Q):
                    p_otk = self.P[k] / self.B[k] if self.B[k] > 0 else 0.0
                    self.history[k].append(p_otk)
                self.next_record += self.cfg.record_interval

    def get_rejection_probs(self):
        return [self.P[k] / self.B[k] if self.B[k] > 0 else 0.0 for k in range(self.cfg.Q)]


def run_experiments_and_plot():
    cfg = Config()
    
    all_p_finals = [[] for _ in range(cfg.Q)]
    all_histories = [[] for _ in range(cfg.Q)]
    time_axis =[]
    
    print(f"ЭТАП 1: Выполняем {cfg.V_base} прогонов (подождите 10-15 сек)...")
    
    def run_simulations(count):
        for _ in range(count):
            model = SMO(cfg)
            model.run()
            probs = model.get_rejection_probs()
            
            for q in range(cfg.Q):
                all_p_finals[q].append(probs[q])
                all_histories[q].append(model.history[q])
                
            if not time_axis:
                time_axis.extend(model.time_points)

    run_simulations(cfg.V_base)
    
    # Считаем статистику и проверяем потребность в ЭТАПЕ 2
    V_current = cfg.V_base
    V_required = V_current
    
    stats =[]
    for q in range(cfg.Q):
        mean_p = np.mean(all_p_finals[q])
        variance = np.var(all_p_finals[q], ddof=1) if V_current > 1 else 0
        all_zero = (mean_p == 0.0)
        
        if all_zero:
            delta = -math.log(1.0 - cfg.P_dov) / V_current
        else:
            delta = cfg.t_stud * math.sqrt(variance / V_current)
            absolute_delta_dop = mean_p * cfg.delta_dop_rel
            
            if delta > absolute_delta_dop:
                V2_q = math.ceil(variance * math.pow(cfg.t_stud / absolute_delta_dop, 2))
                if V2_q > V_required:
                    V_required = min(V2_q, 100) # Ограничитель, чтобы не зависнуть
        
        stats.append((mean_p, variance, delta))

    # Двухэтапный прогон
    if V_required > V_current:
        print(f"ЭТАП 2: Точность не достигнута. Добираем до V2 = {V_required}...")
        run_simulations(V_required - V_current)
        V_current = V_required
        
        # Пересчет после добора
        stats =[]
        for q in range(cfg.Q):
            mean_p = np.mean(all_p_finals[q])
            variance = np.var(all_p_finals[q], ddof=1) if V_current > 1 else 0
            delta = cfg.t_stud * math.sqrt(variance / V_current) if mean_p > 0 else -math.log(1.0 - cfg.P_dov)/V_current
            stats.append((mean_p, variance, delta))

    print("Построение графиков...")

    # --- ОФОРМЛЕНИЕ ГРАФИКА ---
    plt.style.use('bmh') # Приятный встроенный стиль matplotlib
    fig, ax = plt.subplots(figsize=(14, 7)) # Широкий формат для бокового текста
    
    colors =['#E24A33', '#348ABD', '#988ED5']
    labels =['Приоритет 1 (Абс)', 'Приоритет 2 (Отн)', 'Приоритет 3 (Отн)']
    
    for q in range(cfg.Q):
        # Усреднение историй по времени
        mean_history = np.mean(all_histories[q], axis=0)
        std_history = np.std(all_histories[q], axis=0)
        
        ax.plot(time_axis, mean_history, label=labels[q], color=colors[q], linewidth=2.5)
        # Добавляем заливку (показывает разброс/стабильность между прогонами)
        ax.fill_between(time_axis, mean_history - std_history, mean_history + std_history, 
                        color=colors[q], alpha=0.15)

    ax.set_title('Динамика установления вероятности отказа $P_{отк}(t)$', fontsize=15, pad=15, fontweight='bold')
    ax.set_xlabel('Время моделирования (часы)', fontsize=12)
    ax.set_ylabel('Вероятность отказа', fontsize=12)
    
    ax.set_xlim(0, 24)
    ax.set_xticks(np.arange(0, 25, 2))
    ax.grid(True, linestyle='--', alpha=0.7)
    
    ax.legend(loc='upper left', fontsize=11, frameon=True, shadow=True)

    # Формируем красивый текстовый блок со статистикой
    stats_text = f"РЕЗУЛЬТАТЫ СТАТИСТИКИ\nОбъем выборки (V) = {V_current}\n" + "─"*25 + "\n"
    for q in range(cfg.Q):
        stats_text += (
            f"{labels[q]}:\n"
            f"  M[P_отк] = {stats[q][0]:.5f}\n"
            f"  D(P_отк) = {stats[q][1]:.6f}\n"
            f"  Δ (инт)  = {stats[q][2]:.5f}\n\n"
        )
    
    # Располагаем текстовый блок СПРАВА от графика (чтобы не перекрывал линии)
    props = dict(boxstyle='round,pad=0.8', facecolor='#F9F9F9', alpha=0.9, edgecolor='#B0B0B0')
    ax.text(1.03, 0.95, stats_text.strip(), transform=ax.transAxes, fontsize=11,
            verticalalignment='top', bbox=props, family='monospace')

    # Сдвигаем сам график чуть влево, чтобы дать место тексту
    plt.subplots_adjust(right=0.72)
    
    plt.show()

if __name__ == "__main__":
    run_experiments_and_plot()