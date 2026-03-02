import matplotlib.pyplot as plt
import matplotlib.patches as patches
import random

# ==========================================
# КОНФИГУРАЦИЯ
# ==========================================
class Config:
    def __init__(self):
        self.N = 1
        self.L = 6          # УМЕНЬШЕНО ДО 4, чтобы быстрее увидеть переполнение и отказы всех типов
        self.Q = 2
        self.W = 2          # Порог переключения на абсолютный приоритет
        self.t_mod = 25.0   
        self.dt = 0.1       
        self.mu = 15.0       # Длинное обслуживание, чтобы прибор был занят
        self.initial_Z = [5.0, 0.5] 

def gen_random_interval(prio_index):
    if prio_index == 0:
        # ВЫСШИЙ: приходит реже, чтобы дать очереди заполниться низшим приоритетом
        return random.uniform(2.5, 3.5) 
    else:
        # НИЗШИЙ: очень частый поток, чтобы быстро забить очередь L=6
        return random.uniform(0.6, 1.0)

# ==========================================
# МОДЕЛЬ
# ==========================================
class SMO_Model:
    def __init__(self, cfg):
        self.cfg = cfg
        self.t = 0.0
        self.Z = list(cfg.initial_Z)
        
        self.D = [None] * cfg.N  
        self.U =[0.0] * cfg.N
        self.task_start =[0.0] * cfg.N
        
        self.j = [0] * cfg.Q
        self.B = [0] * cfg.Q
        self.P = [0] * cfg.Q
        
        self.log_arrivals = {0: [], 1:[]}  
        self.log_server   = {i:[] for i in range(cfg.N)} 
        self.log_queue    = {'t':[], 0:[], 1:[]}
        
        # Теперь лог отказов хранит кортежи (время, код_причины)
        self.log_drops    = {0:[], 1:[]} 
        self.log_preempts =[]

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
            
            for i in range(self.cfg.N):
                if self.D[i] is not None and self.t >= self.U[i]:
                    start = self.task_start[i]
                    self.log_server[i].append((start, round(self.t - start, 2), self.D[i]))
                    self.D[i] = None
            
            for k in range(self.cfg.Q):
                if self.t >= self.Z[k]:
                    self.B[k] += 1
                    self.put_in_queue(k, is_new_arrival=True)
                    self.Z[k] = round(self.Z[k] + gen_random_interval(k), 2)

            if self.j[0] >= self.cfg.W:
                for i in range(self.cfg.N):
                    if self.D[i] == 1 and self.j[0] > 0: 
                        start = self.task_start[i]
                        if self.t > start:
                            self.log_server[i].append((start, round(self.t - start, 2), self.D[i]))
                        
                        preempted_prio = self.D[i]
                        self.log_preempts.append(self.t) 
                        
                        # Возвращаем прерванную в очередь (может триггернуть Причину 3)
                        self.put_in_queue(preempted_prio, is_new_arrival=False) 
                        
                        self.j[0] -= 1
                        self.D[i] = 0
                        self.task_start[i] = self.t
                        self.U[i] = round(self.t + self.cfg.mu, 2)

            for i in range(self.cfg.N):
                if self.D[i] is None:
                    for k in range(self.cfg.Q):
                        if self.j[k] > 0:
                            self.j[k] -= 1
                            self.D[i] = k
                            self.task_start[i] = self.t
                            self.U[i] = round(self.t + self.cfg.mu, 2)
                            break 
            
            self.log_queue['t'].append(self.t)
            self.log_queue[0].append(self.j[0])
            self.log_queue[1].append(self.j[1])

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
    print(f"  Время моделирования (t_mod):        {cfg.t_mod} сек")
    print(f"  Шаг моделирования (dt):             {cfg.dt} сек")
    print(f"  Количество приборов (N):            {cfg.N}")
    print(f"  Максимальная длина очереди (L):     {cfg.L}")
    print(f"  Количество приоритетов (Q):         {cfg.Q}")
    print(f"  Порог переключения (W):             {cfg.W} (Вариант 20)")
    print(f"  Время обслуживания прибором (mu):   {cfg.mu} сек")
    
    print("\n" + "=" * 60)
    print(" РЕЗУЛЬТАТЫ МОДЕЛИРОВАНИЯ")
    print("=" * 60)

    for prio in range(cfg.Q):
        prio_name = "Высший (1)" if prio == 0 else "Низший (2)"
        arrivals = model.B[prio]
        total_drops = model.P[prio]
        
        # Подсчет причин отказов
        drops_by_reason = {1: 0, 2: 0, 3: 0}
        for _, reason in model.log_drops[prio]:
            drops_by_reason[reason] += 1
            
        drop_prob = (total_drops / arrivals) if arrivals > 0 else 0.0
        
        print(f"--- ПРИОРИТЕТ: {prio_name} ---")
        print(f"  Поступило заявок:    {arrivals}")
        print(f"  Всего отказов:       {total_drops} (Вероятность: {drop_prob:.4f})")
        
        if total_drops > 0:
            print("  В том числе по причинам:")
            if drops_by_reason[1] > 0:
                print(f"    - Причина 1 (Нет места при поступлении):           {drops_by_reason[1]}")
            if drops_by_reason[2] > 0:
                print(f"    - Причина 2 (Вытеснена из очереди высш. приор.):   {drops_by_reason[2]}")
            if drops_by_reason[3] > 0:
                print(f"    - Причина 3 (Вытеснена из прибора и нет места):    {drops_by_reason[3]}")
        print()
        
    print(f"Количество вытеснений из прибора (абс. приоритет): {len(model.log_preempts)}")
    print("=" * 60 + "\n")


