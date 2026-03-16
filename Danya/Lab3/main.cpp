#include <iostream>
#include <vector>
#include <numeric>
#include <random>
#include <iomanip>
#include <cmath>
#include <functional>

using namespace std;

// ==========================================
// Настройки модели
// ==========================================
struct Config {
    int N = 2;      // Количество приборов
    int L = 6;      // Максимальная длина очереди
    int Q = 3;      // Количество приоритетов

    int W = 3;      // Порог включения абсолютного приоритета

    // ТРЕБОВАНИЕ ЛР3: Время моделирования 1 день (в секундах)
    double t_mod = 86400.0;    // 24 часа * 60 минут * 60 секунд
    double dt = 0.1;           // Шаг моделирования (уменьшен для точности)

    // Математическое ожидание времени обслуживания для приборов
    // (Используется для экспоненциального распределения)
    vector<double> server_mus = {3.5, 5.0};

    // Начальные моменты для заявок
    vector<double> initial_Z = {0.5, 1.0, 1.5};
};

class SMO {
private:
    Config cfg;
    double t;

    // Переменные состояния
    vector<double> Z; // Моменты поступления заявок
    vector<double> U; // Время освобождения приборов
    vector<int> D;    // Приоритет заявки на приборе (-1 если свободен)

    // Статистика
    vector<int> B;    // Поступило заявок
    vector<int> P;    // Отказано в обслуживании
    vector<int> j;    // Очередь (текущая)

    // Данные для графиков (Лаба 3, Задание 2)
    vector<vector<int>> req_per_minute; // Заявок в минуту по приоритетам
    vector<int> current_minute_reqs;    // Счетчик для текущей минуты
    double next_minute_mark;            // Отсечка следующей минуты

    // Базовый генератор U(0, 1)
    double get_uniform_0_1() {
        static random_device rd;
        static mt19937 gen(rd());
        static uniform_real_distribution<double> dist(0.00001, 0.99999); // Избегаем 0 и 1
        return dist(gen);
    }

    // ТРЕБОВАНИЕ ЛР3: Математические законы распределения по формулам из методички
    double gen_quadratic(double M, double D) {
        double x = get_uniform_0_1();
        double cbrt_val = cbrt(2.0 * x - 1.0); // Кубический корень
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

    // Функция-обертка для отбрасывания неподходящих значений (<= 0)
    double get_valid_time(const function<double()>& gen_func) {
        double val;
        do {
            val = gen_func();
        } while (val <= 0.0);
        return val;
    }

    // Генерация интервала поступления заявки в зависимости от приоритета и времени суток (Вариант 16)
    double get_arrival_interval(int priority, double current_time) {
        // Переводим текущее время в часы текущих суток[0, 24)
        double h = fmod(current_time, 86400.0) / 3600.0;
        bool is_day = (h >= 8.0 && h < 20.0); // Время от 8 до 20

        if (priority == 0) {
            // Приоритет 1: Квадратичный закон
            double M = is_day ? 4.0 : 4.5455;
            double D = is_day ? 16.0 : 20.6612;
            return get_valid_time([&](){ return gen_quadratic(M, D); });
        } 
        else if (priority == 1) {
            // Приоритет 2: Равномерный закон
            double M = is_day ? 4.807692 : 2.0;
            double D = is_day ? 24.02922 : 4.0;
            return get_valid_time([&](){ return gen_uniform_custom(M, D); });
        } 
        else {
            // Приоритет 3: Равномерный закон
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

        req_per_minute.resize(cfg.Q);
        current_minute_reqs.resize(cfg.Q, 0);
        next_minute_mark = 60.0;
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
            if (!displaced) {
                P[k_index]++; 
            }
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
                current_minute_reqs[k]++; // Считаем заявку для статистики за минуту

                bool placed = false;
                for (int i = 0; i < cfg.N; ++i) {
                    if (D[i] == -1) {
                        D[i] = k;
                        // Задание 4: Экспоненциальное распределение времени обслуживания
                        U[i] = t + get_valid_time([&](){ return gen_exponential(cfg.server_mus[i]); });
                        placed = true;
                        break;
                    }
                }

                if (!placed) {
                    check_task(k); 
                }

                // Генерируем интервал по заданному закону для Варианта 16
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
                        // Экспоненциальное обслуживание
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

            int lowest_s = -1;
            int server_idx = -1;
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
                // Экспоненциальное обслуживание
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

            // Задание 2: Сохраняем статистику каждую минуту (каждые 60 сек)
            if (t >= next_minute_mark) {
                for (int k = 0; k < cfg.Q; ++k) {
                    req_per_minute[k].push_back(current_minute_reqs[k]);
                    current_minute_reqs[k] = 0;
                }
                next_minute_mark += 60.0;
            }

            free_finished_servers();    
            handle_arrivals();          
            process_queue_relative();   
            process_queue_absolute();   
        }
    }
    void print_first_minute_stats() {
        cout << "\n========================================================" << endl;
        cout << "СТАТИСТИКА ПОСТУПЛЕНИЯ ЗАЯВОК ЗА ПЕРВУЮ МИНУТУ" << endl;
        cout << "========================================================" << endl;
        
        if (req_per_minute[0].empty()) {
            cout << "Статистика еще не собрана (моделирование не запущено)." << endl;
            return;
        }

        for (int k = 0; k < cfg.Q; ++k) {
            cout << "Приоритет " << (k + 1) << ": " << req_per_minute[k][0] << " заявок" << endl;
        }
        cout << "========================================================" << endl;
    }
    void print_stats() {
        string line = "+-----------+--------------+--------------+--------------+--------------+";

        cout << endl << "РЕЗУЛЬТАТЫ МОДЕЛИРОВАНИЯ СМО (Лабораторная 3)" << endl;
        cout << "Вариант: 16 | Режим: Смешанный (W = " << cfg.W << ")" << endl;
        cout << "Время мод.: " << cfg.t_mod << " сек (1 день) | Приборов: " << cfg.N << endl;
        cout << endl;

        cout << line << endl;
        cout << "| Приоритет |   Поступило  |    Отказов   |  Вер. отказа |  Вер. обслуж |" << endl;
        cout << line << endl;

        int total_B = 0;
        int total_P = 0;

        for (int k = 0; k < cfg.Q; ++k) {
            total_B += B[k];
            total_P += P[k];

            double p_ref = (B[k] > 0) ? (double)P[k] / B[k] : 0.0;
            double p_srv = 1.0 - p_ref;

            cout << "|     " << left << setw(6) << (k + 1)
            << "| " << right << setw(12) << B[k]
            << " | " << right << setw(12) << P[k]
            << " | " << right << setw(12) << fixed << setprecision(4) << p_ref
            << " | " << right << setw(12) << p_srv << " |" << endl;
        }

        cout << line << endl;

        double total_p_ref = (total_B > 0) ? (double)total_P / total_B : 0.0;
        cout << "|   ИТОГО   | " << right << setw(12) << total_B
        << " | " << right << setw(12) << total_P
        << " | " << right << setw(12) << fixed << setprecision(4) << total_p_ref
        << " | " << right << setw(12) << (1.0 - total_p_ref) << " |" << endl;
        cout << line << endl << endl;
    }
};

int main() {
    setlocale(LC_ALL, "Russian");
    Config myConf;

    SMO model(myConf);
    model.run();
    model.print_stats();            // Итоговая общая статистика
    model.print_first_minute_stats(); // Статистика за первую минуту

    return 0;
}