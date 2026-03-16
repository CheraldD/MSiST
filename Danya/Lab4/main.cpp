#include <iostream>
#include <vector>
#include <numeric>
#include <random>
#include <iomanip>
#include <cmath>
#include <functional>
#include <algorithm>

using namespace std;

// ==========================================
// Настройки модели
// ==========================================
struct Config {
    int N = 2;      // Количество приборов
    int L = 6;      // Максимальная длина очереди
    int Q = 3;      // Количество приоритетов

    int W = 3;      // Порог включения абсолютного приоритета

    double t_mod = 86400.0;    // 1 день (в секундах)
    double dt = 0.1;           // Шаг моделирования

    vector<double> server_mus = {8.5, 5.0};
    vector<double> initial_Z = {0.5, 1.0, 1.5};
};

class SMO {
private:
    Config cfg;
    double t;

    vector<double> Z; 
    vector<double> U; 
    vector<int> D;    

    vector<int> B;    // Поступило заявок
    vector<int> P;    // Отказано в обслуживании
    vector<int> j;    // Очередь (текущая)

    double get_uniform_0_1() {
        static random_device rd;
        static mt19937 gen(rd());
        static uniform_real_distribution<double> dist(0.00001, 0.99999); 
        return dist(gen);
    }

    double gen_quadratic(double M, double D) {
        double x = get_uniform_0_1();
        double cbrt_val = cbrt(2.0 * x - 1.0); 
        return sqrt((5.0 * D) / 3.0) * cbrt_val + M;
    }

    double gen_uniform_custom(double M, double D) {
        double b = (sqrt(12.0 * D) + 2.0 * M) / 2.0;
        double c = 2.0 * M - b;
        double x = get_uniform_0_1();
        return c + x * (b - c);
    }

    double gen_exponential(double M) {
        double lambda = 1.0 / M;
        double x = get_uniform_0_1();
        return -log(x) / lambda;
    }

    double get_valid_time(const function<double()>& gen_func) {
        double val;
        do { val = gen_func(); } while (val <= 0.0);
        return val;
    }

    double get_arrival_interval(int priority, double current_time) {
        double h = fmod(current_time, 86400.0) / 3600.0;
        bool is_day = (h >= 8.0 && h < 20.0); 

        if (priority == 0) {
            double M = is_day ? 4.0 : 4.5455;
            double D = is_day ? 16.0 : 20.6612;
            return get_valid_time([&](){ return gen_quadratic(M, D); });
        } 
        else if (priority == 1) {
            double M = is_day ? 4.807692 : 2.0;
            double D = is_day ? 24.02922 : 4.0;
            return get_valid_time([&](){ return gen_uniform_custom(M, D); });
        } 
        else {
            double M = is_day ? 4.807692 : 2.0;
            double D = is_day ? 24.02922 : 4.0;
            return get_valid_time([&](){ return gen_uniform_custom(M, D); });
        }
    }

public:
    SMO(Config config) : cfg(config) {
        t = 0.0;
        Z = cfg.initial_Z;
        U.resize(cfg.N, 0.0);
        D.resize(cfg.N, -1);
        B.resize(cfg.Q, 0);
        P.resize(cfg.Q, 0);
        j.resize(cfg.Q, 0);
    }

    void check_task(int k_index) {
        int current_queue_size = accumulate(j.begin(), j.end(), 0);
        if (current_queue_size < cfg.L) {
            j[k_index]++;
        } else {
            bool displaced = false;
            for (int low_prio = cfg.Q - 1; low_prio > k_index; --low_prio) {
                if (j[low_prio] > 0) {
                    j[low_prio]--;     
                    P[low_prio]++;     
                    j[k_index]++;      
                    displaced = true;
                    break;
                }
            }
            if (!displaced) P[k_index]++; 
        }
    }

    void free_finished_servers() {
        for (int i = 0; i < cfg.N; ++i) {
            if (D[i] != -1 && t >= U[i]) {
                D[i] = -1; 
            }
        }
    }

    void handle_arrivals() {
        for (int k = 0; k < cfg.Q; ++k) {
            if (t >= Z[k]) {
                B[k]++;
                bool placed = false;
                for (int i = 0; i < cfg.N; ++i) {
                    if (D[i] == -1) {
                        D[i] = k;
                        U[i] = t + get_valid_time([&](){ return gen_exponential(cfg.server_mus[i]); });
                        placed = true;
                        break;
                    }
                }
                if (!placed) check_task(k); 
                Z[k] += get_arrival_interval(k, t);
            }
        }
    }

    void process_queue_relative() {
        for (int i = 0; i < cfg.N; ++i) {
            if (D[i] == -1) { 
                for (int k = 0; k < cfg.Q; ++k) {
                    if (j[k] > 0) {
                        j[k]--; 
                        D[i] = k; 
                        U[i] = t + get_valid_time([&](){ return gen_exponential(cfg.server_mus[i]); });
                        break;
                    }
                }
            }
        }
    }

