#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include <cmath>

using namespace std;

struct Config {
    int N = 1;      // Количество приборов
    int L = 6;      // Максимальная длина очереди
    int Q = 2;      // Количество приоритетов (1 - высший, Q - низший)
    
    // Переключение на абсолютный приоритет при W заявок первого приоритета в очереди
    int W = 2;      

    // Время моделирования: 1 сутки (в секундах)
    double t_mod = 86400;   
    double dt = 0.1;        // Шаг моделирования

    double mu_mean = 2;  // Математическое ожидание времени обработки заявки

    // Начальные Z (моменты поступления)
    vector<double> initial_Z = {0.0, 5.0, 0.5}; 

    // --- ПАРАМЕТРЫ ЛАБОРАТОРНОЙ 4 (Вариант 20) ---
    int V_base = 20;                  // Базовый объем выборки
    double P_dov = 0.95;              // Доверительная вероятность
    double delta_dop_percent = 7.0;   // Допустимый доверительный интервал (%)
    double t_stud = 2.0930;           // Коэф. Стьюдента (V-1 = 19, P_dov = 0.95)
};

class SMO {
private:
    Config cfg;
    double t;

    vector<double> Z; 
    vector<double> U; 
    vector<int> D;    

    vector<int> B;    
    vector<int> P;    
    vector<int> j;    

    double generate_exponential(double mean) {
        static random_device rd;
        static mt19937 gen(rd());
        exponential_distribution<double> dist(1.0 / mean);
        double val;
        do { val = dist(gen); } while (val <= 0); 
        return val;
    }

    double generate_uniform(double M, double D_val) {
        static random_device rd;
        static mt19937 gen(rd());
        double c = M - sqrt(3 * D_val);
        double b = M + sqrt(3 * D_val);
        uniform_real_distribution<double> dist(c, b);
        double val;
        do { val = dist(gen); } while (val <= 0); 
        return val;
    }

    double generate_cubic(double M, double D_val) {
        static random_device rd;
        static mt19937 gen(rd());
        uniform_real_distribution<double> dist(0.0, 1.0);
        double val;
        do {
            double x = dist(gen);
            val = sqrt(37.5 * D_val) * (pow(x, 0.25) - 0.8) + M;
        } while (val <= 0); 
        return val;
    }

    double gen_random_interval(int prio_index, double current_time) {
        double t_hour = fmod(current_time, 86400.0) / 3600.0;
        bool is_day = (t_hour >= 8.0 && t_hour < 20.0);

        if (prio_index == 1) {
            double M = is_day ? 4.807692 : 2.0;
            double D_val = is_day ? 24.02922 : 4.0;
            return generate_uniform(M, D_val);
        } else {
            double M = is_day ? 5.208333 : 0.25;
            double D_val = is_day ? 27.12674 : 0.0625;
            return generate_cubic(M, D_val);
        }
    }

    void put_in_queue(int k) {
        int current_queue_size = 0;
        for (int i = 1; i <= cfg.Q; ++i) current_queue_size += j[i];

        if (current_queue_size < cfg.L) {
            j[k]++;
        } else {
            bool displaced = false;
            for (int low = cfg.Q; low > k; --low) {
                if (j[low] > 0) {
                    j[low]--;      
                    P[low]++;      
                    j[k]++;        
                    displaced = true;
                    break;
                }
            }
            if (!displaced) P[k]++; 
        }
    }

    void CaptureServer(int k) {
        for (int i = 0; i < cfg.N; ++i) {
            if (D[i] == 0) { 
                D[i] = k;
                U[i] = t + generate_exponential(cfg.mu_mean); 
                return;
            }
        }
        if (j[1] >= cfg.W) { 
            for (int lowest = cfg.Q; lowest > k; --lowest) {
                for (int i = 0; i < cfg.N; ++i) {
                    if (D[i] == lowest) { 
                        int preempted = D[i];
                        D[i] = k;             
                        U[i] = t + generate_exponential(cfg.mu_mean);
                        put_in_queue(preempted); 
                        return;
                    }
                }
            }
        }
        put_in_queue(k);
    }

