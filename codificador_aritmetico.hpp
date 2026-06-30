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

typedef struct ArquivoInfo{
    string nome;
    uintmax_t tamanho;
}ArquivoInfo;

typedef struct Codificador_aritmetico Codificador_aritmetico;
struct Codificador_aritmetico{

    // ---- Constantes de precisão ----
    static const uint32_t BITS_PRECISAO = 32;
    static const uint64_t TOP      = 0xFFFFFFFFULL;
    static const uint64_t HALF     = 0x80000000ULL;
    static const uint64_t FIRST_QTR = 0x40000000ULL;
    static const uint64_t THIRD_QTR = 0xC0000000ULL;

    // ---- Estado compartilhado encode/decode ----
    uint32_t low  = 0;
    uint32_t high = (uint32_t)TOP;
    uint64_t bits_lidos = 0; // ← contador de bits lidos do fluxo (decode)
    uint64_t bits_consumidos_total;
    // ---- Estado exclusivo do encode ----
    vector<bool> bits_buffer;
    uint32_t bits_pendentes = 0;
     uint64_t bits_emitidos_total = 0;
    // ---- Estado exclusivo do decode ----
    // Registrador de 32 bits com os próximos bits do fluxo já lidos.
    // Inicializado uma única vez em prepara_decodificacao() e atualizado
    // incrementalmente em renormaliza_leitura() — um bit novo é inserido
    // pela direita a cada iteração, espelhando o que o encoder emitiu.
    uint32_t value = 0;

    // Leitura via shift-register de 1 byte: evita overhead de deque<bool>.
    // A cada le_bit(), se o byte atual foi esgotado, lê o próximo do arquivo.
    uint8_t byte_leitura        = 0;
    int     bits_restantes_byte = 0;

    // =========================================================
    // FUNÇÕES COMPARTILHADAS
    // =========================================================

    // Soma as frequências dos símbolos excluídos (exclusão de contexto PPM).
    uint32_t contagem_excluidos(No* contexto, const set<uint8_t>& excluidos)
    {
        uint32_t t = 0;
        for(int i = 0; i < 256; i++){
            if(excluidos.count(i))
                t += contexto->frequencias[i];
        }
        return t;
    }

    // =========================================================
    // ENCODE
    // =========================================================

    // Codifica um símbolo (0..255 ou ESCAPE=256) dado um contexto.
    // Retorna true se o símbolo foi encontrado e codificado.
    bool encode_byte(
        uint32_t simbolo,
        No* contexto,
        const set<uint8_t>& excluidos)
    {
        auto& freq = contexto->frequencias;
        uint32_t total = contexto->total - contagem_excluidos(contexto, excluidos);

        if(total == 0)
            return false;

        uint32_t cumulativa  = 0;
        uint32_t freq_simbolo = 0;
        bool achou = false;

        for(int i = 0; i < 257; i++){
            if(i != (int)ESCAPE && excluidos.count(i)) continue;
            if(freq[i] == 0) continue;

            if((uint32_t)i == simbolo){
                freq_simbolo = freq[i];
                achou = true;
                break;
            }
            cumulativa += freq[i];
        }

        if(!achou) return false;

        uint64_t range = (uint64_t)high - (uint64_t)low + 1ULL;

        uint64_t novo_high = (uint64_t)low
            + (range * (uint64_t)(cumulativa + freq_simbolo)) / total - 1ULL;
        uint64_t novo_low  = (uint64_t)low
            + (range * (uint64_t)cumulativa) / total;

        low  = (uint32_t)novo_low;
        high = (uint32_t)novo_high;

        renormaliza();
        return true;
    }

    // Renormalização do encoder: emite bits e expande o intervalo.
    // Casos E1 (MSB=0), E2 (MSB=1), E3 (underflow no meio).
    void renormaliza(){
        while(true){
            if(high < HALF){
                escreve_bit_com_pendentes(0);
            }
            else if(low >= HALF){
                escreve_bit_com_pendentes(1);
                low  -= (uint32_t)HALF;
                high -= (uint32_t)HALF;
            }
            else if(low >= FIRST_QTR && high < THIRD_QTR){
                bits_pendentes++;
                low  -= (uint32_t)FIRST_QTR;
                high -= (uint32_t)FIRST_QTR;
            }
            else break;

            low  =  low << 1;
            high = (high << 1) | 1;
        }
    }

