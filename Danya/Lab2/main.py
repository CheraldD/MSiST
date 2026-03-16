import matplotlib.pyplot as plt
import matplotlib.patches as patches
import random

# ==========================================
# КОНФИГУРАЦИЯ (Изначальные параметры)
# ==========================================
class Config:
    def __init__(self):
        self.N = 2               # Количество приборов
        self.L = 6               # Максимальная длина очереди
        self.Q = 3               # Количество приоритетов (0 - высший, 1 - средний, 2 - низший)
        self.W = 3               # Порог переключения на абсолютный приоритет
        self.t_mod = 40.0        # Время моделирования
        self.dt = 0.05           # Шаг
        self.server_mus = [3.5, 5.0] # Время обслуживания для каждого прибора
        self.initial_Z = [2, 1.0, 1.5]

def gen_random_interval(prio_index, current_time):
    # Сценарий "Искусственный затор"
    
    if prio_index == 2: # Низший приоритет
        if current_time < 5.0:
            return 0.5  # В начале активно забиваем систему
        else:
            return 10.0 # Потом затихаем
            
    elif prio_index == 1: # Средний приоритет
        if 10.0 <= current_time <= 20.0:
            return 0.4  # Забиваем очередь, когда приборы уже заняты P3
        else:
            return 15.0
            
    elif prio_index == 0: # Высший приоритет
        if current_time >= 20.0:
            return 0.2  # Резкий наплыв VIP-заявок для активации порога W
        else:
            return 20.0

# ==========================================
# МОДЕЛЬ
# ==========================================
class SMO_Model:
    def __init__(self, cfg):
        self.cfg = cfg
        self.t = 0.0
        self.Z = list(cfg.initial_Z)
        
        self.D = [None] * cfg.N  
        self.U = [0.0] * cfg.N
        self.task_start = [0.0] * cfg.N
        
        self.j = [0] * cfg.Q
        self.B = [0] * cfg.Q
        self.P = [0] * cfg.Q
        
        # Динамическое создание логов под любое количество Q и N
        self.log_arrivals = {k: [] for k in range(cfg.Q)}  
        self.log_server   = {i: [] for i in range(cfg.N)} 
        self.log_queue    = {'t': []}
        for k in range(cfg.Q):
            self.log_queue[k] = []
        
        self.log_drops    = {k: [] for k in range(cfg.Q)} 
        self.log_preempts = [] # Хранит (время, индекс_прибора)

    def put_in_queue(self, k, is_new_arrival=True):
        curr_len = sum(self.j)
        if curr_len < self.cfg.L:
            self.j[k] += 1
            if is_new_arrival: self.log_arrivals[k].append(self.t)
        else:
            displaced = False
            for low_p in range(self.cfg.Q - 1, k, -1):
                if self.j[low_p] > 0:
                    self.j[low_p] -= 1
                    self.P[low_p] += 1
                    
                    # ПРИЧИНА 2: Удалена из очереди более важной заявкой
                    self.log_drops[low_p].append((self.t, 2)) 
                    
                    self.j[k] += 1
                    if is_new_arrival: self.log_arrivals[k].append(self.t)
                    displaced = True
                    break
            
            if not displaced:
                self.P[k] += 1
                # ПРИЧИНА 1: Не влезла при поступлении. 
                # ПРИЧИНА 3: Не влезла после вытеснения из прибора (is_new_arrival=False).
                reason = 1 if is_new_arrival else 3
                self.log_drops[k].append((self.t, reason))

    def run(self):
        while self.t <= self.cfg.t_mod:
            self.t = round(self.t + self.cfg.dt, 2)
            
            # 1. Освобождение приборов
            for i in range(self.cfg.N):
                if self.D[i] is not None and self.t >= self.U[i]:
                    start = self.task_start[i]
                    self.log_server[i].append((start, round(self.t - start, 2), self.D[i]))
                    self.D[i] = None
            
            # 2. Поступление новых заявок
            for k in range(self.cfg.Q):
                if self.t >= self.Z[k]:
                    self.B[k] += 1
                    self.put_in_queue(k, is_new_arrival=True)
                    self.Z[k] = round(self.Z[k] + gen_random_interval(k, self.t), 2)

            # 3. Переключение на абсолютный приоритет (сброс с прибора)
            while self.j[0] >= self.cfg.W:
                highest_q = 0
                active_servers = [i for i in range(self.cfg.N) if self.D[i] is not None]
                if not active_servers: 
                    break
                
                # Ищем заявку с самым низким приоритетом на приборах
                worst_prio = max([self.D[i] for i in active_servers])
                
                if worst_prio > highest_q:
                    # Находим прибор с этой заявкой
                    s_idx = [i for i in active_servers if self.D[i] == worst_prio][0]
                    
                    start = self.task_start[s_idx]
                    if self.t > start:
                        self.log_server[s_idx].append((start, round(self.t - start, 2), self.D[s_idx]))
                    
                    preempted_prio = self.D[s_idx]
                    self.log_preempts.append((self.t, s_idx)) 
                    
                    # Возвращаем прерванную в очередь
                    self.put_in_queue(preempted_prio, is_new_arrival=False) 
                    
                    # Ставим новую
                    self.j[highest_q] -= 1
                    self.D[s_idx] = highest_q
                    self.task_start[s_idx] = self.t
                    self.U[s_idx] = round(self.t + self.cfg.server_mus[s_idx], 2)
                else:
                    break

            # 4. Относительный приоритет (заполнение свободных приборов)
            for i in range(self.cfg.N):
                if self.D[i] is None:
                    for k in range(self.cfg.Q):
                        if self.j[k] > 0:
                            self.j[k] -= 1
                            self.D[i] = k
                            self.task_start[i] = self.t
                            self.U[i] = round(self.t + self.cfg.server_mus[i], 2)
                            break 
            
            # 5. Логирование длины очереди
            self.log_queue['t'].append(self.t)
            for k in range(self.cfg.Q):
                self.log_queue[k].append(self.j[k])

        # Закрываем последние интервалы обслуживания
        for i in range(self.cfg.N):
            if self.D[i] is not None:
                start = self.task_start[i]
                self.log_server[i].append((start, round(self.t - start, 2), self.D[i]))


