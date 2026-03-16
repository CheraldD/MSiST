import random
import math
import matplotlib.pyplot as plt

class Config:
    def __init__(self):
        self.N = 2          # Количество приборов (Вариант 16)
        self.L = 6          # Максимальная длина очереди
        self.Q = 3          # Количество приоритетов (1 - высший, 3 - низший)
        
        self.W = 3          # Порог включения абсолютного приоритета

        self.t_mod = 86400.0   # 1 сутки (в секундах)
        self.dt = 0.1          # Шаг моделирования

        self.server_mus = [3.5, 5.0] # Мат. ожидание для приборов

        # Начальные моменты (индекс 0 не используется для удобства)
        self.initial_Z =[0.0, 0.5, 1.0, 1.5]

class SMO:
    def __init__(self, cfg):
        self.cfg = cfg
        self.t = 0.0

        self.Z = self.cfg.initial_Z.copy()
        self.U =[0.0] * self.cfg.N
        self.D = [0] * self.cfg.N

        self.B = [0] * (self.cfg.Q + 1)
        self.P = [0] * (self.cfg.Q + 1)
        self.j = [0] * (self.cfg.Q + 1)

        # Списки для хранения статистики за каждую минуту
        self.last_recorded_minute = 0
        self.current_min = [0] * (self.cfg.Q + 1)
        self.reqs_per_minute = [[] for _ in range(self.cfg.Q + 1)]

        # Списки для сохранения времени прихода (для поминутного графика)
        self.hist_t = [[] for _ in range(self.cfg.Q + 1)]

    def generate_exponential(self, mean):
        while True:
            val = random.expovariate(1.0 / mean)
            if val > 0:
                return val

    def generate_quadratic(self, M, D):
        while True:
            x = random.random()
            # Вычисление кубического корня с сохранением знака
            v = 2.0 * x - 1.0
            cbrt_val = math.copysign(abs(v)**(1/3), v)
            val = math.sqrt((5.0 * D) / 3.0) * cbrt_val + M
            if val > 0:
                return val

    def generate_uniform_custom(self, M, D):
        b = (math.sqrt(12.0 * D) + 2.0 * M) / 2.0
        c = 2.0 * M - b
        while True:
            val = random.uniform(c, b)
            if val > 0:
                return val

    def gen_random_interval(self, prio_index, current_time):
        t_hour = (current_time % 86400.0) / 3600.0
        is_day = (8.0 <= t_hour < 20.0)

        if prio_index == 1:
            # Приоритет 1: Квадратичный закон
            M = 4.0 if is_day else 4.5455
            D = 16.0 if is_day else 20.6612
            return self.generate_quadratic(M, D)
        else:
            # Приоритеты 2 и 3: Равномерный закон
            M = 4.807692 if is_day else 2.0
            D = 24.02922 if is_day else 4.0
            return self.generate_uniform_custom(M, D)

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
                self.U[i] = self.t + self.generate_exponential(self.cfg.server_mus[i])
                return

        self.put_in_queue(k)

    def extract_from_queue(self, server_index):
        for k in range(1, self.cfg.Q + 1):
            if self.j[k] > 0:
                self.j[k] -= 1
                self.D[server_index] = k
                self.U[server_index] = self.t + self.generate_exponential(self.cfg.server_mus[server_index])
                return

    def check_mixed_priority_switch(self):
        while self.j[1] >= self.cfg.W:
            highest_q = 1
            lowest_s = 0
            server_idx = -1
            
            for i in range(self.cfg.N):
                if self.D[i] > lowest_s:
                    lowest_s = self.D[i]
                    server_idx = i

            if lowest_s > highest_q:
                preempted = self.D[server_idx]
                self.j[highest_q] -= 1
                self.D[server_idx] = highest_q
                self.U[server_idx] = self.t + self.generate_exponential(self.cfg.server_mus[server_idx])
                self.put_in_queue(preempted)
            else:
                break

    def run(self):
        while self.t <= self.cfg.t_mod:
            self.t += self.cfg.dt

            # Сбор поминутной статистики
            current_mod_minute = int(self.t / 60.0)
            if current_mod_minute > self.last_recorded_minute:
                for k in range(1, self.cfg.Q + 1):
                    self.reqs_per_minute[k].append(self.current_min[k])
                    self.current_min[k] = 0
                self.last_recorded_minute = current_mod_minute

            # Приход заявок
            for k in range(1, self.cfg.Q + 1):
                if self.t >= self.Z[k]:
                    self.B[k] += 1
                    self.current_min[k] += 1
                    self.hist_t[k].append(self.t)

                    self.capture_server(k)
                    self.Z[k] += self.gen_random_interval(k, self.t)

            self.check_mixed_priority_switch()

            # Освобождение приборов
            for i in range(self.cfg.N):
                if self.D[i] != 0 and self.t >= self.U[i]:
                    self.D[i] = 0
                    self.extract_from_queue(i)

    def print_stats(self):
        print("\n=========================================================")
        print("   РЕЗУЛЬТАТЫ ИМИТАЦИОННОГО МОДЕЛИРОВАНИЯ (Вариант 16)")
        print("=========================================================")
        
        laws = ["", "Квадратичный", "Равномерный", "Равномерный"]
        for k in range(1, self.cfg.Q + 1):
            p_refusal = (self.P[k] / self.B[k]) if self.B[k] > 0 else 0.0
            print(f"Приоритет {k} ({laws[k]} закон):")
            print(f"  Поступило: {self.B[k]:<6} | Отказов: {self.P[k]:<5} | Вер. обслуж: {(1 - p_refusal)*100:.2f}%")

    def show_plots(self):
        # Настройка стиля
        plt.style.use('seaborn-v0_8-darkgrid')
        
        # Создаем полотно: 2 строки, 3 колонки (под 3 приоритета)
        fig, axes = plt.subplots(2, 3, figsize=(16, 9))
        fig.suptitle('Графики интенсивностей поступления заявок', fontsize=16, fontweight='bold', y=0.98)
        
        # Цветовая палитра и подписи
        colors =['#1f77b4', '#ff7f0e', '#2ca02c'] # Синий, Оранжевый, Зеленый
        laws_names =["Квадратичный", "Равномерный", "Равномерный"]
        
        for idx in range(3):
            k = idx + 1 # Номер приоритета (1, 2, 3)
            color = colors[idx]
            
            # --- ВЕРХНИЙ РЯД: Интенсивность за сутки ---
            ax_day = axes[0, idx]
            minutes = range(1, len(self.reqs_per_minute[k]) + 1)
            hours = [m / 60.0 for m in minutes]
            
            ax_day.plot(hours, self.reqs_per_minute[k], color=color, alpha=0.9, linewidth=1.2)
            # Добавляем заливку под графиком для красоты
            ax_day.fill_between(hours, self.reqs_per_minute[k], color=color, alpha=0.15)
            
            ax_day.set_title(f'Приоритет {k} ({laws_names[idx]})\nЗаявок в минуту (Сутки)', fontsize=11)
            ax_day.set_xlabel('Время (часы)')
            ax_day.set_ylabel('Кол-во заявок')
            ax_day.set_xlim(0, 24)
            ax_day.set_xticks(range(0, 25, 4))
            
            # --- НИЖНИЙ РЯД: Интенсивность за 1-ю минуту ---
            ax_min = axes[1, idx]
            # Отбираем заявки только за первые 60 секунд
            t_1min = [t for t in self.hist_t[k] if t <= 60.0]
            
            # Гистограмма (1 бин = 1 секунда)
            ax_min.hist(t_1min, bins=60, range=(0, 60), color=color, alpha=0.8, edgecolor='black', linewidth=0.5)
            
            ax_min.set_title(f'Приоритет {k} ({laws_names[idx]})\nЗаявок в секунду (Первая минута)', fontsize=11)
            ax_min.set_xlabel('Время (секунды)')
            ax_min.set_ylabel('Кол-во заявок')
            ax_min.set_xlim(0, 60)
            ax_min.set_xticks(range(0, 61, 10))

        plt.tight_layout()
        plt.subplots_adjust(top=0.88) # Оставляем место под общий заголовок
        plt.show()

if __name__ == "__main__":
    my_config = Config()
    model = SMO(my_config)
    model.run()
    model.print_stats()
    model.show_plots()