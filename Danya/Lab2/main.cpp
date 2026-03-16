#include <iostream>
#include <vector>
#include <random>
#include <iomanip>

using namespace std;

struct Config {
    int N = 1;      // Количество приборов
    int L = 6;      // Максимальная длина очереди
    int Q = 2;      // Количество приоритетов (1 - высший, Q - низший)
    
    
    // Переключение на абсолютный приоритет при W заявок первого приоритета в очереди
    int W = 2;      

    double t_mod = 25;      // Время моделирования (Тмод)
    double dt = 0.1;          // Шаг моделирования (dt)

    double mu = 15;          // Время обработки заявки

    // Начальные Z (моменты поступления). 
    // Нулевой индекс не используется для удобства (приоритеты 1 и 2)
    vector<double> initial_Z = {0.0, 5, 0.5}; 
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

    double gen_random_interval(int prio_index) {
    static random_device rd;
    static mt19937 gen(rd());

    if (prio_index == 1) {
        // ВЫСШИЙ: приходит реже, чтобы дать очереди заполниться низшим приоритетом
        static uniform_real_distribution<double> dist_high(2.5, 3.5);
        return dist_high(gen);
    } else {
        // НИЗШИЙ: очень частый поток, чтобы быстро забить очередь L=6
        static uniform_real_distribution<double> dist_low(0.6, 1.0);
        return dist_low(gen);
    }
}

    // Алгоритм постановки в очередь (с учетом переполнения)
    void put_in_queue(int k) {
        int current_queue_size = 0;
        for (int i = 1; i <= cfg.Q; ++i) current_queue_size += j[i];

        if (current_queue_size < cfg.L) {
            j[k]++; // Очередь не полна, просто ставим
        } else {
            bool displaced = false;
            // Ищем заявку более низкого приоритета (от низшего Q до k+1)
            for (int low = cfg.Q; low > k; --low) {
                if (j[low] > 0) {
                    j[low]--;      // Удаляем из очереди
                    P[low]++;      // Отказ удаленной заявке (Причина 2 из методички)
                    j[k]++;        // Ставим текущую
                    displaced = true;
                    break;
                }
            }
            if (!displaced) {
                P[k]++; // Очередь полна равными или высшими приоритетами (Причина 1 или 3)
            }
        }
    }

    void CaptureServer(int k) {
        // Проверяем, есть ли свободные приборы
        for (int i = 0; i < cfg.N; ++i) {
            if (D[i] == 0) { // Прибор свободен
                D[i] = k;
                U[i] = t + cfg.mu;
                return;
            }
        }

        // Если все заняты, смотрим режим:
        // Если j[1] >= W, работает АБСОЛЮТНЫЙ приоритет (вытеснение)
        // Если j[1] < W, работает ОТНОСИТЕЛЬНЫЙ приоритет (вытеснения нет)
        if (j[1] >= cfg.W) { 
            // Ищем прибор, обрабатывающий самый низкий приоритет (от Q до k+1)
            for (int lowest = cfg.Q; lowest > k; --lowest) {
                for (int i = 0; i < cfg.N; ++i) {
                    if (D[i] == lowest) { // Нашли низший
                        int preempted = D[i];
                        D[i] = k;             // Вытесняем!
                        U[i] = t + cfg.mu;
                        put_in_queue(preempted); // Вытесненная заявка идет обратно в очередь
                        return;
                    }
                }
            }
        }

        // Если вытеснение не произошло (или мы в относительном режиме), ставим в очередь
        put_in_queue(k);
    }

    void ExtractFromQueue(int server_index) {
        // Проверка от высшего приоритета (1) до низшего (Q)
        for (int k = 1; k <= cfg.Q; ++k) {
            if (j[k] > 0) {
                j[k]--;
                D[server_index] = k;
                U[server_index] = t + cfg.mu;
                return;
            }
        }
    }

    void CheckMixedPrioritySwitch() {
        // Если накопилось W заявок 1-го приоритета, включается абсолютный режим
        if (j[1] >= cfg.W) {
            for (int i = 0; i < cfg.N; ++i) {
                if (D[i] > 1) { // Если прибор занят приоритетом ниже 1-го
                    if (j[1] > 0) { // И в очереди еще есть 1-й приоритет
                        int preempted = D[i];
                        
                        // Заявка из очереди насильно захватывает прибор
                        j[1]--; 
                        D[i] = 1; 
                        U[i] = t + cfg.mu;
                        
                        // Прерванная заявка выкидывается в очередь
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
        D.resize(cfg.N, 0);   // Все приборы изначально свободны (D = 0)
        
        B.resize(cfg.Q + 1, 0);
        P.resize(cfg.Q + 1, 0);
        j.resize(cfg.Q + 1, 0);
    }

    void run() {
        // Общий алгоритм 
        while (t <= cfg.t_mod) {
            t += cfg.dt; // Увеличение модельного времени t=t+dt

            // --- БЛОК 1: Поступление заявок ---
            for (int k = 1; k <= cfg.Q; ++k) {
                if (t >= Z[k]) {
                    B[k]++;           // Увеличиваем счетчик поступивших (B = B + 1)
                    CaptureServer(k); // Пытаемся занять прибор
                    Z[k] += gen_random_interval(k); // Генерация нового времени Z
                }
            }

            // --- БЛОК 2: Автоматическое переключение режимов ---
            // Если из-за поступления новых заявок j[1] достигло W
            CheckMixedPrioritySwitch(); 

            // --- БЛОК 3: Окончание обработки (Нижняя половина Рис. 2.18) ---
            for (int i = 0; i < cfg.N; ++i) {
                if (D[i] != 0 && t >= U[i]) { // Если выполнено t >= U
                    D[i] = 0;                 // Прибор освобождается (D = 0)
                    ExtractFromQueue(i);      // Берем заявку из очереди
                }
            }
        }
    }

    void print_stats() {
        cout << fixed << setprecision(4);
        cout << "=== Результаты моделирования (Смешанный приоритет) ===" << endl;
        cout << "Приборов (N): " << cfg.N << ", Макс. очередь (L): " << cfg.L << endl;
        cout << "Порог переключения (W): " << cfg.W << " (Вариант " << (cfg.W == 2 ? "20" : "?") << ")" << endl;

        for (int k = 1; k <= cfg.Q; ++k) {
            double p_refusal = (B[k] > 0) ? (double)P[k] / B[k] : 0.0;

            cout << "----------------------------" << endl;
            cout << "Приоритет " << k << (k == 1 ? " (Высший):" : " (Низший):") << endl;
            cout << "  Поступило (B" << k << "): " << B[k] << endl;
            cout << "  Отказов   (P" << k << "): " << P[k] << endl;
            cout << "  Вероятность отказа:  " << p_refusal << endl;
            cout << "  Вероятность приема:  " << 1-p_refusal << endl;
        }
        cout << "----------------------------" << endl;
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