# ==========================================
# ВЫВОД СТАТИСТИКИ В КОНСОЛЬ
# ==========================================
def print_statistics(model):
    cfg = model.cfg
    print("\n" + "=" * 60)
    print(" ХАРАКТЕРИСТИКИ МОДЕЛИ")
    print("=" * 60)
    print(f"  Время (t_mod): {cfg.t_mod}с | Шаг (dt): {cfg.dt}с")
    print(f"  Приборов (N):  {cfg.N} | Очередь (L): {cfg.L}")
    print(f"  Приоритетов:   {cfg.Q} | Порог (W):   {cfg.W}")
    print(f"  Время обсл.:   {cfg.server_mus}")
    print("=" * 60)

    for prio in range(cfg.Q):
        arrivals = model.B[prio]
        total_drops = model.P[prio]
        
        drops_by_reason = {1: 0, 2: 0, 3: 0}
        for _, reason in model.log_drops[prio]:
            drops_by_reason[reason] += 1
            
        drop_prob = (total_drops / arrivals) if arrivals > 0 else 0.0
        
        print(f"--- ПРИОРИТЕТ {prio + 1} ---")
        print(f"  Поступило: {arrivals} | Отказов: {total_drops} (Вер: {drop_prob:.4f})")
        
        if total_drops > 0:
            if drops_by_reason[1] > 0: print(f"    - Причина 1 (Очередь полна):       {drops_by_reason[1]}")
            if drops_by_reason[2] > 0: print(f"    - Причина 2 (Вытеснена из очер.):  {drops_by_reason[2]}")
            if drops_by_reason[3] > 0: print(f"    - Причина 3 (Сброс с прибора):     {drops_by_reason[3]}")
        print()
        
    print(f"Всего прерываний на приборах: {len(model.log_preempts)}")
    print("=" * 60 + "\n")


