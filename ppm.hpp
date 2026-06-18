#pragma once
#include<iostream>
#include<array>
#include<set>
#include<queue>
#include"codificador_aritmetico.hpp"
#include"estrutura_contexto.hpp"

using namespace std;

struct Ppm{
    trie_contexto arvore;
    deque<uint8_t> janela_atual;
    Codificador_aritmetico aritmetico;
    set<uint8_t> excluidos;
    deque<uint8_t> janela_j;
    No* equiprovaveis;

    float p_threshold;
    uint64_t bits_janela_ant;
    uint64_t bits_janela_cur;
    uint32_t simbolos_na_janela;

    int Kmax;
    int J;
    int adaptacao_modo;

    Ppm(int k, int tamanho, int adaptacao, float p = 0.10f){
        Kmax = k;
        J = tamanho;
        adaptacao_modo = adaptacao;
        p_threshold = p;
        bits_janela_ant = 0;
        bits_janela_cur = 0;
        simbolos_na_janela = 0;
        equiprovaveis = new No();
        inicia_equiprovaveis();
    }

    ~Ppm(){
        delete equiprovaveis;
    }

    void inicia_equiprovaveis(){
        for(int i = 0; i < 256; i++) equiprovaveis->frequencias[i] = 1;
        equiprovaveis->total = 256;
    }

    bool existe_contexto(No* contexto, uint8_t simbolo){
        return contexto->frequencias[simbolo] > 0;
    }

    uint32_t calcula_escape(No* contexto){
        uint32_t distintos = 0;
        for(int i = 0; i < 256; i++){
            if(contexto->frequencias[i] > 0) distintos++;
        }
        return max(1u, distintos);
    }

    void insere_em_excluidos(No* contexto){
        for(int i = 0; i < 256; i++){
            if(contexto->frequencias[i] > 0) excluidos.insert(i);
        }
    }

    void atualiza_contexto(uint8_t atual){
        janela_atual.push_back(atual);
        if(janela_atual.size() > (size_t)Kmax)
            janela_atual.pop_front();
        arvore.insere_byte_em_contexto(janela_atual);
    }

    void verifica_adaptacao(){
        if(bits_janela_ant == 0) return;

        double L_ant = (double)bits_janela_ant / J;
        double L_cur = (double)bits_janela_cur / J;

        if(L_cur > L_ant * (1.0 + p_threshold)){
            if(adaptacao_modo == 1){
                executa_reset();
            } else if(adaptacao_modo == 2){
                executa_poda(2);
            }
        }
    }

    void executa_reset(){
        // Grava marcador literal de 32 bits no buffer para o descompressor
        // Usa padrão 0xFFFFFFFF que não pode ocorrer naturalmente no fluxo aritmético
        // (o codificador aritmético nunca emite high == TOP sem renormalizar)
        for(int i = 0; i < 32; i++)
            aritmetico.escreve_bit(1);  // marcador: 32 bits 1

        arvore.reset();
        janela_atual.clear();
        excluidos.clear();
        inicia_equiprovaveis();
        // estado do codificador aritmético (low/high) NÃO é resetado:
        // o fluxo de bits é contínuo
    }

    void executa_poda(uint32_t limiar){
        // Grava marcador diferente: 31 bits 1 seguido de 1 bit 0
        for(int i = 0; i < 31; i++)
            aritmetico.escreve_bit(1);
        aritmetico.escreve_bit(0);

        uint32_t removidos = arvore.poda(limiar);
        janela_atual.clear();
        excluidos.clear();
        cout << "[PODA] Nós removidos: " << removidos << endl;
    }

    void processa_simbolo(uint8_t atual){
        bool codificado = false;
        No* contexto = arvore.busca_contexto_byte(janela_atual);
        No* contexto_inicial = contexto;

        uint64_t bits_antes = aritmetico.bits_buffer.size();

        if((int)janela_j.size() >= J) janela_j.pop_front();
        janela_j.push_back(atual);

        while(contexto){
            if(existe_contexto(contexto, atual) && excluidos.count(atual) == 0){
                if(aritmetico.encode_byte(atual, contexto, excluidos)){
                    codificado = true;
                    if(equiprovaveis->frequencias[atual] > 0){
                        equiprovaveis->frequencias[atual]--;
                        equiprovaveis->total--;
                    }
                    break;
                }
            } else if(excluidos.count(atual) == 0){
                uint32_t freq_esc = calcula_escape(contexto);
                contexto->frequencias[ESCAPE] = freq_esc;
                contexto->total += freq_esc;
                bool escapou = aritmetico.encode_byte(ESCAPE, contexto, excluidos);
                contexto->frequencias[ESCAPE] = 0;
                contexto->total -= freq_esc;
                if(escapou){
                    insere_em_excluidos(contexto);
                }
            }
            contexto = contexto->pai;
        }

        if(!codificado){
            if(aritmetico.encode_byte(atual, equiprovaveis, excluidos)){
                if(equiprovaveis->frequencias[atual] > 0){
                    equiprovaveis->frequencias[atual]--;
                    equiprovaveis->total--;
                }
            }
        }

        arvore.atualiza_frequencia(contexto_inicial, atual);
        atualiza_contexto(atual);
        excluidos.clear();

        // --- Monitoramento de taxa ---
        uint64_t bits_depois = aritmetico.bits_buffer.size();
        bits_janela_cur += (bits_depois - bits_antes);
        simbolos_na_janela++;

        if(simbolos_na_janela >= (uint32_t)J){
            verifica_adaptacao();
            bits_janela_ant = bits_janela_cur;
            bits_janela_cur = 0;
            simbolos_na_janela = 0;
        }
    }
};