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

    // Время моделирования: 1 сутки (в секундах), чтобы проверить работу генераторов
    double t_mod = 86400;   
    double dt = 0.1;        // Шаг моделирования (dt)

    double mu_mean = 15.0;  // Математическое ожидание времени обработки заявки

    // Начальные Z (моменты поступления). 
    // Нулевой индекс не используется для удобства
    vector<double> initial_Z = {0.0, 5.0, 0.5}; 
};

class SMO {
private:
    Config cfg;
    double t; // Текущее модельное время

    vector<double> Z; // Моменты поступления заявок Z1..ZQ
    vector<double> U; // Время окончания обработки заявок приборами U1..UN
    vector<int> D;    // Приоритет заявки в приборе D1..DN (0 = свободен)

    vector<int> B;    // Количество поступивших заявок B1..BQ
    vector<int> P;    // Количество отказов P1..PQ
    vector<int> j;    // Заявок в очереди по приоритетам j1..jQ

    // --- Переменные для Пункта 2 (сбор заявок за минуту) ---
    int last_recorded_minute = 0;
    int current_min_P1 = 0;
    int current_min_P2 = 0;
public:
    vector<int> reqs_per_minute_P1;
    vector<int> reqs_per_minute_P2;

private:
    // --- ПУНКТ 4: Генератор экспоненциального распределения (Время обработки) ---
    double generate_exponential(double mean) {
        static random_device rd;
        static mt19937 gen(rd());
        exponential_distribution<double> dist(1.0 / mean); // lambda = 1/M
        double val;
        do { val = dist(gen); } while (val <= 0); // Отбрасываем <= 0
        return val;
    }

    // --- ПУНКТ 1: Генератор равномерного закона (Формула из методички) ---
    double generate_uniform(double M, double D) {
        static random_device rd;
        static mt19937 gen(rd());
        double c = M - sqrt(3 * D);
        double b = M + sqrt(3 * D);
        uniform_real_distribution<double> dist(c, b);
        double val;
        do { val = dist(gen); } while (val <= 0); // Отбрасываем <= 0
        return val;
    }

    // --- ПУНКТ 1: Генератор кубического закона (Формула из методички) ---
    double generate_cubic(double M, double D) {
        static random_device rd;
        static mt19937 gen(rd());
        uniform_real_distribution<double> dist(0.0, 1.0);
        double val;
        do {
            double x = dist(gen);
            // Формула: y = sqrt(37.5 * D(y)) * (x^(1/4) - 0.8) + M(y)
            val = sqrt(37.5 * D) * (pow(x, 0.25) - 0.8) + M;
        } while (val <= 0); // Отбрасываем <= 0
        return val;
    }

    // Определение интервала по варианту 20
    double gen_random_interval(int prio_index, double current_time) {
        // Определяем час текущих суток (0..24)
        double t_hour = fmod(current_time, 86400.0) / 3600.0;
        bool is_day = (t_hour >= 8.0 && t_hour < 20.0);

        if (prio_index == 1) {
            // ВАРИАНТ 20, Приоритет 1 (Равномерный)
            double M = is_day ? 4.807692 : 2.0;
            double D = is_day ? 24.02922 : 4.0;
            return generate_uniform(M, D);
        } else {
            // ВАРИАНТ 20, Приоритет 2 (Кубический)
            double M = is_day ? 5.208333 : 0.25;
            double D = is_day ? 27.12674 : 0.0625;
            return generate_cubic(M, D);
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
            if (!displaced) {
                P[k]++; 
            }
        }
    }

    void CaptureServer(int k) {
        for (int i = 0; i < cfg.N; ++i) {
            if (D[i] == 0) { 
                D[i] = k;
                U[i] = t + generate_exponential(cfg.mu_mean); // Экспоненциальное время
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

            // --- ПУНКТ 2: Сбор статистики за минуту ---
            int current_mod_minute = static_cast<int>(t / 60.0);
            if (current_mod_minute > last_recorded_minute) {
                reqs_per_minute_P1.push_back(current_min_P1);
                reqs_per_minute_P2.push_back(current_min_P2);
                current_min_P1 = 0;
                current_min_P2 = 0;
                last_recorded_minute = current_mod_minute;
            }

            // --- БЛОК 1: Поступление заявок ---
            for (int k = 1; k <= cfg.Q; ++k) {
                if (t >= Z[k]) {
                    B[k]++;           
                    
                    // Увеличиваем счетчик заявок в текущей минуте
                    if (k == 1) current_min_P1++;
                    else if (k == 2) current_min_P2++;

                    CaptureServer(k); 
                    Z[k] += gen_random_interval(k, t); // Генерация нового Z
                }
            }

            // --- БЛОК 2: Автоматическое переключение режимов ---
            CheckMixedPrioritySwitch(); 

            // --- БЛОК 3: Окончание обработки ---
            for (int i = 0; i < cfg.N; ++i) {
                if (D[i] != 0 && t >= U[i]) { 
                    D[i] = 0;                 
                    ExtractFromQueue(i);      
                }
            }
        }
    }

    void print_stats() {
        cout << "\n=========================================================" << endl;
        cout << "   РЕЗУЛЬТАТЫ ИМИТАЦИОННОГО МОДЕЛИРОВАНИЯ   " << endl;
        cout << "=========================================================" << endl;
        cout << "Время моделирования: " << cfg.t_mod / 3600.0 << " ч." << endl;
        //cout << "Всего учтено интервалов (по 1 мин): " << reqs_per_minute_P1.size() << endl;

        for (int k = 1; k <= cfg.Q; ++k) {
            double p_refusal = (B[k] > 0) ? (double)P[k] / B[k] : 0.0;
            double p_accept = 1.0 - p_refusal;
            
            string p_name = (k == 1) ? "1 (Равномерный закон)" : "2  (Кубический закон)";

            cout << "\n--- Приоритет: " << p_name << " ---" << endl;
            cout << "  Всего заявок поступило: " << B[k] << endl;
            cout << "  Заявок отклонено:       " << P[k] << endl;
            cout << "  Вероятность отказа:     " << p_refusal * 100 << "%" << endl;
            cout << "  Вероятность приема:     " << p_accept * 100 << "%" << endl;
        }

        cout << "\n=========================================================" << endl;
        cout << "   Статистика за 1 минуту" << endl;
        
        if (!reqs_per_minute_P1.empty()) {
            // Функция-помощник для вывода вероятности в конкретной точке (на основе накопленного)
            auto print_min_info = [&](int min_idx) {
                int n1 = reqs_per_minute_P1[min_idx];
                int n2 = reqs_per_minute_P2[min_idx];
                //cout << "  Минута #" << min_idx + 1 << ":" << endl;
                cout << "    Поток P1: " << n1 << " заявок | Поток P2: " << n2 << " заявок" << endl;
            };

            print_min_info(0); // Первая минута
            //print_min_info(reqs_per_minute_P1.size() / 2); // Середина моделирования
        }
        
    }
};

int main() {
    setlocale(LC_ALL, "Russian");
    Config myConf;
    SMO model(myConf);
    model.run();
    model.print_stats();
    return 0;
}