# ==========================================
# КРАСИВАЯ ОТРИСОВКА
# ==========================================
def draw_plot(model):
    cfg = model.cfg
    colors = ['#3498db', '#e67e22', '#2ecc71'] # Синий, оранжевый, зеленый
    labels_prio = [f'P{i+1}' for i in range(cfg.Q)]

    drop_styles = {
        1: {'c': '#555555', 'marker': 'X', 'label': 'Причина 1: Нет места'},
        2: {'c': '#e74c3c', 'marker': 'v', 'label': 'Причина 2: Вытеснена из очереди'},
        3: {'c': '#9b59b6', 'marker': '*', 'label': 'Причина 3: Сброс из прибора'}
    }

    fig, ax = plt.subplots(2 + cfg.N, 1, figsize=(12, 11), sharex=True, gridspec_kw={'height_ratios': [1, 2] + [0.8]*cfg.N})
    plt.subplots_adjust(hspace=0.3)
    fig.suptitle(f'СМО: Смешанный приоритет (L={cfg.L}, W={cfg.W})', fontsize=15, weight='bold')

    # Применяем легкий фон к осям
    for a in ax:
        a.set_facecolor('#fafafa')
        a.grid(axis='x', linestyle='--', alpha=0.4)

    # --- 1. Поступление заявок (Объединенный график) ---
    ax_arr = ax[0]
    ax_arr.set_title('Поступление заявок в систему', fontsize=11, loc='left', weight='bold')
    
    for k in range(cfg.Q):
        times = model.log_arrivals[k]
        ax_arr.scatter(times, [cfg.Q - k]*len(times), color=colors[k], marker='o', s=40, edgecolors='black', label=labels_prio[k], zorder=3)
        
        # Отрисовка отказов на графике поступлений
        for t_val, reason in model.log_drops[k]:
            st = drop_styles[reason]
            ax_arr.scatter(t_val, cfg.Q - k + 0.3, color=st['c'], marker=st['marker'], s=60, zorder=4)

    ax_arr.set_yticks(range(1, cfg.Q + 1))
    ax_arr.set_yticklabels(labels_prio[::-1])
    ax_arr.set_ylim(0.5, cfg.Q + 0.8)
    
    # Легенда для поступлений и отказов
    handles_drops = [plt.Line2D([0], [0], color='w', marker=v['marker'], markerfacecolor=v['c'], markersize=8) for k, v in drop_styles.items()]
    labels_drops = [v['label'] for k, v in drop_styles.items()]
    ax_arr.legend(handles_drops, labels_drops, loc='upper right', fontsize=8, ncol=3)

    # --- 2. Очереди ---
    ax_q = ax[1]
    ax_q.set_title(f'Динамика очереди (Максимум L={cfg.L})', fontsize=11, loc='left', weight='bold')
    
    t = model.log_queue['t']
    for k in range(cfg.Q):
        q_vals = model.log_queue[k]
        ax_q.step(t, q_vals, where='post', color=colors[k], linewidth=2.5, alpha=0.8, label=f'Очередь {labels_prio[k]}')
        ax_q.fill_between(t, q_vals, step='post', color=colors[k], alpha=0.05)

    ax_q.axhline(cfg.W, color='red', linestyle=':', linewidth=2, label=f'Порог W={cfg.W}')

    ax_q.set_ylabel('Заявок')
    ax_q.set_yticks(range(0, cfg.L + 1))
    ax_q.set_ylim(-0.2, cfg.L + 0.5)
    ax_q.grid(axis='y', linestyle='-', alpha=0.2)
    ax_q.legend(loc='upper left', fontsize=9)

    # --- 3. Занятость приборов ---
    for i in range(cfg.N):
        ax_srv = ax[2 + i]
        ax_srv.set_title(f'Прибор {i+1} (mu={cfg.server_mus[i]})', fontsize=10, loc='left', weight='bold')
        ax_srv.set_yticks([])
        ax_srv.set_ylim(0, 1)
        
        # Фоновая полоса
        ax_srv.add_patch(patches.Rectangle((0, 0.2), cfg.t_mod, 0.6, color='#eeeeee'))

        # Отрисовка обслуживания
        for (start, duration, prio) in model.log_server[i]:
            rect = patches.Rectangle(
                (start, 0.2), duration, 0.6,
                linewidth=1.2, edgecolor='#333333', facecolor=colors[prio], alpha=0.8, hatch='////'
            )
            ax_srv.add_patch(rect)
            if duration > 0.8:
                ax_srv.text(start + duration/2, 0.5, labels_prio[prio], color='white', ha='center', va='center', fontsize=9, weight='bold')

        # Отрисовка прерываний (вертикальные красные линии)
        for t_preempt, s_idx in model.log_preempts:
            if s_idx == i:
                ax_srv.axvline(t_preempt, color='red', linestyle='-', linewidth=2, zorder=5)

    ax[-1].set_xlabel('Время моделирования (сек)', weight='bold')
    plt.show()

# Запуск
cfg = Config()
mdl = SMO_Model(cfg)
mdl.run()

print_statistics(mdl)
draw_plot(mdl)