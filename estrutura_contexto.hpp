#include<iostream>
#include<map>
#include<algorithm>
using namespace std;
typedef struct No No;

// cada No é um contexto 
// a ideia seria percorrer até a folha e codificar na subida, para dar prioridade ao maior contexto
// Kmax <= 10 => O(Kmax)<= O(10) [acho que é ok]
struct No{
    No* filhos[256];
    bool fim_;
    array<uint32_t,256>frequencias;// armazena as frequencias de cada byte no contexto
    No* pai; 
    uint32_t total; //soma de todas as frequências
    No(){
        fim_ = false;
        fill(filhos,filhos+256,nullptr);
        frequencias.fill(0);
        pai = nullptr;
        total = 0;
    }
};

struct trie_contexto{
    No* raiz;
    trie_contexto(){
        raiz = new No;
    }
    void insere_byte_em_contexto(const vector<uint32_t>&palavra){
        
    }
    void busca_contexto_byte(){
        
    }

};