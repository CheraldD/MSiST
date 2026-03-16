import math
import random
import numpy as np
import matplotlib.pyplot as plt

class Config:
    def __init__(self):
        self.N = 1              # Количество приборов
        self.L = 6              # Максимальная длина очереди
        self.Q = 2              # Количество приоритетов
        self.W = 2              # Порог переключения на абсолютный приоритет
        
        self.t_mod = 86400      # Время моделирования (1 сутки)
        self.dt = 0.1           # Шаг моделирования
        self.mu_mean = 2      # Матожидание времени обслуживания
        
        self.initial_Z = [0.0, 5.0, 0.5] # Начальные Z (индекс 0 не используется)
        
        # Параметры статистики
        self.V_base = 20
        self.P_dov = 0.95
        self.delta_dop_percent = 7.0
        self.t_stud = 2.0930
        
        # Шаг записи данных для графика (каждые 15 минут)
        self.record_interval = 900 

class SMO:
    def __init__(self, cfg):
        self.cfg = cfg
        self.t = 0.0
        self.Z = cfg.initial_Z.copy()
        
        self.U = [0.0] * cfg.N
        self.D = [0] * cfg.N
        
        self.B = [0] * (cfg.Q + 1)
        self.P = [0] * (cfg.Q + 1)
        self.j =[0] * (cfg.Q + 1)
        
        # Для сбора данных во времени
        self.time_points = []
        self.p1_history =[]
        self.p2_history =[]
        self.next_record = 0.0

    def generate_exponential(self, mean):
        while True:
            val = random.expovariate(1.0 / mean)
            if val > 0: return val

    def generate_uniform(self, M, D):
        c = M - math.sqrt(3 * D)
        b = M + math.sqrt(3 * D)
        while True:
            val = random.uniform(c, b)
            if val > 0: return val

    def generate_cubic(self, M, D):
        while True:
            x = random.uniform(0.0, 1.0)
            val = math.sqrt(37.5 * D) * (math.pow(x, 0.25) - 0.8) + M
            if val > 0: return val

    def gen_random_interval(self, prio_index, current_time):
        t_hour = (current_time % 86400.0) / 3600.0
        is_day = (8.0 <= t_hour < 20.0)

        if prio_index == 1:
            M = 4.807692 if is_day else 2.0
            D_val = 24.02922 if is_day else 4.0
            return self.generate_uniform(M, D_val)
        else:
            M = 5.208333 if is_day else 0.25
            D_val = 27.12674 if is_day else 0.0625
            return self.generate_cubic(M, D_val)

    def put_in_queue(self, k):
        current_queue_size = sum(self.j[1:])
        if current_queue_size < self.cfg.L:
            self.j[k] += 1
        else:
            displaced = False
            for low in range(self.cfg.Q, k, -1):
                if self.j[low] > 0:
                    self.j[low] -= 1
                    self.P[low] += 1
                    self.j[k] += 1
                    displaced = True
                    break
            if not displaced:
                self.P[k] += 1

    def capture_server(self, k):
        for i in range(self.cfg.N):
            if self.D[i] == 0:
                self.D[i] = k
                self.U[i] = self.t + self.generate_exponential(self.cfg.mu_mean)
                return
        
        if self.j[1] >= self.cfg.W:
            for lowest in range(self.cfg.Q, k, -1):
                for i in range(self.cfg.N):
                    if self.D[i] == lowest:
                        preempted = self.D[i]
                        self.D[i] = k
                        self.U[i] = self.t + self.generate_exponential(self.cfg.mu_mean)
                        self.put_in_queue(preempted)
                        return
        self.put_in_queue(k)

    def extract_from_queue(self, server_index):
        for k in range(1, self.cfg.Q + 1):
            if self.j[k] > 0:
                self.j[k] -= 1
                self.D[server_index] = k
                self.U[server_index] = self.t + self.generate_exponential(self.cfg.mu_mean)
                return

    def check_mixed_priority_switch(self):
        if self.j[1] >= self.cfg.W:
            for i in range(self.cfg.N):
                if self.D[i] > 1:
                    if self.j[1] > 0:
                        preempted = self.D[i]
                        self.j[1] -= 1
                        self.D[i] = 1
                        self.U[i] = self.t + self.generate_exponential(self.cfg.mu_mean)
                        self.put_in_queue(preempted)

    def run(self):
        while self.t <= self.cfg.t_mod:
            # Запись данных для графика
            if self.t >= self.next_record:
                self.time_points.append(self.t / 3600.0) # в часах
                p1 = self.P[1] / self.B[1] if self.B[1] > 0 else 0.0
                p2 = self.P[2] / self.B[2] if self.B[2] > 0 else 0.0
                self.p1_history.append(p1)
                self.p2_history.append(p2)
                self.next_record += self.cfg.record_interval

            self.t += self.cfg.dt
            
            for k in range(1, self.cfg.Q + 1):
                if self.t >= self.Z[k]:
                    self.B[k] += 1
                    self.capture_server(k)
                    self.Z[k] += self.gen_random_interval(k, self.t)
            
            self.check_mixed_priority_switch()
            
            for i in range(self.cfg.N):
                if self.D[i] != 0 and self.t >= self.U[i]:
                    self.D[i] = 0
                    self.extract_from_queue(i)

    def get_final_probs(self):
        p1 = self.P[1] / self.B[1] if self.B[1] > 0 else 0.0
        p2 = self.P[2] / self.B[2] if self.B[2] > 0 else 0.0
        return p1, p2