    void escreve_bit(bool bit){
        bits_buffer.push_back(bit);
        bits_emitidos_total++;
    }

    void escreve_bit_com_pendentes(bool bit){
        escreve_bit(bit);
        for(uint32_t i = 0; i < bits_pendentes; i++)
            escreve_bit(!bit);
        bits_pendentes = 0;
    }

    // Flush final: emite os bits restantes para fechar o intervalo.
    // Deve ser chamado UMA ÚNICA VEZ ao fim de toda a codificação.
    void finaliza_codificacao(){
        bits_pendentes++;
        if(low < FIRST_QTR)
            escreve_bit_com_pendentes(0);
        else
            escreve_bit_com_pendentes(1);
    }

    // Salva o stream de bits no arquivo com cabeçalho:
    //   [uint64_t: qtd arquivos]
    //   para cada arquivo: [uint16_t: len_nome][nome][uint64_t: tamanho_bytes]
    //   [uint32_t: tamanho_bits]
    //   [dados comprimidos, MSB first]
    bool salva_arquivo(const string& nome_arquivo,
                       const vector<ArquivoInfo>& arquivos_vet,
                       uint64_t tamanho_total_original)
    {
        try{
            ofstream arquivo(nome_arquivo, ios::binary);
            if(!arquivo.is_open()){
                cerr << "[ERRO] Não foi possível abrir arquivo: " << nome_arquivo << endl;
                return false;
            }

            uint64_t quantidade_arquivos = arquivos_vet.size();
            arquivo.write((char*)&quantidade_arquivos, sizeof(uint64_t));

            for(const auto& arq : arquivos_vet){
                uint16_t nome_tamanho = (uint16_t)arq.nome.size();
                arquivo.write((char*)&nome_tamanho, sizeof(uint16_t));
                arquivo.write(arq.nome.data(), nome_tamanho);
                uint64_t tam = (uint64_t)arq.tamanho;
                arquivo.write((char*)&tam, sizeof(uint64_t));
            }

            uint32_t tamanho_bits = (uint32_t)bits_buffer.size();
            arquivo.write((char*)&tamanho_bits, sizeof(uint32_t));

            uint8_t byte_atual = 0;
            int bit_count = 0;
            for(size_t i = 0; i < bits_buffer.size(); i++){
                byte_atual = (byte_atual << 1) | (bits_buffer[i] ? 1 : 0);
                bit_count++;
                if(bit_count == 8){
                    arquivo.put(byte_atual);
                    byte_atual = 0;
                    bit_count  = 0;
                }
            }
            if(bit_count > 0){
                byte_atual <<= (8 - bit_count);
                arquivo.put(byte_atual);
            }

            arquivo.close();
            cout << "[INFO] Arquivo comprimido salvo: " << nome_arquivo << endl;
            cout << "[INFO] Total de bits: " << bits_buffer.size() << endl;
            cout << "[INFO] Total de bytes: "
                 << (bits_buffer.size() / 8 + (bits_buffer.size() % 8 ? 1 : 0)) << endl;
            return true;
        }catch(exception& e){
            cerr << "[ERRO] Exceção ao salvar arquivo: " << e.what() << endl;
            return false;
        }
    }

    void limpa_buffer(){
        bits_buffer.clear();
        bits_pendentes = 0;
        low  = 0;
        high = (uint32_t)TOP;
    }

    uint64_t tamanho_comprimido(){
        return bits_buffer.size() / 8 + (bits_buffer.size() % 8 ? 1 : 0);
    }

    // =========================================================
    // DECODE
    // =========================================================

    // Lê o próximo bit do fluxo via shift-register de 1 byte.
    // Retorna 0 como padding implícito se o arquivo acabar — o
    // finaliza_codificacao() do encoder garante bits suficientes.
    bool le_bit(ifstream& arquivo_bits){
        if(bits_restantes_byte == 0){
            uint8_t b = 0;
            arquivo_bits.get((char&)b);
            byte_leitura        = b;
            bits_restantes_byte = 8;
        }
        bool bit = (byte_leitura >> 7) & 1;
        byte_leitura <<= 1;
        bits_restantes_byte--;
        bits_lidos++; // ← adiciona aqui
        return bit;
    }

