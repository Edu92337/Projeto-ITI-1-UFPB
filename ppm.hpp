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

    double low,high;
    // Kmax e J vão ser passados como parâmetros da compilação
    int Kmax;
    int J;
    Ppm(int k, int tamanho){
        for(int i = 0;i<256;i++)alfabeto[i] = i;
        Kmax = k;
        J = tamanho;
        low = 0;
        high = 1;
    }
    void le_nova_janela(){
        // faz a leitura de novos J bytes

    }
    bool existe_contexto(No* contexto,uint8_t simbolo){
        return contexto->frequencias[simbolo] > 0;
    }
    No* busca_maior_contexto(vector<uint8_t>&janela){
        // Busca o maior contexto possivel para aquela janela de bytes e tenta codificar o simbolo
        No* maior_contexto_possivel = arvore.busca_contexto_byte(janela);
        return maior_contexto_possivel;
    }
    void atualiza_frequencia_contexto(No* contexto,uint8_t atual){
        // atualiza no contexto usado
        arvore.atualiza_frequencia(contexto,atual);
    }

    void insere_contexto(){
        //
    } 
    void processa_simbolo(uint8_t atual){
        No* contexto = arvore.busca_contexto_byte(janela_atual);
        // percorre, subindo, procurando onde codificar o simbolo
        // a raiz está inclusa
        while(contexto){
            if(existe_contexto(contexto,atual)){
                aritmetico.encode_byte(atual,contexto,low,high);
                arvore.atualiza_frequencia(contexto,atual);
                return ;
            }else{
                aritmetico.encode_byte(ESCAPE,contexto,low,high);
                contexto->frequencias[ESCAPE]++;
                contexto->total++;
            }
            contexto = contexto->pai;
        }
        // chegou em k = -1 -> usa distribuição uniforme
        
    }
};