def run_experiments_and_plot():
    print("Начинаем моделирование (это может занять около 10-20 секунд)...")
    cfg = Config()
    
    p1_finals = []
    p2_finals = []
    
    histories_p1 =[]
    histories_p2 = []
    time_axis =[]
    
    # ЭТАП 1: Базовый объем выборки
    V = cfg.V_base
    
    def run_simulations(count):
        for _ in range(count):
            model = SMO(cfg)
            model.run()
            p1, p2 = model.get_final_probs()
            p1_finals.append(p1)
            p2_finals.append(p2)
            histories_p1.append(model.p1_history)
            histories_p2.append(model.p2_history)
            if not time_axis:
                time_axis.extend(model.time_points)
                
    run_simulations(V)
    
    # Расчет статистики для Приоритета 1 (по которому обычно идет останов)
    def calc_stats(p_list, V_cur):
        mean_p = np.mean(p_list)
        variance = np.var(p_list, ddof=1) if V_cur > 1 else 0
        all_zero = all(p == 0 for p in p_list)
        
        if all_zero:
            delta = -math.log(1.0 - cfg.P_dov) / V_cur
        else:
            delta = cfg.t_stud * math.sqrt(variance / V_cur)
            
        return mean_p, variance, delta, all_zero

    mean1, var1, delta1, zero1 = calc_stats(p1_finals, V)
    mean2, var2, delta2, zero2 = calc_stats(p2_finals, V)
    
    # Проверка на двухэтапный прогон (Формула 1.5)
    delta_dop_abs = (cfg.delta_dop_percent / 100.0) * mean1
    V_need = V
    if not zero1 and delta_dop_abs > 0:
        V_need = math.ceil(var1 * math.pow(cfg.t_stud / delta_dop_abs, 2))
        
    if V_need > 100: V_need = 100 # Ограничитель
    
    if V_need > V:
        print(f"-> Сработал авто-останов! Требуется V = {V_need}. Домоделируем {V_need - V} раз...")
        run_simulations(V_need - V)
        V = V_need
        mean1, var1, delta1, zero1 = calc_stats(p1_finals, V)
        mean2, var2, delta2, zero2 = calc_stats(p2_finals, V)

    print("Моделирование завершено! Строим графики...")

    # Усреднение графиков по времени
    avg_history_p1 = np.mean(histories_p1, axis=0)
    avg_history_p2 = np.mean(histories_p2, axis=0)

    # --- ПОСТРОЕНИЕ ГРАФИКОВ ---
    plt.figure(figsize=(12, 7))
    plt.plot(time_axis, avg_history_p1, label='Приоритет 1', color='red', linewidth=2)
    plt.plot(time_axis, avg_history_p2, label='Приоритет 2', color='blue', linewidth=2)
    
    plt.title('Зависимость вероятности отказа от времени моделирования', fontsize=14)
    plt.xlabel('Время моделирования (часы)', fontsize=12)
    plt.ylabel('Вероятность отказа $P_{отк}(t)$', fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.xlim(0, 24)
    plt.xticks(np.arange(0, 25, 2))
    
    # Текстовый блок со статистикой (Пункт 1)
    stats_text = (
        f"СТАТИСТИЧЕСКИЕ ХАРАКТЕРИСТИКИ\n"
        f"Объем выборки (V): {V}\n\n"
        f"Приоритет 1 (Высший):\n"
        f"  $M[P_{{отк}}]$ = {mean1:.4f}\n"
        f"  $D(P_{{отк}})$ = {var1:.6f}\n"
        f"  Дов. интервал $\Delta$ = {delta1:.4f}\n\n"
        f"Приоритет 2 (Низший):\n"
        f"  $M[P_{{отк}}]$ = {mean2:.4f}\n"
        f"  $D(P_{{отк}})$ = {var2:.6f}\n"
        f"  Дов. интервал $\Delta$ = {delta2:.4f}"
    )
    
    # Размещение текстового блока на графике
    props = dict(boxstyle='round', facecolor='white', alpha=0.9, edgecolor='gray')
    plt.text(0.02, 0.95, stats_text, transform=plt.gca().transAxes, fontsize=10,
             verticalalignment='top', bbox=props, family='monospace')

    plt.legend(loc='upper right', fontsize=11)
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    run_experiments_and_plot()