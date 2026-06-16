#pragma once
#include<iostream>
#include<map>
#include<algorithm>
#include<vector>
#include<queue>
using namespace std;
typedef struct No No;

const uint32_t ESCAPE = 256;
const int TEMPOV = 1000; // tempo de vida arbitrário
// cada No é um contexto 
// a ideia seria percorrer até a folha e codificar na subida, para dar prioridade ao maior contexto
// Kmax <= 10 => O(Kmax)<= O(10) [acho que é ok]
struct No{
    uint8_t byte;
    No* filhos[257];
    vector<uint32_t>frequencias;// armazena as frequencias de cada byte no contexto
    No* pai; 
    uint32_t total; //soma de todas as frequências
    int tempo_de_vida; // vai ser usado para os mecanismos de poda
    No(int tempo = TEMPOV){
        fill(filhos,filhos+257,nullptr);
        frequencias.assign(257,0);
        pai = nullptr;
        total = 0;
        byte = 0;
        tempo_de_vida = tempo;
    }
};

// Vai receber uma janela de contexto contendo J bytes vistos pelo PPM (vector<uint8_t>&bytes)
struct trie_contexto{
    No* raiz;
    trie_contexto(){
        raiz = new No();
    }


    // a partir do no Raiz vai percorrendo e fazendo a ligação do byte com todos os outros Nós
    void insere_byte_em_contexto(const deque<uint8_t>&bytes){
        No* atual = raiz;
        // inverte para que ramos com contextos iguais compartilhem Nós
        for(auto it = bytes.rbegin(); it != bytes.rend(); it++){
            uint32_t b = *it;
            if(!atual->filhos[b]) {
                atual->filhos[b] = new No;
                atual->filhos[b]->byte = b;
                atual->filhos[b]->pai = atual; // faz a ligação da volta para a busca de Kpossivel -> K = -1(raiz)
            }
            atual = atual->filhos[b];
        }
    }
    // A partir da raiz vai percorrendo os nos que estão conectados/ contextos conectados 
    //até encontrar o maior contexto possível 
    No* busca_contexto_byte(const deque<uint8_t>&contexto){
        No* atual = raiz;
        for(auto it = contexto.rbegin();it!=contexto.rend();it++){
            uint32_t b = *it;
            if(atual->filhos[b])atual = atual->filhos[b];
            else return atual;
        }
        return atual;
    }
    // faz a propagação para todos os contextos menores até a raiz
    void atualiza_frequencia(No* contexto, uint8_t simbolo){
        contexto->frequencias[simbolo]++;
        contexto->total ++;
        if(!contexto->pai) return ;
        atualiza_frequencia(contexto->pai,simbolo);
        
    }

};