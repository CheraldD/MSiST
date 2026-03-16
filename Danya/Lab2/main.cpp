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
    int N = 2;      // Количество приборов
    int L = 6;      // Максимальная длина очереди
    int Q = 3;      // Количество приоритетов

    int W = 3;      // Порог включения абсолютного приоритета (Вариант)

    double t_mod = 40;         // Время моделирования
    double dt = 0.05;          // Шаг моделирования

    // Время обслуживания для приборов (Прибор 0 -> 3.5, Прибор 1 -> 5.0)
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

    // Генератор интервалов
    double gen_random_interval() {
        static random_device rd;
        static mt19937 gen(rd());
        static uniform_real_distribution<float> dist(1.5, 3.0);
        return dist(gen);
    }

public:
    SMO(Config config) : cfg(config) {
        t = 0.0;
        Z = cfg.initial_Z;
        U.resize(cfg.N, 0.0);
        D.resize(cfg.N, -1); // Изначально все приборы свободны
        B.resize(cfg.Q, 0);
        P.resize(cfg.Q, 0);
        j.resize(cfg.Q, 0);
    }

    // Постановка заявки в очередь (с возможным вытеснением более низкого приоритета)
    void check_task(int k_index) {
        int current_queue_size = accumulate(j.begin(), j.end(), 0);

        if (current_queue_size < cfg.L) {
            j[k_index]++;
        } else {
            bool displaced = false;
            // Ищем заявку с более низким приоритетом (индекс больше) для удаления
            for (int low_prio = cfg.Q - 1; low_prio > k_index; --low_prio) {
                if (j[low_prio] > 0) {
                    j[low_prio]--;     // Удаляем из очереди
                    P[low_prio]++;     // Считаем как отказ
                    j[k_index]++;      // Ставим новую
                    displaced = true;
                    break;
                }
            }
            // Если вытеснить некого (вся очередь заполнена такими же или более высокими приоритетами)
            if (!displaced) {
                P[k_index]++; // Отказ пришедшей заявке
            }
        }
    }

    // Освобождение завершивших работу приборов
    void free_finished_servers() {
        for (int i = 0; i < cfg.N; ++i) {
            if (D[i] != -1 && t >= U[i]) {
                D[i] = -1; // Прибор свободен
            }
        }
    }

    // Обработка поступления новых заявок
    void handle_arrivals() {
        for (int k = 0; k < cfg.Q; ++k) {
            if (t >= Z[k]) {
                B[k]++;

                bool placed = false;
                // Сначала пытаемся занять свободный прибор
                for (int i = 0; i < cfg.N; ++i) {
                    if (D[i] == -1) {
                        D[i] = k;
                        U[i] = t + cfg.server_mus[i];
                        placed = true;
                        break;
                    }
                }

                // Если все приборы заняты, отправляем в очередь
                if (!placed) {
                    check_task(k); 
                }

                // Генерируем время прихода следующей заявки этого приоритета
                Z[k] += gen_random_interval();
            }
        }
    }

    // Логика относительного приоритета: загрузка заявок из очереди на свободные приборы
    void process_queue_relative() {
        for (int i = 0; i < cfg.N; ++i) {
            if (D[i] == -1) { // Если прибор свободен
                for (int k = 0; k < cfg.Q; ++k) {
                    if (j[k] > 0) {
                        j[k]--; // Извлекаем из очереди
                        D[i] = k; // Занимаем прибор
                        U[i] = t + cfg.server_mus[i];
                        break;
                    }
                }
            }
        }
    }

    // Логика абсолютного приоритета: автоматическое переключение при W заявках высшего приоритета
    void process_queue_absolute() {
        // Условие включения абсолютного приоритета: заявок 1-го приоритета (индекс 0) >= W
        while (j[0] >= cfg.W) {
            // Ищем наивысший приоритет в очереди
            int highest_q = -1;
            for(int q = 0; q < cfg.Q; ++q) {
                if(j[q] > 0) { highest_q = q; break; }
            }

            // Ищем прибор, обрабатывающий заявку с самым низким приоритетом
            int lowest_s = -1;
            int server_idx = -1;
            for(int i = 0; i < cfg.N; ++i) {
                if(D[i] > lowest_s) {
                    lowest_s = D[i];
                    server_idx = i;
                }
            }

            // Если приоритет в очереди ВЫШЕ (индекс меньше), чем худший приоритет на приборах -> Прерываем!
            if (highest_q != -1 && lowest_s != -1 && highest_q < lowest_s) {
                int interrupted_prio = D[server_idx]; // Запоминаем кого прервали
                
                D[server_idx] = highest_q;            // Загружаем новую
                j[highest_q]--;                       // Забираем из очереди
                U[server_idx] = t + cfg.server_mus[server_idx]; // Устанавливаем время
                
                check_task(interrupted_prio);         // Возвращаем прерванную заявку в очередь
            } else {
                break; // Вытеснять некого (на приборах тоже высокие приоритеты)
            }
        }
    }

    // Основной цикл симуляции
    void run() {
        while (t <= cfg.t_mod) {
            t += cfg.dt;

            free_finished_servers();    // 1. Освобождаем приборы по времени
            handle_arrivals();          // 2. Обрабатываем приход новых заявок (и возможно пушим их в очередь)
            process_queue_relative();   // 3. Заполняем освободившиеся приборы
            process_queue_absolute();   // 4. Проверяем условие вытеснения (Смешанный приоритет)
        }
    }

    void print_stats() {
        string line = "+-----------+--------------+--------------+--------------+--------------+";

        cout << endl << "РЕЗУЛЬТАТЫ МОДЕЛИРОВАНИЯ СМО (Лабораторная 2)" << endl;
        cout << "Режим: Смешанный приоритет (порог W = " << cfg.W << ")" << endl;
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