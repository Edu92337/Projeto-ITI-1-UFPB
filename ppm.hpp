#include<iostream>
#include<array>
#include"codificador_aritmetico.hpp"
#include"estrutura_contexto.hpp"

using namespace std;

struct Ppm{
    uint8_t alfabeto[256];
    trie_contexto arvore;
    vector<uint8_t> janela_atual;
    Codificador_aritmetico aritmetico;
    // Kmax e J vão ser passados como parâmetros da compilação
    int Kmax;
    int J;
    Ppm(int k, int tamanho){
        for(int i = 0;i<256;i++)alfabeto[i] = i;
        Kmax = k;
        J = tamanho;
    }
    void le_nova_janela(){
        // faz a leitura de novos J bytes

    }
    No* busca_maior_contexto(trie_contexto &arvore,vector<uint8_t>&janela){
        // Busca o maior contexto possivel para aquela janela de bytes
    }
    void atualiza_frequencia_contexto(No* contexto,uint8_t byte){
        // atualiza no contexto usado
    }




    pair<double,double> codifica_byte(){

    }
};