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
    pair<uint32_t,uint32_t> encode_byte(uint8_t atual, No* contexto, uint32_t& low, uint32_t& high) {
        auto& frequencias = contexto->frequencias;
        uint32_t total = contexto->total;
        uint32_t range = high - low + 1;
        uint32_t p0 = 0;

        for (int i = 0; i < 256; i++) {
            if (frequencias[i] == 0) continue;
            if (i == atual) {
                high = low + range * (p0 + frequencias[i]) / total - 1;
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