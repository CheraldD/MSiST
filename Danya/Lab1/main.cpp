#include <iostream>
#include <vector>
#include <numeric>
#include <random>
#include <iomanip>

using namespace std;

// ==========================================
// Настройки модели
// ==========================================
struct Config {
    int N = 1;      // Количество приборов (маршрутов)
    int L = 6;      // Максимальная длина очереди
    int Q = 2;      // Количество источников (приоритетов)

    double t_mod = 25;     // Время моделирования
    double dt = 0.1;          // Шаг моделирования

    double mu = 1.8;          // Время обработки заявки (const)

    // Начальные Z для каждого приоритета (размер должен совпадать с Q)
    vector<double> initial_Z = {0.15, 0.625};
};

class SMO {
private:
    Config cfg;
    double t; // Текущее время (t_real)

    // Переменные состояния
    vector<double> Z; // Моменты поступления заявок (размер Q)
    vector<double> U; // Время освобождения приборов (размер N)

    // Статистика
    vector<int> B;    // Всего заявок (поступивших)
    vector<int> P;    // Количество отказов
    vector<int> j;    // Заявок в очереди по приоритетам

    // Генератор случайных чисел
    double gen_random_interval() {
        static random_device rd;
        static mt19937 gen(rd());
        static uniform_real_distribution<float> dist(1.2, 1.5);
        return dist(gen);
    }

public:
    SMO(Config config) : cfg(config) {
        t = 0.0;

        // Инициализация векторов под размер задачи
        Z = cfg.initial_Z;
        U.resize(cfg.N, 0.0); // N приборов, время 0
        B.resize(cfg.Q, 0);
        P.resize(cfg.Q, 0);
        j.resize(cfg.Q, 0);
    }

    // Логика постановки в очередь (check_task)
    void check_task(int k_index) {
        int current_queue_size = accumulate(j.begin(), j.end(), 0);

        // Если есть место в очереди
        if (current_queue_size < cfg.L) {
            j[k_index]++;
        }
        // Если очередь полна, пытаемся вытеснить менее важную заявку
        else {
            bool displaced = false;
            // Ищем заявку с более низким приоритетом (индекс больше), чем k_index
            // Идем с конца (самый низкий приоритет)
            for (int low_prio = cfg.Q - 1; low_prio > k_index; --low_prio) {
                if (j[low_prio] > 0) {
                    j[low_prio]--;      // Выкидываем низкоприоритетную
                    P[low_prio]++;      // Записываем ей отказ
                    j[k_index]++;       // Ставим новую
                    displaced = true;
                    break;
                }
            }

            if (!displaced) {
                // Если вытеснить некого (все важнее или такие же), отказываем текущей
                P[k_index]++;
            }
        }
    }

    // Логика извлечения из очереди (trow_task) и загрузки прибора
    // Возвращает true, если прибор взял задачу
    bool try_process_queue(int server_idx) {
        // Проверяем очередь, начиная с высшего приоритета (0)
        for (int k = 0; k < cfg.Q; ++k) {
            if (j[k] > 0) {
                j[k]--; // Убираем из очереди
                U[server_idx] = t + cfg.mu; // Занимаем прибор
                return true;
            }
        }
        return false;
    }

    void run() {
        while (t <= cfg.t_mod) {
            t += cfg.dt;

            // 1. Проверка поступления новых заявок (цикл по всем источникам)
            for (int k = 0; k < cfg.Q; ++k) {
                if (t >= Z[k]) {
                    B[k]++;             // Увеличиваем счетчик поступивших
                    check_task(k);      // Пробуем поставить в очередь
                    Z[k] += gen_random_interval(); // Планируем следующую
                }
            }

            // 2. Проверка освобождения приборов (цикл по всем приборам)
            for (int i = 0; i < cfg.N; ++i) {
                // Если прибор свободен (время освобождения <= текущему)
                // ИЛИ только что освободился
                if (t >= U[i]) {
                    // Пытаемся взять задачу из очереди
                    try_process_queue(i);
                }
            }
        }
    }

    void print_stats() {
        cout << fixed << setprecision(4);
        cout << "=== Результаты моделирования ===" << endl;
        cout << "Приборов (N): " << cfg.N << ", Очередь (L): " << cfg.L << endl;

        for (int k = 0; k < cfg.Q; ++k) {
            double p_refusal = (B[k] > 0) ? (double)P[k] / B[k] : 0.0;

            cout << "----------------------------" << endl;
            cout << "Приоритет " << (k + 1) << ":" << endl;
            cout << "  Поступило (B[" << k+1 << "]): " << B[k] << endl;
            cout << "  Отказов   (P[" << k+1 << "]): " << P[k] << endl;
            cout << "  Вероятность отказа:  " << p_refusal << endl;
            cout << "  Вероятность обслуж.: " << (1.0 - p_refusal) << endl;
        }
        cout << "----------------------------" << endl;
    }
};

int main() {
    setlocale(LC_ALL, "Russian");

    // Настройка параметров перед запуском
    Config myConf;

    SMO model(myConf);
    model.run();
    model.print_stats();

    return 0;
}
