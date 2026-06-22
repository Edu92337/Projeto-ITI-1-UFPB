#include <iostream>
#include <vector>
#include <fstream>
#include <filesystem>
#include "codificador_aritmetico.hpp"
#include "estrutura_contexto.hpp"
#include "ppm.hpp"
#include <cstdlib>
using namespace std;
namespace fs = filesystem;

const uint64_t AMOSTRA_INTERVALO = 5000;

void gera_grafico(){
    FILE* gp = popen("gnuplot", "w");
    if(!gp){ cerr << "Erro ao abrir gnuplot\n"; return; }
    fprintf(gp, "set terminal pngcairo size 1200,700\n");
    fprintf(gp, "set output 'grafico_comprimento_medio.png'\n");
    fprintf(gp, "set title 'Comprimento medio progressivo durante a compressao'\n");
    fprintf(gp, "set xlabel 'Simbolos processados (n)'\n");
    fprintf(gp, "set ylabel 'Comprimento medio (bits/simbolo)'\n");
    fprintf(gp, "set grid\n");
    fprintf(gp, "set datafile separator ','\n");
    fprintf(gp, "set decimalsign '.'\n");
    fprintf(gp, "plot 'resultado_compressao.csv' using 1:2 with lines title 'PPM'\n");
    pclose(gp);
    cout << "[INFO] Grafico salvo em grafico_comprimento_medio.png" << endl;
}

void codifica_arquivo(ifstream& arquivo, ofstream& dados_grafico,
                      Ppm& modelo, bool metricas)
{
    char byte;
    while(arquivo.get(byte)){
        modelo.processa_simbolo((uint8_t)byte);
        if(metricas && modelo.total_simbolos_processados % AMOSTRA_INTERVALO == 0){
            double media = (double)modelo.aritmetico.bits_buffer.size()
                         / (double)modelo.total_simbolos_processados;
            dados_grafico << modelo.total_simbolos_processados << "," << media << "\n";
        }
    }
    arquivo.close();
}

void descomprime_arquivo(const string& nome_arquivo_comprimido,
                         int k, int j, int adaptacao, bool log_adaptacao)
{
    ifstream arquivo(nome_arquivo_comprimido, ios::binary);
    if(!arquivo.is_open()){
        cerr << "[ERRO] Não foi possível abrir: " << nome_arquivo_comprimido << endl;
        return;
    }

    cout << "[INFO] Iniciando descompressão de: " << nome_arquivo_comprimido << endl;

    // 1. Lê quantidade de arquivos
    uint64_t quantidade_arquivos;
    arquivo.read((char*)&quantidade_arquivos, sizeof(uint64_t));
    cout << "[INFO] Quantidade de arquivos: " << quantidade_arquivos << endl;

    // 2. Lê metadados de cada arquivo
    vector<ArquivoInfo> arquivos;
    uint64_t tamanho_total_original = 0;
    for(uint64_t i = 0; i < quantidade_arquivos; i++){
        uint16_t nome_tamanho;
        arquivo.read((char*)&nome_tamanho, sizeof(uint16_t));
        string nome(nome_tamanho, '\0');
        arquivo.read(&nome[0], nome_tamanho);
        uint64_t tamanho;
        arquivo.read((char*)&tamanho, sizeof(uint64_t));
        arquivos.push_back({nome, tamanho});
        tamanho_total_original += tamanho;
    }

    // 3. Lê tamanho dos bits comprimidos (usado só para informação)
    uint32_t tamanho_bits;
    arquivo.read((char*)&tamanho_bits, sizeof(uint32_t));
    cout << "[INFO] Bits comprimidos: " << tamanho_bits << endl;

    // 4. Inicializa PPM com os mesmos parâmetros da compressão
    Ppm modelo(k, j, adaptacao, 0.1f, log_adaptacao);

    // Carrega os primeiros 32 bits do fluxo no registrador `value`.
    // DEVE ser chamado após ler todo o cabeçalho, com o ifstream
    // já posicionado no início dos dados comprimidos.
    modelo.aritmetico.prepara_decodificacao(arquivo);

    // 5. Descomprime arquivo a arquivo
    uint64_t total_bytes_escritos = 0;
    for(const auto& arq : arquivos){
        cout << "[INFO] Descomprimindo: " << arq.nome
             << " (" << arq.tamanho << " bytes)" << endl;

        ofstream saida(arq.nome, ios::binary);
        if(!saida.is_open()){
            cerr << "[ERRO] Não foi possível criar: " << arq.nome << endl;
            continue;
        }

        uint64_t progresso_anterior = 0;
        for(uint64_t i = 0; i < arq.tamanho; i++){
            uint8_t byte = modelo.decodifica_simbolo(arquivo);
            saida.write((char*)&byte, 1);
            total_bytes_escritos++;

            uint64_t progresso = (i * 100) / arq.tamanho;
            if(progresso != progresso_anterior && progresso % 10 == 0){
                cout << "\r  Progresso: " << progresso << "%" << flush;
                progresso_anterior = progresso;
            }
        }
        cout << "\r  [OK] " << arq.nome << " (" << arq.tamanho << " bytes)" << endl;
        saida.close();
    }

    arquivo.close();

    cout << "[INFO] Descompressão concluída!" << endl;
    cout << "[INFO] Total de bytes escritos: " << total_bytes_escritos << endl;
    cout << "[INFO] Tamanho original esperado: " << tamanho_total_original << " bytes" << endl;

    if(total_bytes_escritos == tamanho_total_original)
        cout << "[OK] Verificação de integridade: PASSOU" << endl;
    else{
        cout << "[ERRO] Verificação de integridade: FALHOU" << endl;
        cout << "  Esperado: " << tamanho_total_original
             << "  Obtido: "  << total_bytes_escritos << endl;
    }
}