    void ExtractFromQueue(int server_index) {
        for (int k = 1; k <= cfg.Q; ++k) {
            if (j[k] > 0) {
                j[k]--;
                D[server_index] = k;
                U[server_index] = t + generate_exponential(cfg.mu_mean);
                return;
            }
        }
    }

    void CheckMixedPrioritySwitch() {
        if (j[1] >= cfg.W) {
            for (int i = 0; i < cfg.N; ++i) {
                if (D[i] > 1) { 
                    if (j[1] > 0) { 
                        int preempted = D[i];
                        j[1]--; 
                        D[i] = 1; 
                        U[i] = t + generate_exponential(cfg.mu_mean);
                        put_in_queue(preempted); 
                    }
                }
            }
        }
    }

public:
    SMO(Config config) : cfg(config) {
        t = 0.0;
        Z = cfg.initial_Z;
        U.resize(cfg.N, 0.0); 
        D.resize(cfg.N, 0);   
        B.resize(cfg.Q + 1, 0);
        P.resize(cfg.Q + 1, 0);
        j.resize(cfg.Q + 1, 0);
    }

    void run() {
        while (t <= cfg.t_mod) {
            t += cfg.dt; 
            for (int k = 1; k <= cfg.Q; ++k) {
                if (t >= Z[k]) {
                    B[k]++;           
                    CaptureServer(k); 
                    Z[k] += gen_random_interval(k, t); 
                }
            }
            CheckMixedPrioritySwitch(); 
            for (int i = 0; i < cfg.N; ++i) {
                if (D[i] != 0 && t >= U[i]) { 
                    D[i] = 0;                 
                    ExtractFromQueue(i);      
                }
            }
        }
    }

    // Возвращает вероятность отказа для данного приоритета по результатам 1 прогона
    double get_p_refusal(int priority) {
        if (B[priority] == 0) return 0.0;
        return (double)P[priority] / B[priority];
    }
};

// Структура для хранения статистики по приоритету
struct PriStat {
    double Potk = 0;
    double D = 0;
    double delta = 0;
    int V_final = 0;
};

// Функция проведения статистического эксперимента
vector<PriStat> run_experiments(Config cfg, bool enable_two_stage) {
    int V = cfg.V_base;
    vector<vector<double>> p_results(cfg.Q + 1);
    vector<double> sum_p(cfg.Q + 1, 0.0);

    // БЛОК 1: Первоначальный прогон V раз
    for (int k = 0; k < V; ++k) {
        SMO model(cfg);
        model.run();
        for (int q = 1; q <= cfg.Q; ++q) {
            double p = model.get_p_refusal(q);
            p_results[q].push_back(p);
            sum_p[q] += p;
        }
    }

    vector<PriStat> stats(cfg.Q + 1);
    int max_V_need = V;

    for (int q = 1; q <= cfg.Q; ++q) {
        double Potk = sum_p[q] / V;
        
        // Исправленная дисперсия (Формула 1.3)
        double D = 0;
        for (double p : p_results[q]) {
            D += (p - Potk) * (p - Potk);
        }
        D /= (V - 1);

        bool all_zero = true;
        for (double p : p_results[q]) {
            if (p > 0.0) { all_zero = false; break; }
        }

        double delta;
        if (all_zero) {
            delta = -log(1.0 - cfg.P_dov) / V; // Формула 1.4
        } else {
            delta = cfg.t_stud * sqrt(D / V);  // Формула 1.2
        }

        // Вычисляем требуемое V для данного приоритета (Формула 1.5)
        double delta_dop_abs = (cfg.delta_dop_percent / 100.0) * Potk;
        int V_need_q = V;
        if (!all_zero && delta_dop_abs > 0) {
            V_need_q = ceil(D * pow(cfg.t_stud / delta_dop_abs, 2));
        }

        if (V_need_q > max_V_need) max_V_need = V_need_q;

        stats[q].Potk = Potk;
        stats[q].D = D;
        stats[q].delta = delta;
        stats[q].V_final = V;
    }

    // БЛОК 2: Двухэтапный прогон (Способ 1 из методички)
    if (enable_two_stage && max_V_need > V) {
        if (max_V_need > 100) max_V_need = 100; // Ограничитель, чтобы не ждать вечность
        cout << "\n[i] Автоматический останов: требуемый объем выборки V=" << max_V_need 
             << " > текущего V=" << V << ". Домоделируем...\n";
             
        int additional = max_V_need - V;
        for (int k = 0; k < additional; ++k) {
            SMO model(cfg);
            model.run();
            for (int q = 1; q <= cfg.Q; ++q) {
                double p = model.get_p_refusal(q);
                p_results[q].push_back(p);
                sum_p[q] += p;
            }
        }
        
        V = max_V_need;
        for (int q = 1; q <= cfg.Q; ++q) {
            double Potk = sum_p[q] / V;
            double D = 0;
            for (double p : p_results[q]) D += (p - Potk) * (p - Potk);
            D /= (V - 1);

            bool all_zero = true;
            for (double p : p_results[q]) {
                if (p > 0.0) { all_zero = false; break; }
            }

            double delta = all_zero ? -log(1.0 - cfg.P_dov) / V : cfg.t_stud * sqrt(D / V);
            
            stats[q].Potk = Potk;
            stats[q].D = D;
            stats[q].delta = delta;
            stats[q].V_final = V;
        }
    }

    return stats;
}