# ==========================================
# КРАСИВАЯ ОТРИСОВКА
# ==========================================
def draw_plot(model):
    cfg = model.cfg
    colors =['#2ca02c', '#d62728'] 
    labels_prio =['Высший (1)', 'Низший (2)']

    # Настройки для трех причин отказа
    drop_styles = {
        1: {'c': 'black', 'ls': ':',   'label': 'Причина 1: Нет места'},
        2: {'c': 'red',   'ls': '--',  'label': 'Причина 2: Вытеснена из очереди'},
        3: {'c': 'purple','ls': '-.',  'label': 'Причина 3: Сброс из прибора, нет места'}
    }

    fig, ax = plt.subplots(4, 1, figsize=(12, 10), sharex=True)
    plt.subplots_adjust(hspace=0.3)
    fig.suptitle(f'СМО со смешанным приоритетом (L={cfg.L}, W={cfg.W})', fontsize=14, weight='bold', y=0.95)

    # --- 1 & 2. Поступление заявок ---
    for k in[0, 1]:
        ax[k].set_title(f'Поступление заявок: {labels_prio[k]}', fontsize=11, loc='left', color=colors[k], weight='bold')
        times = model.log_arrivals[k]
        drops = model.log_drops[k] # Теперь тут кортежи (время, причина)
        
        # Линии успешных поступлений
        ax[k].vlines(times, 0, 1, color=colors[k], linewidth=1.5, alpha=0.8)
        for t_val in times:
            ax[k].text(t_val, 1.05, f"{t_val:.1f}", rotation=90, fontsize=8, ha='center', va='bottom', color='#333')

        # Линии отказов (в зависимости от причины)
        for t_val, reason in drops:
            style = drop_styles[reason]
            # Чуть приподнимаем линию причины 3, чтобы она была заметнее, если они совпадают
            height = 0.7 if reason == 3 else 0.5 
            ax[k].vlines(t_val, 0, height, color=style['c'], linestyles=style['ls'], linewidth=2)
            ax[k].text(t_val, height + 0.1, style['label'].split(':')[0], color=style['c'], fontsize=7, rotation=90)

        ax[k].set_ylim(0, 1.6)
        ax[k].set_yticks([])
        for spine in['top', 'right', 'left']: ax[k].spines[spine].set_visible(False)
        ax[k].grid(axis='x', linestyle='--', alpha=0.3)

    # Добавляем общую легенду отказов на график высшего приоритета
    handles = [plt.Line2D([0], [0], color=v['c'], linestyle=v['ls'], lw=2) for k, v in drop_styles.items()]
    labels = [v['label'] for k, v in drop_styles.items()]
    ax[0].legend(handles, labels, loc='upper right', fontsize=8, framealpha=0.9)

    # --- 3. Занятость прибора (С Вытеснением) ---
    ax_srv = ax[2]
    ax_srv.set_title('Занятость прибора (Оранжевый пунктир = Вытеснение)', fontsize=11, loc='left', weight='bold')
    ax_srv.set_yticks([])
    ax_srv.set_ylim(0, 1)
    for spine in ['top', 'right', 'left']: ax_srv.spines[spine].set_visible(False)
    
    ax_srv.add_patch(patches.Rectangle((0, 0.2), cfg.t_mod, 0.6, color='#f0f0f0'))

    for (start, duration, prio) in model.log_server[0]:
        rect = patches.Rectangle(
            (start, 0.2), duration, 0.6,
            linewidth=1, edgecolor='black', facecolor=colors[prio], alpha=0.9
        )
        ax_srv.add_patch(rect)
        if duration > 0.5:
            ax_srv.text(start + duration/2, 0.5, str(prio+1), color='white', ha='center', va='center', fontsize=10, weight='bold')

    for t_preempt in model.log_preempts:
        ax_srv.axvline(t_preempt, color='orange', linestyle='--', ymin=0.1, ymax=0.9, linewidth=2)
        # Молния удалена

    # --- 4. Очереди ---
    ax_q = ax[3]
    ax_q.set_title(f'Динамика очереди (Макс. L={cfg.L})', fontsize=11, loc='left', weight='bold')
    
    t = model.log_queue['t']
    q1 = model.log_queue[0]
    q2 = model.log_queue[1]

    ax_q.step(t, q1, where='post', color=colors[0], linewidth=2, label='Очередь 1 (Высший)')
    ax_q.step(t, q2, where='post', color=colors[1], linewidth=2, label='Очередь 2 (Низший)', linestyle='--')
    ax_q.fill_between(t, q1, step='post', color=colors[0], alpha=0.1)

    ax_q.axhline(cfg.W, color='purple', linestyle=':', linewidth=1.5, label=f'Порог W={cfg.W}')

    # Крестики отказов в очереди (красим в цвет причины)
    for t_drop, reason in model.log_drops[1]:
        ax_q.plot(t_drop, sum([j for j in[q1[model.log_queue['t'].index(t_drop)], q2[model.log_queue['t'].index(t_drop)]]]), 
                  'X', color=drop_styles[reason]['c'], markersize=8, label='_nolegend_')

    ax_q.set_ylabel('Заявок в очереди')
    ax_q.set_xlabel('Время моделирования (сек)')
    ax_q.set_yticks(range(0, cfg.L + 2))
    ax_q.set_ylim(-0.1, cfg.L + 0.5)
    ax_q.grid(True, linestyle='-', alpha=0.2)
    ax_q.legend(loc='upper left', frameon=True, fancybox=True, shadow=True, fontsize=9)

    plt.show()

# Запуск
cfg = Config()
mdl = SMO_Model(cfg)
mdl.run()

# Вызов новой функции со статистикой
print_statistics(mdl)

# Отрисовка графиков
draw_plot(mdl)