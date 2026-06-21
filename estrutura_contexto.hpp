#pragma once
#include<iostream>
#include<map>
#include<unordered_map>
#include<algorithm>
#include<vector>
#include<queue>
#include<array>
#include<deque>
#include<cstdint>
using namespace std;
typedef struct No No;

const uint32_t ESCAPE     = 256;
const uint32_t SYM_RESET  = 257; // sinaliza ao decoder: reset total do modelo
const uint32_t SYM_PODA   = 258; // sinaliza ao decoder: poda por envelhecimento
const int NUM_SIMBOLOS_RESERVADOS = 259; // 0..255 dados + ESCAPE + RESET + PODA
const int TEMPOV = 1000; // tempo de vida arbitrário

// cada No é um contexto
// a ideia seria percorrer até a folha e codificar na subida, para dar prioridade ao maior contexto
// Kmax <= 10 => O(Kmax)<= O(10)
struct No{
    unordered_map<uint8_t, No*> filhos;// implementação original uint8_t filhos[257]

    // frequencias: tabela AGREGADA (própria + soma de todos os descendentes),
    // é o que o codificador aritmético usa diretamente para montar a
    // distribuição cumulativa em encode_byte/decode_byte. Continua incluindo
    // posições reservadas (ESCAPE/RESET/PODA), embora estas não persistam
    // entre chamadas -- são injetadas e removidas na hora do uso.
    vector<uint32_t> frequencias;

    // frequencias_proprias: quanto ESTE nó especificamente recebeu como
    // contexto de maior ordem ao longo da compressão (ou seja, as vezes em
    // que este nó foi o contexto_inicial/maior contexto tentado para um
    // símbolo, não contando o que veio "de baixo" via propagação para os
    // pais). Só usa as 256 posições de símbolos de dado -- ESCAPE/RESET/PODA
    // nunca são contados aqui, pois nunca persistem no modelo.
    //
    // É esta tabela que a poda envelhece (divide por 2). A tabela agregada
    // (frequencias) é então RECONSTRUÍDA bottom-up a partir das próprias já
    // envelhecidas de todo o caminho raiz->folha, preservando o invariante:
    //   frequencias[i] (de um nó N) = frequencias_proprias[i] de N
    //                                + soma de frequencias[i] de cada filho de N
    vector<uint32_t> frequencias_proprias;

    No* pai;
    uint32_t total; // soma de frequencias[0..255] (não inclui reservados)
    int tempo_de_vida; // não usado pela poda por envelhecimento; reservado p/ outra estratégia

    No(int tempo = TEMPOV){
        frequencias.assign(NUM_SIMBOLOS_RESERVADOS, 0);
        frequencias_proprias.assign(256, 0);
        pai = nullptr;
        total = 0;
        tempo_de_vida = tempo;
    }
};

// Estrutura relacional que conecta contextos semelhantes
// EX: simbolo : abc => r - c -bc -abc
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
                atual->filhos[b]->pai = atual; // faz a ligação da volta para a busca de Kpossivel -> K = -1(raiz)
            }
            atual = atual->filhos[b];
        }
    }

    // A partir da raiz vai percorrendo os nos que estão conectados/ contextos conectados
    // até encontrar o maior contexto possível
    No* busca_contexto_byte(const deque<uint8_t>&contexto){
        No* atual = raiz;
        for(auto it = contexto.rbegin();it!=contexto.rend();it++){
            uint8_t b = *it;
            if(atual->filhos.find(b) != atual->filhos.end())atual = atual->filhos[b];
            else return atual;
        }
        return atual;
    }

    // faz a propagação para todos os contextos menores até a raiz.
    // `contexto` aqui é sempre o contexto_inicial (maior contexto tentado),
    // então É no nó `contexto` que a frequência é "própria"; a propagação
    // para os ancestrais é só atualização da tabela agregada deles.
    void atualiza_frequencia(No* contexto, uint8_t simbolo){
        atualiza_frequencia_propria(contexto, simbolo);
    }

private:
    // Marca a frequência como "própria" do nó onde o símbolo foi de fato
    // observado como contexto de maior ordem, e propaga a contagem agregada
    // para todos os ancestrais (sem marcá-la como própria deles).
    void atualiza_frequencia_propria(No* contexto, uint8_t simbolo){
        contexto->frequencias_proprias[simbolo]++;
        propaga_agregada(contexto, simbolo);
    }

    void propaga_agregada(No* no, uint8_t simbolo){
        no->frequencias[simbolo]++;
        no->total++;
        if(!no->pai) return;
        propaga_agregada(no->pai, simbolo);
    }

