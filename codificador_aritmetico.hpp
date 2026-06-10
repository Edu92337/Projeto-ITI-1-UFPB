#include <iostream>
#include <vector>
#include<set>
#include<map>
#include<algorithm>
#include<bitset>
#include"estrutura_contexto.hpp"
using namespace std;



typedef struct Codificador_aritmetico Codificador_aritmetico;
struct Codificador_aritmetico{

    // aplica as operações de ampliação e deslocamento do codificador
    // retorna -> {limite inferior, limite superior}
    pair<double,double> encode_byte(uint8_t& atual,No* contexto, double& low, double& high){
        auto& frequencias = contexto->frequencias;
        double range = high - low;
        double p0 = low;
        uint32_t total = contexto->total;
        for(int i = 0;i<256;i++){
            if(frequencias[i]==0) continue;// nao influencia no intervalo
            if(i == atual){
                high = p0 + range*((double)frequencias[i])/total;
                low = p0;
                return {low,high};
            }
            p0 += range*((double)frequencias[i])/total;
        }
        return {low,high};
    }
    
    // decodificador  
    uint8_t decode(double& n){

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