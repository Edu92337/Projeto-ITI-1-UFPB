#pragma once
#include<iostream>
#include<array>
#include<set>
#include<queue>
#include"codificador_aritmetico.hpp"
#include"estrutura_contexto.hpp"

using namespace std;

struct Ppm{
    uint8_t alfabeto[256];
    trie_contexto arvore;
    deque<uint8_t> janela_atual;
    Codificador_aritmetico aritmetico;
    set<uint8_t>excluidos; // bytes ja vistos -> precisa ser limpo a cada novo byte 
    deque<uint8_t> janela_j; // janela para acompanhar as métricas
    double low,high;
    // Kmax e J vão ser passados como parâmetros da compilação
    int Kmax;
    int J;
    Ppm(int k, int tamanho,int adaptacao){
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
    No* busca_maior_contexto(deque<uint8_t>&janela){
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
    void insere_em_excluidos(No* contexto){
        for(int i = 0;i<256;i++){
            if(contexto->frequencias[i]>0)excluidos.insert(i);
        }
    }
    void realiza_exclusao_contexto(No* contexto){
        for(int i = 0;i<256;i++){
            if(excluidos.count(i) != 0)contexto->frequencias[i] = 0;
        }
    }
    
    void atualiza_contexto(uint8_t atual){
        // limita o comprimento do contexto atual para Kmax
        // modifica aqui para não modificar a estrutura da trie
        janela_atual.push_back(atual);
        if (janela_atual.size() > Kmax)
            janela_atual.pop_front();
        arvore.insere_byte_em_contexto(janela_atual);
    }

    void processa_simbolo(uint8_t atual){
        bool codificado = false;
        No* contexto = arvore.busca_contexto_byte(janela_atual);
        //atualiza a janela para as métricas
        if(janela_j.size()>=J)janela_j.pop_front();
        janela_j.push_back(atual);

        
        // percorre, subindo, procurando onde codificar o simbolo
        // a raiz está inclusa
        while(contexto){
            if(existe_contexto(contexto,atual)){
                aritmetico.encode_byte(atual,contexto,excluidos,low,high);
                arvore.atualiza_frequencia(contexto,atual);
                codificado = true;
                break ;
            }else{
                // o set<uint8_t>excluidos precisa ser modificado antes de codificado
                // caso tenha exclusão
                aritmetico.encode_byte(ESCAPE,contexto,excluidos,low,high);
                insere_em_excluidos(contexto);
                contexto->frequencias[ESCAPE]++;
                contexto->total++;
            }
            contexto = contexto->pai;
        }
        

        if(codificado == false){
            // chegou em k = -1 -> usa distribuição uniforme
            // processa em k =-1(FALTA)
        }

        atualiza_contexto(atual);
        // limpar excluidos para processar um byte novo
        excluidos.clear();
    }
};