    void process_queue_absolute() {
        while (j[0] >= cfg.W) {
            int highest_q = -1;
            for(int q = 0; q < cfg.Q; ++q) {
                if(j[q] > 0) { highest_q = q; break; }
            }

            int lowest_s = -1, server_idx = -1;
            for(int i = 0; i < cfg.N; ++i) {
                if(D[i] > lowest_s) {
                    lowest_s = D[i];
                    server_idx = i;
                }
            }

            if (highest_q != -1 && lowest_s != -1 && highest_q < lowest_s) {
                int interrupted_prio = D[server_idx]; 
                D[server_idx] = highest_q;            
                j[highest_q]--;                       
                U[server_idx] = t + get_valid_time([&](){ return gen_exponential(cfg.server_mus[server_idx]); });
                check_task(interrupted_prio);         
            } else {
                break; 
            }
        }
    }

    void run() {
        while (t <= cfg.t_mod) {
            t += cfg.dt;
            free_finished_servers();    
            handle_arrivals();          
            process_queue_relative();   
            process_queue_absolute();   
        }
    }

    // Возвращает массив вероятностей отказов для данного прогона модели
    vector<double> get_rejection_probs() const {
        vector<double> probs(cfg.Q, 0.0);
        for (int k = 0; k < cfg.Q; ++k) {
            probs[k] = (B[k] > 0) ? (double)P[k] / B[k] : 0.0;
        }
        return probs;
    }
};

