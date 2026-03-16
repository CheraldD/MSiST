import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import random
import numpy as np
from pandas import DataFrame # Для красивого вывода таблицы, если есть

# ==========================================
# Настройки модели
# ==========================================
class Config:
    def __init__(self):
        self.N = 2       
        self.L = 6       
        self.Q = 3       
        self.t_mod = 40.0 
        self.dt = 0.05    
        self.server_mus = [3.5, 5.0] 
        self.initial_Z = [0.5, 1.0, 1.5]

# ==========================================
# Класс СМО
# ==========================================
class SMO:
    def __init__(self, config):
        self.cfg = config
        self.t = 0.0
        self.Z = list(self.cfg.initial_Z)
        self.U = [0.0] * self.cfg.N
        self.j = [0] * self.cfg.Q
        
        self.server_occupant = [-1] * self.cfg.N
        
        # Статистика для итогового вывода
        self.B = [0] * self.cfg.Q # Поступило
        self.P = [0] * self.cfg.Q # Отказано
        self.S = [0] * self.cfg.Q # Обслужено (успешно)
        
        # История для графиков
        self.time_history = []
        self.queue_history = [[] for _ in range(self.cfg.Q)]
        self.server_history = [[] for _ in range(self.cfg.N)]
        self.reject_events = []   
        self.arrival_events = []  

    def gen_random_interval(self):
        return random.uniform(1.5, 3.0)

    def check_task(self, k_index):
        self.B[k_index] += 1
        self.arrival_events.append((self.t, k_index))
        
        current_queue_size = sum(self.j)
        if current_queue_size < self.cfg.L:
            self.j[k_index] += 1
        else:
            displaced = False
            for low_prio in range(self.cfg.Q - 1, k_index, -1):
                if self.j[low_prio] > 0:
                    self.j[low_prio] -= 1
                    self.P[low_prio] += 1
                    self.reject_events.append((self.t, low_prio))
                    self.j[k_index] += 1
                    displaced = True
                    break
            if not displaced:
                self.P[k_index] += 1
                self.reject_events.append((self.t, k_index))

    def try_process_queue(self, server_idx):
        # Перед тем как взять новую заявку, пометим старую как обслуженную
        # (если прибор только что освободился)
        old_prio = self.server_occupant[server_idx]
        if old_prio != -1:
            self.S[old_prio] += 1

        for k in range(self.cfg.Q):
            if self.j[k] > 0:
                self.j[k] -= 1
                self.U[server_idx] = self.t + self.cfg.server_mus[server_idx]
                self.server_occupant[server_idx] = k 
                return True
        
        self.server_occupant[server_idx] = -1 
        return False

    def run(self):
        while self.t <= self.cfg.t_mod:
            self.t += self.cfg.dt
            for k in range(self.cfg.Q):
                if self.t >= self.Z[k]:
                    self.check_task(k)
                    self.Z[k] += self.gen_random_interval()
            
            for i in range(self.cfg.N):
                if self.t >= self.U[i]:
                    self.try_process_queue(i)

            self.time_history.append(self.t)
            for k in range(self.cfg.Q):
                self.queue_history[k].append(self.j[k])
            for i in range(self.cfg.N):
                self.server_history[i].append(self.server_occupant[i])

    def print_final_stats(self):
        line = "+" + "-"*11 + "+" + "-"*12 + "+" + "-"*12 + "+" + "-"*14 + "+" + "-"*14 + "+"
        print("\n" + "="*30)
        print("РЕЗУЛЬТАТЫ МОДЕЛИРОВАНИЯ")
        print("="*30)
        print(f"Время: {self.cfg.t_mod}с | Очередь: {self.cfg.L} | Приборов: {self.cfg.N}")
        print(line)
        print("| Приоритет | Поступило  |  Отказов   | Вер. отказа  | Вер. обслуж. |")
        print(line)
        
        total_B, total_P = sum(self.B), sum(self.P)
        
        for k in range(self.cfg.Q):
            p_ref = self.P[k] / self.B[k] if self.B[k] > 0 else 0
            p_srv = 1.0 - p_ref
            print(f"| {k+1:^9} | {self.B[k]:^10} | {self.P[k]:^10} | {p_ref:^12.4f} | {p_srv:^12.4f} |")
        
        print(line)
        t_p_ref = total_P / total_B if total_B > 0 else 0
        print(f"| {'ИТОГО':^9} | {total_B:^10} | {total_P:^10} | {t_p_ref:^12.4f} | {1-t_p_ref:^12.4f} |")
        print(line + "\n")

# ==========================================
# Визуализация
# ==========================================
def plot_results(model):
    t = np.array(model.time_history)
    fig, axes = plt.subplots(4, 1, figsize=(14, 10), sharex=True, 
                             gridspec_kw={'height_ratios': [1, 2, 0.6, 0.6]})
    
    colors = ['#e74c3c', '#2ecc71', '#3498db'] 
    prio_labels = ['P1 (High)', 'P2 (Mid)', 'P3 (Low)']

    # 1. Поступление
    for k in range(model.cfg.Q):
        k_times = [evt[0] for evt in model.arrival_events if evt[1] == k]
        axes[0].scatter(k_times, [k]*len(k_times), color=colors[k], marker='|', s=150, linewidth=2)
    axes[0].set_yticks(range(model.cfg.Q)), axes[0].set_yticklabels(['P1', 'P2', 'P3'])
    axes[0].set_title('Входящий поток заявок')

    # 2. Очереди
    for k in range(model.cfg.Q):
        axes[1].step(t, model.queue_history[k], where='post', color=colors[k], linewidth=2, label=prio_labels[k])
    if model.reject_events:
        rej_t, rej_p = zip(*model.reject_events)
        axes[1].scatter(rej_t, [model.cfg.L + 0.3]*len(rej_t), marker='x', color='black', label='Отказ')
    axes[1].set_ylabel('В очереди'), axes[1].set_ylim(-0.5, model.cfg.L + 1), axes[1].legend(loc='upper right')

    # 3 & 4. Приборы
    for i in range(model.cfg.N):
        ax_s = axes[2 + i]
        occ_history = np.array(model.server_history[i])
        for k in range(model.cfg.Q):
            ax_s.fill_between(t, 0, 1, where=(occ_history==k), color=colors[k], alpha=0.7, step='post')
        ax_s.set_ylabel(f'Прибор {i+1}', rotation=0, labelpad=40, va='center')

    plt.tight_layout()
    plt.show()

# ==========================================
# Старт
# ==========================================
if __name__ == "__main__":
    cfg = Config()
    model = SMO(cfg)
    model.run()
    
    # 1. Сначала выводим статистику в консоль
    model.print_final_stats()
    
    # 2. Затем показываем график
    plot_results(model)