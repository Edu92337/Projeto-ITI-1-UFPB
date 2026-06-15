#include <iostream>
#include <vector>
#include<set>
#include<map>
#include<algorithm>
#include<bitset>
#include"estrutura_contexto.hpp"
using namespace std;
const uint16_t ESCAPE = 256;


typedef struct Codificador_aritmetico Codificador_aritmetico;
struct Codificador_aritmetico{

    // aplica as operações de ampliação e deslocamento do codificador
    // retorna -> {limite inferior, limite superior}
    uint32_t contagem_excluidos(No* contexto,set<uint8_t>&excluidos){
        uint32_t t = 0;
        for(auto b : excluidos){
            t += contexto->frequencias[b];
        }
        return t;
    }
    pair<double,double> encode_byte(uint8_t atual, No* contexto,set<uint8_t>&excluidos, double& low, double& high) {
        auto& frequencias = contexto->frequencias;
        uint32_t total = contexto->total-contagem_excluidos(contexto,excluidos);
        double range = high - low ;
        double p0 = 0.0;

        for (int i = 0; i < 257; i++) {
            if(excluidos.count(i) != 0) continue;
            if (frequencias[i] == 0) continue;
            if (i == atual) {
                high = low + range * (p0 + frequencias[i]) / total;
                low  = low + range * p0 / total;
                return {low, high};
            }
            p0 += frequencias[i];  
        }
        return {low, high};
    }
    
    // decodificador  
    uint8_t decode(uint32_t& n){

    }



    // faz a transcrição para um numero após todos bytes serem codificados
    uint32_t valor_final(uint32_t low, uint32_t high){
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