// Uso:
//   compressão:   ./a path_pasta k j adaptacao metricas 1 [log_adaptacao]
//   descompressão:./a saida.bin  k j adaptacao 0        0 [log_adaptacao]
//   (k, j, adaptacao DEVEM ser os mesmos usados na compressão)
int main(int argc, char* argv[]){
    if(argc < 7){
        cout << "Uso: ./a path k j adaptacao metricas comprime [log_adaptacao]\n";
        return 1;
    }
    string path     = argv[1];
    int    k        = atoi(argv[2]);
    int    j        = atoi(argv[3]);
    int    adaptacao = atoi(argv[4]);
    bool   metricas  = atoi(argv[5]) != 0;
    bool   comprime  = atoi(argv[6]) != 0;
    bool   log_adaptacao = (argc >= 8) ? (atoi(argv[7]) != 0) : false;

    ofstream csv("resultado_compressao.csv");

    if(comprime){
        Ppm modelo(k, j, adaptacao, 0.1f, log_adaptacao);
        uintmax_t tamanho_original = 0;
        uintmax_t arquivos_count   = 0;
        error_code ec;
        vector<ArquivoInfo> arquivos;

        cout << "[INFO] Iniciando compressão (k=" << k
             << " j=" << j << " adapta=" << adaptacao << ")" << endl;

        for(auto& entrada : fs::recursive_directory_iterator(
                path, fs::directory_options::skip_permission_denied, ec))
        {
            if(ec){ cerr << "Erro: " << ec.message() << endl; return 1; }

            if(fs::is_regular_file(entrada.status())){
                uint64_t tamanho = fs::file_size(entrada.path(), ec);
                string   nome    = entrada.path().filename().string();
                arquivos.push_back({nome, tamanho});
                tamanho_original += tamanho;
                arquivos_count++;

                cout << "  Comprimindo: " << nome << " (" << tamanho << " bytes)" << endl;

                ifstream arquivo(entrada.path(), ios::binary);
                if(!arquivo.is_open()){
                    cerr << "Erro ao abrir: " << entrada.path() << endl;
                    return 1;
                }
                codifica_arquivo(arquivo, csv, modelo, metricas);
            }
        }

        cout << "[INFO] Arquivos: " << arquivos_count
             << "  Original: " << tamanho_original << " bytes" << endl;

        csv.flush();
        csv.close();

        modelo.aritmetico.finaliza_codificacao();
        modelo.arvore.imprime_estatisticas_trie(k);
        modelo.imprime_resumo_adaptacao();

        // tamanho_total_original é passado mas não usado no cabeçalho atual
        // (cada arquivo já tem seu tamanho individual); mantido para eventual uso futuro.
        modelo.aritmetico.salva_arquivo("saida.bin", arquivos, tamanho_original);

        if(metricas) gera_grafico();

        cout << "[INFO] Tamanho comprimido: "
             << modelo.aritmetico.tamanho_comprimido() << " bytes" << endl;
        cout << "Razão de Compressão: "
             << (100.0 * (1.0 - (double)modelo.aritmetico.tamanho_comprimido()
                                / (double)tamanho_original))
             << " %" << endl;

    } else {
        csv.close();
        descomprime_arquivo(path, k, j, adaptacao, log_adaptacao);
    }

    return 0;
}