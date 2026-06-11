#include<iostream>
#include<array>
#include"codificador_aritmetico.hpp"
#include"estrutura_contexto.hpp"

using namespace std;

struct Ppm{
    uint8_t alfabeto[256];
    trie_contexto arvore;
    Codificador_aritmetico aritmetico;
    Ppm(){
        for(int i = 0;i<256;i++)alfabeto[i] = i;
    }
};