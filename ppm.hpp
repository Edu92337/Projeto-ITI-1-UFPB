#include<iostream>
#include<array>
#include"codificador_aritmetico.hpp"
#include"estrutura_contexto.hpp"

using namespace std;

struct Ppm{
    array<uint32_t,256>alfabeto;
    Ppm(){
        for(int i = 0;i<256;i++)alfabeto[i] = i;
    }
};