int main() {
    setlocale(LC_ALL, "Russian");
    Config cfg;

    cout << "=========================================================\n";
    cout << " ЗАДАНИЯ 1 и 3: СТАТИСТИЧЕСКАЯ ОБРАБОТКА (W = " << cfg.W << ")\n";
    cout << "=========================================================\n";
    
    // Выполняем эксперимент с разрешением увеличить выборку (двухэтапный прогон)
    auto stats = run_experiments(cfg, true);

    for (int q = 1; q <= cfg.Q; ++q) {
        cout << "\n--- Приоритет " << q << " ---" << endl;
        cout << "  Итоговый объем выборки (V):   " << stats[q].V_final << endl;
        cout << "  Математическое ожидание Pотк: " << stats[q].Potk << endl;
        cout << "  Дисперсия D(Pотк):            " << stats[q].D << endl;
        cout << "  Доверительный интервал (Δ):   " << stats[q].delta << endl;
        cout << "  Истинное значение Pотк лежит в диапазоне:[" 
             << max(0.0, stats[q].Potk - stats[q].delta) << " ; " 
             << stats[q].Potk + stats[q].delta << "]" << endl;
    }

    // Проверка условия по пункту 3
    cout << "\n[Задание 3] Условие: P_отк для 1-го приоритета <= 0.1" << endl;
    if (stats[1].Potk <= 0.1) {
        cout << " -> ВЫПОЛНЯЕТСЯ (Текущее значение: " << stats[1].Potk << ")\n";
    } else {
        cout << " -> НЕ ВЫПОЛНЯЕТСЯ (Текущее значение: " << stats[1].Potk << ")\n";
    }

    cout << "\n=========================================================\n";
    cout << " ЗАДАНИЕ 4: ПОИСК ОПТИМАЛЬНОГО ПОРОГА ПЕРЕКЛЮЧЕНИЯ (W)\n";
    cout << "=========================================================\n";
    
    int optimal_W = -1;
    for (int w = 1; w <= cfg.L; ++w) {
        cfg.W = w;
        // Для поиска W отключаем расширение выборки, чтобы процесс шел быстрее
        auto current_stats = run_experiments(cfg, false); 
        
        double p1 = current_stats[1].Potk;
        double p2 = current_stats[2].Potk;
        
        cout << "Проверка при W = " << w << ":\t P_отк(1) = " << fixed << setprecision(4) << p1 
             << ",\t P_отк(2) = " << p2 << "\n";
        
        // Условия задачи: 1 приоритет <= 0.1, 2 приоритет <= 0.2
        if (p1 <= 0.1 && p2 <= 0.2) {
            optimal_W = w;
            cout << "\n[!] НАЙДЕНО ОПТИМАЛЬНОЕ ЗНАЧЕНИЕ: W = " << optimal_W << "!\n";
            cout << "Оно обеспечивает требуемые вероятности отказов.\n";
            break;
        }
    }
    
    if (optimal_W == -1) {
        cout << "\n[!] Не удалось найти значение W, при котором выполняются все условия.\n";
    }

    return 0;
}