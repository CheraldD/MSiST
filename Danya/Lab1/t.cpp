#include <iostream>
#include <vector>
#include <numeric>
#include <random>
#include <iomanip> // Нужен для красивой таблички

using namespace std;

// ==========================================
// Настройки модели
// ==========================================
struct Config {
    int N = 2;      // Количество приборов
    int L = 6;      // Максимальная длина очереди
    int Q = 3;      // Количество приоритетов

    double t_mod = 40;      // Время моделирования
    double dt = 0.05;          // Шаг моделирования


    // Прибор 0 -> 2.5 сек, Прибор 1 -> 2.7 сек
    vector<double> server_mus = {3.5, 5};

    // Начальные моменты для заявок
    vector<double> initial_Z = {0.5, 1.0, 1.5};
};

class SMO {
private:
    Config cfg;
    double t;

    // Переменные состояния
    vector<double> Z; // Моменты поступления
    vector<double> U; // Время освобождения приборов

    // Статистика
    vector<int> B;    // Поступило
    vector<int> P;    // Отказано
    vector<int> j;    // Очередь (текущая)

    // Генератор
    double gen_random_interval() {
        static random_device rd;
        static mt19937 gen(rd());

        static uniform_real_distribution<float> dist(1.5,3);
        return dist(gen);
    }

public:
    SMO(Config config) : cfg(config) {
        t = 0.0;
        Z = cfg.initial_Z;
        U.resize(cfg.N, 0.0);
        B.resize(cfg.Q, 0);
        P.resize(cfg.Q, 0);
        j.resize(cfg.Q, 0);
    }

    void check_task(int k_index) {
        int current_queue_size = accumulate(j.begin(), j.end(), 0);

        if (current_queue_size < cfg.L) {
            j[k_index]++;
        }
        else {
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

    bool try_process_queue(int server_idx) {
        // Пробегаем по приоритетам: от 0 (высший) до конца
        for (int k = 0; k < cfg.Q; ++k) {
            if (j[k] > 0) {
                j[k]--; // Извлекаем из очереди


                // Берем mu из вектора согласно индексу прибора (server_idx)
                double current_mu = cfg.server_mus[server_idx];

                U[server_idx] = t + current_mu; // Занимаем прибор
                return true;
            }
        }
        return false;
    }

    void run() {
        while (t <= cfg.t_mod) {
            t += cfg.dt;

            // 1. Генерация
            for (int k = 0; k < cfg.Q; ++k) {
                if (t >= Z[k]) {
                    B[k]++;
                    check_task(k);
                    Z[k] += gen_random_interval();
                }
            }

            // 2. Обработка
            for (int i = 0; i < cfg.N; ++i) {
                if (t >= U[i]) {
                    try_process_queue(i);
                }
            }
        }
    }


    void print_stats() {

        string line = "+-----------+--------------+--------------+--------------+--------------+";

        cout << endl << "РЕЗУЛЬТАТЫ МОДЕЛИРОВАНИЯ СМО" << endl;
        cout << "Время: " << cfg.t_mod << " | Приборов: " << cfg.N << " | Очередь макс: " << cfg.L << endl;
        cout << "Параметры обработки (mu): ";
        for(size_t i=0; i<cfg.server_mus.size(); ++i)
            cout << "Пр." << i+1 << "=" << cfg.server_mus[i] << " ";
        cout << endl << endl;

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

        // Итоговая строка
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
    model.print_stats();

    return 0;
}
