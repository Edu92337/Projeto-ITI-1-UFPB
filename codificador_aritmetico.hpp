#include <iostream>
#include <vector>
#include<set>
#include<map>
#include<algorithm>
#include<bitset>
using namespace std;

class Codificador_aritmetico{
    public:
        //gera o vetor representante das probabilidades
        // O(N) [* acho que pode melhorar ]
        vector<pair<uint8_t,double>> gera_mapa(const vector<uint8_t>& dados){
            vector<pair<uint8_t,double>> mapa;
            set<uint8_t> vistos;
            double n = dados.size();

            for(uint8_t b : dados){
                if(vistos.count(b)) continue;
                vistos.insert(b);
                double q = count(dados.begin(), dados.end(), b);
                mapa.push_back({b, q/n});
            }

            return mapa;
        }
        // aplica as operações de ampliação e deslocamento do codificador
        // retorna -> {limite inferior, limite superior}
        pair<double,double> encode_byte(uint8_t& atual,vector<pair<uint8_t,double>>&mapa, double& low, double& high){
            double range = high - low;
            double p0 = low;
            for(auto [s,p] : mapa){
                if(s == atual){
                    high = p0 + range * p;
                    low  = p0;
                    return {low, high};
                }else{
                    p0 += range * p;
                }
            }
        }
        // faz a transcrição para um numero após todos bytes serem codificados
        double valor_final(double low, double high){
            string low_s = to_string(low);
            string high_s = to_string(high);
            string final = "";
            for(int i = 0;i<low_s.size();i++){
                if(low_s[i] == high_s[i])final += low_s[i];
                else{
                    final += high_s[i];
                    break;
                }
            }
            return stod(final);
        }
        
};