public:
    // Calcula estatísticas de profundidade da trie(DFS recursivo)
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

    void reset() {
        libera(raiz);
        raiz = new No();
    }

    // ---- Poda por envelhecimento exponencial ----
    //
    // Critério: a cada poda, a frequência PRÓPRIA de cada nó (não a
    // agregada) é dividida por 2 (divisão inteira). A tabela agregada
    // (frequencias) de cada nó é então RECONSTRUÍDA bottom-up como:
    //   frequencias[i] = frequencias_proprias[i] (já envelhecida)
    //                   + soma de frequencias[i] de cada filho (após a
    //                     poda do filho, ou seja, já também envelhecida
    //                     recursivamente)
    //
    // Isso preserva o invariante "agregado = próprio + soma dos
    // descendentes" em qualquer instante, inclusive depois da poda --
    // ao contrário de simplesmente dividir a tabela agregada de cada nó
    // de forma independente, o que contaria a mesma divisão múltiplas
    // vezes (uma para o nó, outra embutida de novo nos pais) e
    // descalibraria o modelo.
    //
    // Critério de remoção de nó: depois de envelhecido, se o nó não tem
    // filhos E sua frequência própria é toda zero, ele é removido (his
    // história não tem mais nada a contribuir). Nós internos com
    // frequência própria zerada mas com filhos vivos são mantidos --
    // são apenas estruturais, ainda necessários para a cadeia de pais
    // usada pela propagação bottom-up de futuras atualizações.
    void poda(){
        poda_recursiva(raiz);
    }

private:
    // Executa a poda em pós-ordem e retorna a tabela agregada (256
    // posições, símbolos de dado) já recalculada do nó `no`, para que o
    // chamador (pai) possa somá-la à sua própria contribuição.
    array<uint32_t,256> poda_recursiva(No* no){
        array<uint32_t,256> agregada_filhos{};
        agregada_filhos.fill(0);

        // 1) Poda cada filho primeiro (pós-ordem) e acumula a agregada
        //    resultante de cada um.
        vector<uint8_t> filhos_para_remover;
        for(auto& [simbolo, filho] : no->filhos){
            array<uint32_t,256> agregada_filho = poda_recursiva(filho);

            bool filho_vazio = filho->filhos.empty();
            uint32_t soma_propria_filho = 0;
            for(int i = 0; i < 256; i++) soma_propria_filho += filho->frequencias_proprias[i];

            if(filho_vazio && soma_propria_filho == 0){
                filhos_para_remover.push_back(simbolo);
                // não soma a agregada de um filho que será removido
                continue;
            }

            for(int i = 0; i < 256; i++) agregada_filhos[i] += agregada_filho[i];
        }
        for(uint8_t simbolo : filhos_para_remover){
            delete no->filhos[simbolo];
            no->filhos.erase(simbolo);
        }

        // 2) Envelhece a frequência PRÓPRIA deste nó (divisão inteira por 2).
        for(int i = 0; i < 256; i++){
            no->frequencias_proprias[i] /= 2;
        }

        // 3) Reconstrói a tabela agregada deste nó = própria (já
        //    envelhecida) + agregada dos filhos (já também envelhecida,
        //    recursivamente, pela chamada acima).
        uint32_t novo_total = 0;
        for(int i = 0; i < 256; i++){
            uint32_t valor = no->frequencias_proprias[i] + agregada_filhos[i];
            no->frequencias[i] = valor;
            novo_total += valor;
        }
        // Posições reservadas nunca persistem fora do momento de uso.
        no->frequencias[ESCAPE]    = 0;
        no->frequencias[SYM_RESET] = 0;
        no->frequencias[SYM_PODA]  = 0;
        no->total = novo_total;

        // O que este nó contribui para o pai é a sua tabela agregada
        // recém-recalculada (própria + filhos), não apenas a própria --
        // é exatamente o valor que acabamos de gravar em no->frequencias.
        array<uint32_t,256> agregada_deste_no{};
        for(int i = 0; i < 256; i++) agregada_deste_no[i] = no->frequencias[i];
        return agregada_deste_no;
    }

public:
};