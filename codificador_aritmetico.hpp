#pragma once
#include <iostream>
#include <vector>
#include<set>
#include<map>
#include<algorithm>
#include<bitset>
#include<cstdint>
#include<fstream>
#include"estrutura_contexto.hpp"
using namespace std;

typedef struct Codificador_aritmetico Codificador_aritmetico;
struct Codificador_aritmetico{

    // ---- Constantes de precisão ----
    // Usamos 32 bits de precisão para low/high.
    // TOP é o maior valor representável (0xFFFFFFFF).
    static const uint32_t BITS_PRECISAO = 32;
    static const uint64_t TOP   = 0xFFFFFFFFULL;
    static const uint64_t HALF  = 0x80000000ULL; // bit mais significativo
    static const uint64_t FIRST_QTR = 0x40000000ULL;
    static const uint64_t THIRD_QTR = 0xC0000000ULL;

    // Intervalo de trabalho do codificador (substitui o low/high double do Ppm)
    uint32_t low  = 0;
    uint32_t high = (uint32_t)TOP;

    // Buffer para armazenar bits
    vector<bool> bits_buffer;

    // Contador de bits pendentes (underflow / E3 scaling)
    uint32_t bits_pendentes = 0;

    // soma as frequências dos símbolos excluídos (para exclusão de contexto no PPM)
    uint32_t contagem_excluidos(No* contexto, const set<uint8_t>& excluidos)
    {
        uint32_t t = 0;
        for(int i=0;i<256;i++){
            if(excluidos.count(i))
                t += contexto->frequencias[i];
        }
        return t;
    }

    // Codifica um símbolo (ou ESCAPE) dado um contexto, atualizando low/high internos.
    // Retorna true se o símbolo foi encontrado e codificado nesse contexto.
    bool encode_byte(
        uint32_t simbolo,
        No* contexto,
        const set<uint8_t>& excluidos)
    {
        auto& freq = contexto->frequencias;

        uint32_t total = contexto->total - contagem_excluidos(contexto, excluidos);

        if(total == 0)
            return false;

        uint32_t cumulativa = 0;
        uint32_t freq_simbolo = 0;
        bool achou = false;

        for(int i=0;i<257;i++){

            if(i != ESCAPE && excluidos.count(i))
                continue;

            if(freq[i] == 0)
                continue;

            if((uint32_t)i == simbolo){
                freq_simbolo = freq[i];
                achou = true;
                break;
            }

            cumulativa += freq[i];
        }

        if(!achou)
            return false;

        // ---- Núcleo da codificação aritmética em inteiros ----
        // range atual
        uint64_t range = (uint64_t)high - (uint64_t)low + 1ULL;

        uint64_t novo_high = (uint64_t)low
            + (range * (uint64_t)(cumulativa + freq_simbolo)) / total - 1ULL;

        uint64_t novo_low = (uint64_t)low
            + (range * (uint64_t)cumulativa) / total;

        low  = (uint32_t)novo_low;
        high = (uint32_t)novo_high;

        renormaliza();

        return true;
    }

    double valor_final(double low_unused, double high_unused){
        // mantido por compatibilidade de assinatura; usa o estado interno em inteiros
        return ((double)low + (double)high) / 2.0 / (double)TOP;
    }

    // Renormalização em inteiros: trata os 3 casos clássicos
    // E1 (MSB=0 nos dois), E2 (MSB=1 nos dois) e E3 (underflow, intervalo no meio)
    void renormaliza(){
        while(true){
            if(high < HALF){
                // ambos começam com bit 0
                escreve_bit_com_pendentes(0);
            }
            else if(low >= HALF){
                // ambos começam com bit 1
                escreve_bit_com_pendentes(1);
                low  -= (uint32_t)HALF;
                high -= (uint32_t)HALF;
            }
            else if(low >= FIRST_QTR && high < THIRD_QTR){
                // underflow: intervalo preso no meio, sem decidir o bit ainda
                bits_pendentes++;
                low  -= (uint32_t)FIRST_QTR;
                high -= (uint32_t)FIRST_QTR;
            }
            else{
                break;
            }

            low  = low  << 1;
            high = (high << 1) | 1;
        }
    }

    // Escreve um bit no buffer
    void escreve_bit(bool bit){
        bits_buffer.push_back(bit);
    }

    // Escreve bit e seus complementos pendentes (acumulados pelo caso E3)
    void escreve_bit_com_pendentes(bool bit){
        escreve_bit(bit);
        for(uint32_t i = 0; i < bits_pendentes; i++){
            escreve_bit(!bit);
        }
        bits_pendentes = 0;
    }

    // Deve ser chamado uma única vez, no final de toda a codificação,
    // para esvaziar o intervalo final em bits (flush).
    void finaliza_codificacao(){
        bits_pendentes++;
        if(low < FIRST_QTR){
            escreve_bit_com_pendentes(0);
        }else{
            escreve_bit_com_pendentes(1);
        }
    }

    // Salva os bits em um arquivo
    bool salva_arquivo(const string& nome_arquivo,const vector<ArquivoInfo>& arquivos,uint64_t tamanho_total_original){
        try{
            ofstream arquivo(nome_arquivo, ios::binary);
            if(!arquivo.is_open()){
                cerr << "[ERRO] Não foi possível abrir arquivo: " << nome_arquivo << endl;
                return false;
            }
            // Coloca os Cabeçalhos: 
            // quantidade de arquivos, nome e tamanho de cada arquivo,
            // tamanho total original (para análise de métricas)
            
            uint64_t quantidade_arquivos = arquivos.size();
            arquivo.write((char*)&quantidade_arquivos, sizeof(uint64_t));
            for(const auto& arq : arquivos){
                uint16_t nome_tamanho = arq.nome.size();
                arquivo.write((char*)&nome_tamanho, sizeof(uint16_t));
                arquivo.write(arq.nome.data(),nome_tamanho);
                arquivo.write((char*)&arq.tamanho, sizeof(uint64_t));

            }
            uint32_t tamanho_bits = bits_buffer.size();
            arquivo.write((char*)&tamanho_bits, sizeof(uint32_t));

            uint8_t byte_atual = 0;
            int bit_count = 0;

            for(size_t i = 0; i < bits_buffer.size(); i++){
                byte_atual = (byte_atual << 1) | (bits_buffer[i] ? 1 : 0);
                bit_count++;

                if(bit_count == 8){
                    arquivo.put(byte_atual);
                    byte_atual = 0;
                    bit_count = 0;
                }
            }

            if(bit_count > 0){
                byte_atual <<= (8 - bit_count);
                arquivo.put(byte_atual);
            }

            arquivo.close();
            cout << "[INFO] Arquivo comprimido salvo: " << nome_arquivo << endl;
            cout << "[INFO] Total de bits: " << bits_buffer.size() << endl;
            cout << "[INFO] Total de bytes: " << (bits_buffer.size() / 8 + (bits_buffer.size() % 8 ? 1 : 0)) << endl;

            return true;
        }catch(exception& e){
            cerr << "[ERRO] Exceção ao salvar arquivo: " << e.what() << endl;
            return false;
        }
    }

    // Limpa o buffer e reseta o estado do codificador para um novo fluxo
    void limpa_buffer(){
        bits_buffer.clear();
        bits_pendentes = 0;
        low = 0;
        high = (uint32_t)TOP;
    }

    // Retorna tamanho do buffer em bytes
    uint64_t tamanho_comprimido(){
        return bits_buffer.size() / 8 + (bits_buffer.size() % 8 ? 1 : 0);
    }
};