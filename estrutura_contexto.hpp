#pragma once
#include<iostream>
#include<map>
#include<unordered_map>
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
    unordered_map<uint8_t, No*> filhos;
    vector<uint32_t>frequencias;// armazena as frequencias de cada byte no contexto
    No* pai; 
    uint32_t total; //soma de todas as frequências
    int tempo_de_vida; // vai ser usado para os mecanismos de poda
    No(int tempo = TEMPOV){
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
    ~trie_contexto(){
        libera(raiz);
    }

    void libera(No* no){
        if(!no) return;
        for(auto& [_, filho] : no->filhos)
            libera(filho);

        delete no;
    }


    // a partir do no Raiz vai percorrendo e fazendo a ligação do byte com todos os outros Nós
    void insere_byte_em_contexto(const deque<uint8_t>&bytes){
        No* atual = raiz;
        // inverte para que ramos com contextos iguais compartilhem Nós
        for(auto it = bytes.rbegin(); it != bytes.rend(); it++){
            uint8_t b = *it;
            if(atual->filhos.find(b) == atual->filhos.end()) {
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
            uint8_t b = *it;
            if(atual->filhos.find(b) != atual->filhos.end())atual = atual->filhos[b];
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

    // Calcula estatísticas de profundidade da trie
    void calcula_profundidades(No* no, int profundidade, vector<int>& contadores) {
        if (!no) return;
        if (profundidade < (int)contadores.size()) {
            contadores[profundidade]++;
        }
        for (auto& [_, filho] : no->filhos) {
            calcula_profundidades(filho, profundidade + 1, contadores);
        }
    }

    void imprime_estatisticas_trie(int kmax) {
        vector<int> contadores(kmax + 2, 0);
        calcula_profundidades(raiz, 0, contadores);
        
        cout << "\n=== ESTATÍSTICAS DA TRIE ===" << endl;
        cout << "Kmax: " << kmax << endl;
        cout << "Distribuição de profundidades:" << endl;
        
        uint64_t total_nos = 0;
        for (int i = 0; i < (int)contadores.size(); i++) {
            if (contadores[i] > 0) {
                cout << "  Profundidade " << i << ": " << contadores[i] << " nós";
                if (i == kmax) cout << " (MAX)";
                cout << endl;
                total_nos += contadores[i];
            }
        }
        
        int nos_em_kmax = contadores[kmax];
        double percentual_kmax = (total_nos > 0) ? (100.0 * nos_em_kmax / total_nos) : 0;
        
        cout << "\nTotal de nós: " << total_nos << endl;
        cout << "Nós em profundidade Kmax: " << nos_em_kmax << " (" << percentual_kmax << "%)" << endl;
        cout << "==========================\n" << endl;
    }

};