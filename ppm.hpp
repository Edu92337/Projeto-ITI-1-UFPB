#pragma once
#include<iostream>
#include<array>
#include<set>
#include<queue>
#include"codificador_aritmetico.hpp"
#include"estrutura_contexto.hpp"

using namespace std;
typedef struct Simbolo Simbolo;
struct Simbolo{
    uint8_t byte;
    uint64_t bits_emitidos; // sequência de bits emitida pelo codificador
};


struct Ppm{
    trie_contexto arvore;
    deque<uint8_t> janela_atual;
    Codificador_aritmetico aritmetico;
    set<uint8_t>excluidos; // bytes ja vistos -> precisa ser limpo a cada novo byte 
    deque<Simbolo> janela_j; // janela para acompanhar as métricas
    No* equiprovaveis;
    // Kmax e J vão ser passados como parâmetros da compilação
    int Kmax;
    int J;
    float p;
    int adapta;
    uint64_t bits_inicial;
    uint64_t bits_final;
    uint64_t comprimento_emitido; // para análise de métricas
    uint64_t total_simbolos_processados;
    Ppm(int k, int tamanho,int adaptacao,float per = 0.1f){
        Kmax = k;
        J = tamanho;
        equiprovaveis = new No();
        inicia_equiprovaveis();
        p = per;
        adapta = adaptacao;
        total_simbolos_processados = 0;
    }

    ~Ppm(){
        delete equiprovaveis;
    }

    void inicia_equiprovaveis(){
        for(int i = 0;i<256;i++)equiprovaveis -> frequencias[i]=1;
        equiprovaveis->total = 256;
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

    uint32_t calcula_escape(No* contexto){
        uint32_t distintos = 0;

        for(int i=0;i<256;i++){
            if(contexto->frequencias[i] > 0)
                distintos++;
        }

        return max(1u, distintos);
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
        total_simbolos_processados++;
        bool codificado = false;
        double l0 = calcula_comprimento_medio();
        bits_inicial = aritmetico.bits_buffer.size();
        No* contexto = arvore.busca_contexto_byte(janela_atual);
        No* contexto_inicial = contexto;
        //atualiza a janela para as métricas
        
        
        // percorre, subindo, procurando onde codificar o simbolo
        // a raiz está inclusa
        while(contexto){
            if(existe_contexto(contexto,atual) && excluidos.count(atual)==0){
                // Tenta codificar o símbolo neste contexto
                if(aritmetico.encode_byte(atual,contexto,excluidos)){
                    codificado = true;
                    if(equiprovaveis->frequencias[atual]>0){
                        equiprovaveis->frequencias[atual]--;
                        equiprovaveis->total --;
                    }
                    break;
                }
            }else if(excluidos.count(atual)==0){
                // símbolo não existe neste contexto (e não está excluído):
                // emite ESCAPE para sinalizar fallback ao contexto menor
                // ESCAPE = número de símbolos distintos do contexto (não acumula estado)
                uint32_t freq_esc = calcula_escape(contexto);
                contexto->frequencias[ESCAPE] = freq_esc;
                contexto->total += freq_esc;
                bool escapou = aritmetico.encode_byte(ESCAPE, contexto, excluidos);
                // desfaz injeção temporária — ESCAPE não persiste nas frequências
                contexto->frequencias[ESCAPE] = 0;
                contexto->total -= freq_esc;
                if(escapou){
                    insere_em_excluidos(contexto);
                    // Continua para tentar contextos menores
                }
            }
            // se atual está em excluidos: sobe silenciosamente, ESCAPE já foi
            // emitido pelo nível que originou a exclusão
            contexto = contexto->pai;
        }
        

        if(codificado == false){
            // Se não codificou em nenhum contexto, codifica com equiprováveis
            if(aritmetico.encode_byte(atual,equiprovaveis,excluidos)){
                if(equiprovaveis->frequencias[atual]>0) {
                    equiprovaveis->frequencias[atual]--;
                    equiprovaveis->total--;
                }
            }
        }

        // atualiza o tamanho emitido para o símbolo atual
        bits_final = aritmetico.bits_buffer.size();  

        if(janela_j.size()>=J) janela_j.pop_front();

        janela_j.push_back({atual,bits_final - bits_inicial});
        double lf = calcula_comprimento_medio();

        // sempre propaga a partir do contexto de ORDEM MAIS ALTA
        // tentado (contexto_inicial), independente de qual nível efetivamente
        // codificou o símbolo. Isso garante que todos os contextos no caminho
        // -1,0,1,...,Kmax aprendam sobre "atual", inclusive os que deram ESCAPE.
        arvore.atualiza_frequencia(contexto_inicial,atual);

        atualiza_contexto(atual);
        // limpar excluidos para processar um byte novo
        excluidos.clear();

        if(lf >(1+p)*l0){
            comprimento_emitido = calcula_comprimento_medio();
            if(adapta == 1) arvore.poda();
            else if(adapta == 2) executa_reset();

        }
    }

    void executa_reset(){
        // estado do codificador aritmético (low/high) NÃO é resetado:
        // o fluxo de bits é contínuo
        arvore.reset();
        janela_atual.clear();
        excluidos.clear();
        inicia_equiprovaveis();
    }

    double calcula_comprimento_medio(){
        // calcula o comprimento médio emitido na janela J mais recente
        if(janela_j.empty())return 0.0;
        uint64_t bits_emitidos_janela = 0;
        for(const auto& simbolo : janela_j){
            bits_emitidos_janela += simbolo.bits_emitidos;

        }
        return (double)bits_emitidos_janela / (double)janela_j.size();
    }

};