    // Inicializa o registrador `value` com os primeiros 32 bits do fluxo.
    // Deve ser chamado UMA ÚNICA VEZ, após ler o cabeçalho do arquivo,
    // antes do primeiro decode_byte().
    void prepara_decodificacao(ifstream& arquivo_bits){
        byte_leitura        = 0;
        bits_restantes_byte = 0;
        low   = 0;
        high  = (uint32_t)TOP;
        value = 0;
        bits_consumidos_total = 0; // ← reinicia contador de bits consumidos
        for(int i = 0; i < 32; i++)
            value = (value << 1) | (le_bit(arquivo_bits) ? 1 : 0);
    }

    // Renormalização do decoder: espelha renormaliza() bit a bit.
    // Para cada bit que o encoder emitiu (E1/E2) ou postergou (E3),
    // o decoder descarta a informação correspondente de `value` e
    // insere um novo bit lido do arquivo pela direita.
    void renormaliza_leitura(ifstream& arquivo_bits){
        while(true){
            if(high < HALF){
                // E1: encoder emitiu 0
                low   =  low        << 1;
                high  = (high       << 1) | 1;
                value = (value      << 1) | (le_bit(arquivo_bits) ? 1 : 0);
            }
            else if(low >= HALF){
                // E2: encoder emitiu 1
                low   = (low   - (uint32_t)HALF) << 1;
                high  = ((high - (uint32_t)HALF) << 1) | 1;
                value = ((value - (uint32_t)HALF) << 1) | (le_bit(arquivo_bits) ? 1 : 0);
            }
            else if(low >= FIRST_QTR && high < THIRD_QTR){
                // E3: underflow – Ajusta os intervalos e consome o bit do arquivo normalmente!
                low   = (low   - (uint32_t)FIRST_QTR) << 1;
                high  = ((high - (uint32_t)FIRST_QTR) << 1) | 1;
                value = ((value - (uint32_t)FIRST_QTR) << 1) | (le_bit(arquivo_bits) ? 1 : 0);
            }
            else break;
        }
    }

    // Decodifica um símbolo do fluxo usando o contexto dado.
    // Retorna ESCAPE=256 se o contexto estiver vazio (total==0 após excluídos)
    // ou se o intervalo decodificado corresponder ao ESCAPE injetado pelo PPM.
    uint32_t decode_byte(No* contexto, const set<uint8_t>& excluidos, ifstream& arquivo_bits){
        auto& freq = contexto->frequencias;
        
        // Calcula o total real considerando as exclusões dinâmicas
        uint32_t total = contexto->total - contagem_excluidos(contexto, excluidos);

        // Se o total for de fato 0 e não houver frequência de ESCAPE injetada,
        // significa que não há dados válidos matematicamente.
        if(total == 0) return ESCAPE;

        uint64_t range = (uint64_t)high - (uint64_t)low + 1ULL;
        uint64_t ponto = ((((uint64_t)value - (uint64_t)low + 1ULL) * (uint64_t)total) - 1ULL) / range;

        uint32_t cumulativa = 0;
        uint32_t freq_simbolo = 0;
        uint32_t simbolo_encontrado = 0;
        bool encontrou = false;

        for(int i = 0; i < 257; i++) {
            if(i != (int)ESCAPE && excluidos.count((uint8_t)i)) continue;
            if(freq[i] == 0) continue;

            if(ponto >= cumulativa && ponto < cumulativa + freq[i]) {
                simbolo_encontrado = (uint32_t)i;
                freq_simbolo = freq[i];
                encontrou = true;
                break;
            }
            cumulativa += freq[i];
        }

        if(!encontrou) {
            // Fallback de segurança se falhar a precisão matemática
            return ESCAPE;
        }

        // Renormalização obrigatória do decodificador (Deve ser executada SEMPRE que ler um símbolo/escape)
        uint64_t next_low = (uint64_t)low + (range * cumulativa) / total;
        uint64_t next_high = (uint64_t)low + (range * (cumulativa + freq_simbolo)) / total - 1ULL;
        low = (uint32_t)next_low;
        high = (uint32_t)next_high;

        renormaliza_leitura(arquivo_bits);

        return simbolo_encontrado;
    }
};