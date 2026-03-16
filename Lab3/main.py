import random
import math
import matplotlib.pyplot as plt

class Config:
    def __init__(self):
        self.N = 1          # Количество приборов
        self.L = 6          # Максимальная длина очереди
        self.Q = 2          # Количество приоритетов (1 - высший, Q - низший)
        
        # Переключение на абсолютный приоритет при W заявок первого приоритета в очереди
        self.W = 2          

        # Время моделирования: 1 сутки (в секундах)
        self.t_mod = 86400.0   
        self.dt = 0.1       # Шаг моделирования

        self.mu_mean = 15.0 # Математическое ожидание времени обработки заявки

        # Начальные Z (моменты поступления). Нулевой индекс не используется.
        self.initial_Z = [0.0, 5.0, 0.5]

class SMO:
    def __init__(self, cfg):
        self.cfg = cfg
        self.t = 0.0

        self.Z = self.cfg.initial_Z.copy()
        self.U =[0.0] * self.cfg.N
        self.D =[0] * self.cfg.N

        self.B = [0] * (self.cfg.Q + 1)
        self.P =[0] * (self.cfg.Q + 1)
        self.j = [0] * (self.cfg.Q + 1)

        # Переменные для Пункта 2 (сбор заявок за минуту)
        self.last_recorded_minute = 0
        self.current_min_P1 = 0
        self.current_min_P2 = 0
        
        self.reqs_per_minute_P1 =[]
        self.reqs_per_minute_P2 =[]

        # Списки для сохранения сгенерированных интервалов и построения графиков
        self.hist_t_P1 =[]
        self.hist_val_P1 =[]
        self.hist_t_P2 =[]
        self.hist_val_P2 =[]
        self.hist_t_mu =[]
        self.hist_val_mu =[]

    def generate_exponential(self, mean):
        while True:
            val = random.expovariate(1.0 / mean)
            if val > 0:
                return val

    def generate_uniform(self, M, D):
        c = M - math.sqrt(3 * D)
        b = M + math.sqrt(3 * D)
        while True:
            val = random.uniform(c, b)
            if val > 0:
                return val

    def generate_cubic(self, M, D):
        while True:
            x = random.random()
            val = math.sqrt(37.5 * D) * (x**0.25 - 0.8) + M
            if val > 0:
                return val

    def gen_random_interval(self, prio_index, current_time):
        t_hour = (current_time % 86400.0) / 3600.0
        is_day = (8.0 <= t_hour < 20.0)

        if prio_index == 1:
            M = 4.807692 if is_day else 2.0
            D = 24.02922 if is_day else 4.0
            return self.generate_uniform(M, D)
        else:
            M = 5.208333 if is_day else 0.25
            D = 27.12674 if is_day else 0.0625
            return self.generate_cubic(M, D)

    def put_in_queue(self, k):
        current_queue_size = sum(self.j[1:self.cfg.Q + 1])
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
                mu_val = self.generate_exponential(self.cfg.mu_mean)
                self.U[i] = self.t + mu_val
                
                self.hist_t_mu.append(self.t)
                self.hist_val_mu.append(mu_val)
                return

        if self.j[1] >= self.cfg.W:
            for lowest in range(self.cfg.Q, k, -1):
                for i in range(self.cfg.N):
                    if self.D[i] == lowest:
                        preempted = self.D[i]
                        self.D[i] = k
                        mu_val = self.generate_exponential(self.cfg.mu_mean)
                        self.U[i] = self.t + mu_val
                        
                        self.hist_t_mu.append(self.t)
                        self.hist_val_mu.append(mu_val)
                        self.put_in_queue(preempted)
                        return
        self.put_in_queue(k)

    def extract_from_queue(self, server_index):
        for k in range(1, self.cfg.Q + 1):
            if self.j[k] > 0:
                self.j[k] -= 1
                self.D[server_index] = k
                mu_val = self.generate_exponential(self.cfg.mu_mean)
                self.U[server_index] = self.t + mu_val
                
                self.hist_t_mu.append(self.t)
                self.hist_val_mu.append(mu_val)
                return

    def check_mixed_priority_switch(self):
        if self.j[1] >= self.cfg.W:
            for i in range(self.cfg.N):
                if self.D[i] > 1:
                    if self.j[1] > 0:
                        preempted = self.D[i]
                        self.j[1] -= 1
                        self.D[i] = 1
                        mu_val = self.generate_exponential(self.cfg.mu_mean)
                        self.U[i] = self.t + mu_val
                        
                        self.hist_t_mu.append(self.t)
                        self.hist_val_mu.append(mu_val)
                        self.put_in_queue(preempted)

    def run(self):
        while self.t <= self.cfg.t_mod:
            self.t += self.cfg.dt

            current_mod_minute = int(self.t / 60.0)
            if current_mod_minute > self.last_recorded_minute:
                self.reqs_per_minute_P1.append(self.current_min_P1)
                self.reqs_per_minute_P2.append(self.current_min_P2)
                self.current_min_P1 = 0
                self.current_min_P2 = 0
                self.last_recorded_minute = current_mod_minute

            for k in range(1, self.cfg.Q + 1):
                if self.t >= self.Z[k]:
                    self.B[k] += 1
                    
                    if k == 1:
                        self.current_min_P1 += 1
                    elif k == 2:
                        self.current_min_P2 += 1

                    self.capture_server(k)
                    
                    interval = self.gen_random_interval(k, self.t)
                    
                    if k == 1:
                        self.hist_t_P1.append(self.t)
                        self.hist_val_P1.append(interval)
                    else:
                        self.hist_t_P2.append(self.t)
                        self.hist_val_P2.append(interval)

                    self.Z[k] += interval

            self.check_mixed_priority_switch()

            for i in range(self.cfg.N):
                if self.D[i] != 0 and self.t >= self.U[i]:
                    self.D[i] = 0
                    self.extract_from_queue(i)

    def print_stats(self):
        print("\n=========================================================")
        print("   РЕЗУЛЬТАТЫ ИМИТАЦИОННОГО МОДЕЛИРОВАНИЯ (Вариант 20)")
        print("=========================================================")
        print(f"Время моделирования: {self.cfg.t_mod / 3600.0:.2f} ч.")

        for k in range(1, self.cfg.Q + 1):
            p_refusal = (self.P[k] / self.B[k]) if self.B[k] > 0 else 0.0
            p_accept = 1.0 - p_refusal
            
            p_name = "1 (Равномерный закон)" if k == 1 else "2 (Кубический закон)"

            print(f"\n--- Приоритет: {p_name} ---")
            print(f"  Всего заявок поступило: {self.B[k]}")
            print(f"  Заявок отклонено:       {self.P[k]}")
            print(f"  Вероятность отказа:     {p_refusal * 100:.2f}%")
            print(f"  Вероятность приема:     {p_accept * 100:.2f}%")

        print("\n=========================================================")
        print("   Статистика за 1 минуту (выборка)")
        
        if self.reqs_per_minute_P1:
            def print_min_info(min_idx):
                n1 = self.reqs_per_minute_P1[min_idx]
                n2 = self.reqs_per_minute_P2[min_idx]
                print(f"    Поток P1: {n1} заявок | Поток P2: {n2} заявок")

            print_min_info(0)
        print("=========================================================\n")

    def get_smoothed_data(self, t_list, val_list, bin_size=600):
        if not t_list:
            return [],[]
        
        bins = {}
        for t, v in zip(t_list, val_list):
            b = int(t / bin_size) * bin_size
            if b not in bins:
                bins[b] = []
            bins[b].append(v)
            
        sorted_bins = sorted(bins.keys())
        t_smooth = sorted_bins
        val_smooth = [sum(bins[b]) / len(bins[b]) for b in sorted_bins]
        return t_smooth, val_smooth

    def show_plots(self):
        # -------------------------------------------------------------
        # График 1: Интенсивность заявок (Сетка 2x2)
        # -------------------------------------------------------------
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        (ax1, ax2), (ax3, ax4) = axes
        
        minutes = range(1, len(self.reqs_per_minute_P1) + 1)
        hours = [m / 60.0 for m in minutes]
        
        # Верхний ряд: Суточная интенсивность
        ax1.plot(hours, self.reqs_per_minute_P1, color='blue', alpha=0.8, linewidth=1.5)
        ax1.set_title('Интенсивность за сутки: Приоритет 1')
        ax1.set_xlabel('Время (часы)')
        ax1.set_ylabel('Заявок в минуту')
        ax1.grid(True)
        
        ax2.plot(hours, self.reqs_per_minute_P2, color='red', alpha=0.8, linewidth=1.5)
        ax2.set_title('Интенсивность за сутки: Приоритет 2')
        ax2.set_xlabel('Время (часы)')
        ax2.set_ylabel('Заявок в минуту')
        ax2.grid(True)

        # Нижний ряд: Интенсивность за 1-ю минуту (с 0 по 60 секунду)
        # Отфильтруем заявки, которые пришли в первые 60 секунд
        t_1min_P1 =[t for t in self.hist_t_P1 if t <= 60.0]
        t_1min_P2 = [t for t in self.hist_t_P2 if t <= 60.0]

        # Строим гистограмму, где 1 столбик = 1 секунда
        ax3.hist(t_1min_P1, bins=60, range=(0, 60), color='blue', alpha=0.8, edgecolor='black', linewidth=0.5)
        ax3.set_title('Интенсивность в 1-ю минуту: Приоритет 1')
        ax3.set_xlabel('Время (секунды)')
        ax3.set_ylabel('Заявок в секунду')
        ax3.grid(True, alpha=0.5)

        ax4.hist(t_1min_P2, bins=60, range=(0, 60), color='red', alpha=0.8, edgecolor='black', linewidth=0.5)
        ax4.set_title('Интенсивность в 1-ю минуту: Приоритет 2')
        ax4.set_xlabel('Время (секунды)')
        ax4.set_ylabel('Заявок в секунду')
        ax4.grid(True, alpha=0.5)
        
        plt.tight_layout()
        plt.show()

        # -------------------------------------------------------------
        # График 2: Динамика изменения интервалов (оставлен без изменений)
        # -------------------------------------------------------------
        fig, axes = plt.subplots(3, 1, figsize=(12, 10))
        
        bin_size = 600
        t_sm_P1, v_sm_P1 = self.get_smoothed_data(self.hist_t_P1, self.hist_val_P1, bin_size)
        t_sm_P2, v_sm_P2 = self.get_smoothed_data(self.hist_t_P2, self.hist_val_P2, bin_size)
        t_sm_mu, v_sm_mu = self.get_smoothed_data(self.hist_t_mu, self.hist_val_mu, bin_size)

        def to_hours(t_list):
            return[t / 3600.0 for t in t_list]

        axes[0].plot(to_hours(self.hist_t_P1), self.hist_val_P1, color='blue', alpha=0.1, linewidth=0.3, label='Фактические данные')
        axes[0].plot(to_hours(t_sm_P1), v_sm_P1, color='darkblue', linewidth=2, label='Среднее за 10 мин')
        axes[0].set_title('Динамика интервалов: 1 приоритет (Равномерный закон)')
        axes[0].set_ylabel('Интервал (с)')
        axes[0].legend(loc='upper right')
        axes[0].grid(True)
        
        axes[1].plot(to_hours(self.hist_t_P2), self.hist_val_P2, color='red', alpha=0.1, linewidth=0.3, label='Фактические данные')
        axes[1].plot(to_hours(t_sm_P2), v_sm_P2, color='darkred', linewidth=2, label='Среднее за 10 мин')
        axes[1].set_title('Динамика интервалов: 2 приоритет (Кубический закон)')
        axes[1].set_ylabel('Интервал (с)')
        axes[1].legend(loc='upper right')
        axes[1].grid(True)

        axes[2].plot(to_hours(self.hist_t_mu), self.hist_val_mu, color='green', alpha=0.1, linewidth=0.3, label='Фактические данные')
        axes[2].plot(to_hours(t_sm_mu), v_sm_mu, color='darkgreen', linewidth=2, label='Среднее за 10 мин')
        axes[2].set_title('Время обработки (Экспоненциальный закон)')
        axes[2].set_xlabel('Время (часы)')
        axes[2].set_ylabel('Время обработки (с)')
        axes[2].legend(loc='upper right')
        axes[2].grid(True)

        plt.tight_layout()
        plt.show()

if __name__ == "__main__":
    my_config = Config()
    model = SMO(my_config)
    model.run()
    model.print_stats()
    model.show_plots()