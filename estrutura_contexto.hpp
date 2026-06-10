#include<iostream>
#include<map>
#include<algorithm>
using namespace std;
typedef struct No NO;

struct No{
    unordered_map<uint8_t,No*>filhos;
    bool fim_palavra;
    No(){
        fim_palavra = false;
    }
};

struct trie_contexto{
    No* raiz;
    trie_contexto(){
        raiz = new No;
    }
    void insere_byte(){

    }
    void busca_byte(){
        
    }
};