// ==========================================
// ЛАБОРАТОРНАЯ РАБОТА 4
// ==========================================
void run_lab4_experiments() {
    Config base_cfg;
    
    // Параметры для ВАРИАНТА 16
    int V1 = 17;                 // Базовый объем выборки (V1)
    double P_dov = 0.90;         // Доверительная вероятность
    double t_stud = 1.7450;      // Коэффициент Стьюдента для V=17, P_дов=0.90
    double delta_dop_rel = 0.12; // Допустимая относительная погрешность (12%)

    cout << "==========================================================" << endl;
    cout << "      ЛАБОРАТОРНАЯ РАБОТА 4 (Вариант 16)                  " << endl;
    cout << "==========================================================" << endl;
    cout << "Параметры: V1 = " << V1 << ", P_дов = " << P_dov << ", t_стьюд = " << t_stud << ", Delta_доп = 12%\n" << endl;

    // ------------------------------------------------------------------
    // Задание 1. Вычисление мат. ожидания, дисперсии и дов. интервала
    // РЕАЛИЗАЦИЯ ДВУХЭТАПНОГО ПРОГОНА
    // ------------------------------------------------------------------
    cout << "---> Задание 1: Статистическая обработка (двухэтапный прогон)" << endl;
    cout << "ЭТАП 1: Проводим базовые " << V1 << " прогонов симуляции (пожалуйста, подождите)..." << endl;

    // Используем динамические массивы, чтобы добавлять новые результаты
    vector<vector<double>> all_p_otk(base_cfg.Q);

    // Выполняем базовый объем прогонов V1
    for (int v = 0; v < V1; ++v) {
        SMO model(base_cfg);
        model.run();
        vector<double> p = model.get_rejection_probs();
        for (int q = 0; q < base_cfg.Q; ++q) {
            all_p_otk[q].push_back(p[q]);
        }
    }

    // Анализ результатов первого этапа: ищем, нужен ли добор выборки (V2)
    int V_required = V1; // По умолчанию считаем, что V1 достаточно
    vector<double> D_stage1(base_cfg.Q, 0.0);
    vector<double> mean_stage1(base_cfg.Q, 0.0);

    for (int q = 0; q < base_cfg.Q; ++q) {
        double sum = 0;
        for (double p : all_p_otk[q]) sum += p;
        mean_stage1[q] = sum / V1;

        if (mean_stage1[q] > 0.0) { // Если были отказы
            double sum_sq = 0;
            for (double p : all_p_otk[q]) sum_sq += pow(p - mean_stage1[q], 2);
            D_stage1[q] = sum_sq / (V1 - 1); // Дисперсия
            
            double delta = t_stud * sqrt(D_stage1[q] / V1); // Дов. интервал
            double absolute_delta_dop = mean_stage1[q] * delta_dop_rel; // Допустимая погрешность (12%)

            // Если точность не достигнута, вычисляем V2 по формуле (1.5)
            if (delta > absolute_delta_dop) {
                int V2_q = ceil(D_stage1[q] * pow(t_stud, 2) / pow(absolute_delta_dop, 2));
                if (V2_q > V_required) {
                    V_required = V2_q; // Запоминаем максимальный требуемый объем
                }
            }
        }
    }

    // ЭТАП 2: Добор реализаций
    if (V_required > V1) {
        cout << "\nЭТАП 2: Точность 12% не достигнута на объеме V1=" << V1 << "." << endl;
        cout << "Рассчитан необходимый объем выборки V2 = " << V_required << endl;
        cout << "Добираем недостающие " << (V_required - V1) << " реализаций..." << endl;
        
        for (int v = V1; v < V_required; ++v) {
            SMO model(base_cfg);
            model.run();
            vector<double> p = model.get_rejection_probs();
            for (int q = 0; q < base_cfg.Q; ++q) {
                all_p_otk[q].push_back(p[q]);
            }
        }
    } else {
        cout << "\nЭТАП 2: Точность 12% успешно достигнута на базовой выборке (V=" << V1 << ")." << endl;
        cout << "Добор реализаций не требуется." << endl;
    }

    // Вывод итоговых результатов (на основе итогового объема final_V)
    int final_V = all_p_otk[0].size();
    cout << "\nИТОГОВЫЕ РЕЗУЛЬТАТЫ (Объем выборки V = " << final_V << "):" << endl;

    for (int q = 0; q < base_cfg.Q; ++q) {
        double sum = 0;
        for (double p : all_p_otk[q]) sum += p;
        double P_otk_mean = sum / final_V;

        bool all_zero = true;
        for (double p : all_p_otk[q]) if (p > 0) all_zero = false;

        double D = 0, delta = 0;
        if (all_zero) {
            // Формула 1.4
            delta = -log(1.0 - P_dov) / final_V;
        } else {
            // Формула 1.3
            double sum_sq = 0;
            for (double p : all_p_otk[q]) sum_sq += pow(p - P_otk_mean, 2);
            D = sum_sq / (final_V - 1);
            // Формула 1.2
            delta = t_stud * sqrt(D / final_V);
        }

        cout << "Приоритет " << (q + 1) << ":" << endl;
        cout << fixed << setprecision(6);
        cout << "  Мат. ожидание (P_отк): " << P_otk_mean << endl;
        cout << "  Дисперсия (D):         " << D << endl;
        cout << "  Граница интервала (A): " << delta << endl;
        cout << "  Дов. интервал:[" << max(0.0, P_otk_mean - delta) << "; " << (P_otk_mean + delta) << "]" << endl;
        
        if (P_otk_mean > 0.0) {
            double absolute_delta_dop = P_otk_mean * delta_dop_rel;
            cout << "  Требуемая точность:    " << absolute_delta_dop;
            if (delta <= absolute_delta_dop) cout << " (ВЫПОЛНЕНО)" << endl;
            else cout << " (МАКСИМАЛЬНО ПРИБЛИЖЕНО)" << endl;
        } else {
            cout << "  Требуемая точность:    Неприменимо (нет отказов)" << endl;
        }
        cout << endl;
    }

    // ------------------------------------------------------------------
    // Задание 4. Поиск значения W
    // ------------------------------------------------------------------
    cout << "---> Задание 4: Поиск порога переключения приоритетов (W)" << endl;
    cout << "Условие: P_отк1 <= 0.1, P_отк2 <= 0.2, P_отк3 <= 0.3" << endl;
    
    int optimal_W = -1;
    
    // Для ускорения поиска W используем базовый объем V1
    for (int w = 0; w <= base_cfg.L + 1; ++w) {
        base_cfg.W = w;
        cout << "Проверка W = " << w << "... ";
        
        double sum_p[3] = {0, 0, 0};
        for (int v = 0; v < V1; ++v) {
            SMO model(base_cfg);
            model.run();
            vector<double> p = model.get_rejection_probs();
            for (int q = 0; q < base_cfg.Q; ++q) sum_p[q] += p[q];
        }
        
        double mean_p[3];
        for(int q=0; q < 3; ++q) mean_p[q] = sum_p[q] / V1;

        cout << "Ср. отказы: P1=" << fixed << setprecision(4) << mean_p[0] 
             << ", P2=" << mean_p[1] << ", P3=" << mean_p[2] << " -> ";

        if (mean_p[0] <= 0.1 && mean_p[1] <= 0.2 && mean_p[2] <= 0.3) {
            cout << "ПОДХОДИТ!" << endl;
            if (optimal_W == -1) optimal_W = w; 
        } else {
            cout << "Не подходит" << endl;
        }
    }
    
    cout << "\n==========================================================" << endl;
    if (optimal_W != -1) {
        cout << "ВЫВОД: Требуется установить порог абсолютного приоритета W = " << optimal_W << endl;
    } else {
        cout << "ВЫВОД: Требуемые условия не достигаются ни при одном W." << endl;
    }
    cout << "==========================================================" << endl;
}

int main() {
    setlocale(LC_ALL, "Russian");
    
    // Запуск всего комплекса расчетов для 4 лабораторной
    run_lab4_experiments();

    return 0;
}