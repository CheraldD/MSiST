#include <iostream>
#include <random>
#include <vector>
#include <numeric>
using namespace std;
int L = 6; //Максимальный размер очереди
vector<float> Z{0.15,0.625};
float t_mod = 86400; // время моделирования
float delta_time = 1.5; //dt
float t_real=0; //текущее время
vector<float> U(3,0);//Время заполнения заявок приборами
vector<int> B(2,0);//Счетчик количества заявок Q-го приоритета
vector<int> P(2,0);//Счетчик количества отказов в обслуживании заявок Q-го приоритета
vector<int> j(2,0);//Счетчик количества заявок Q-го приоритета в очереди
float mu = 0.3;
float gen_z(){
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<float> dist (1,10);
    return dist(gen);
}
void check_task(int k){
    if(accumulate(j.begin(),j.end(),0)<6){
        j[k-1]+=1;
    }
    else{
        if(k==2){
            P[1]+=1;
            return;
        }
        else{
            if(j[1]>0){
                j[1]-=1;
                P[1]+=1;
                j[k-1]+=1;
                return;
            }
            else{
                P[0]+=1;
                return;

            }
        }
    }
    return;
}
void trow_task(){
    if(j[0]>0){
        j[0]-=1;
    }
    else{
        if(j[1]>0){
            j[1]-=1;
        }
    }
    return;
}
void new_task(float t){
    if (t>=Z[0]){
        check_task(1);
        Z[0]+=gen_z(); //генерация числа Z; заменить позднее
        B[0]+=1;
        return;
    }
    if (t>=Z[1]){
        check_task(2);
        Z[1]+=gen_z();//генерация числа Z; заменить позднее
        B[1]+=1;
    }
    else{
        if(accumulate(j.begin(),j.end(),0)==0){
            return;
        }
        else{
            if (t>=U[0]){
                trow_task();
                U[0]=t+mu;
                if(accumulate(j.begin(),j.end(),0)==0){
                    return;
                }
            }
            if (t>=U[1]){
                trow_task();
                U[1]=t+mu;
                if(accumulate(j.begin(),j.end(),0)==0){
                    return;
                }
            }
            if (t>=U[2]){
                trow_task();
                U[2]=t+mu;
                if(accumulate(j.begin(),j.end(),0)==0){
                    return;
                }
            }
            if (t>=U[3]){
                trow_task();
                U[3]=t+mu;
                if(accumulate(j.begin(),j.end(),0)==0){
                    return;
                }
            }
            if (t>=U[4]){
                trow_task();
                U[4]=t+mu;
                if(accumulate(j.begin(),j.end(),0)==0){
                    return;
                }
            }
            if (t>=U[5]){
                trow_task();
                U[5]=t+mu;
                if(accumulate(j.begin(),j.end(),0)==0){
                    return;
                }
            }
        }

    }
    return;
}
int main(){
    while(t_real<=t_mod){
        t_real+=delta_time;
        new_task(t_real);
    }
    for (int i = 0;i<2;i++){
        cout<<"P["<<i+1<<"]="<<P[i]<<"; ";
    }
    cout<<endl;
    for (int i = 0;i<2;i++){
        cout<<"B["<<i+1<<"]="<<B[i]<<"; ";
    }
    cout<<endl;
    cout<<"Вероятность отказа в обслуживании пакета с 1 приоритетом: "<<static_cast<double>(P[0])/static_cast<double>(B[0])<<endl;
    cout<<"Вероятность отказа в обслуживании пакета с 2 приоритетом: "<<static_cast<double>(P[1])/static_cast<double>(B[1])<<endl;
    cout<<"Вероятность корректного приема пакета с 1 приоритетом: "<<1-(static_cast<double>(P[0])/static_cast<double>(B[0]))<<endl;
    cout<<"Вероятность корректного приема пакета с 2 приоритетом: "<<1-(static_cast<double>(P[1])/static_cast<double>(B[1]))<<endl;
    return 0;
}
