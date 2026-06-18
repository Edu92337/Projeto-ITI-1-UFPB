#include <iostream>
#include <vector>
#include<fstream>
#include<filesystem>
#include"codificador_aritmetico.hpp"
#include"estrutura_contexto.hpp"
#include"ppm.hpp"

using namespace std;
namespace fs = filesystem;

void codifica_arquivo(ifstream& arquivo, Ppm& modelo, bool metricas){
    char byte;
    while (arquivo.get(byte)){
        // processa o byte visto
        modelo.processa_simbolo((uint8_t)byte);
        if (metricas){
            // analisa as metricas na ultima janela de J bytes do ppm
        }
    }
    arquivo.close();
}

//  path, Kmax, J, opção de poda ou clear(0,1,2), opção de analise de metricas(1,0) comprime(1,0)
int main(int argc , char* argv[]) {
    if(argc < 7){
        cout <<"Uso: ./a path_pasta k j adaptacao metricas comprime\n";
        return 1;
    }
    string path = argv[1];
    int k = atoi(argv[2]);
    int j = atoi(argv[3]);
    int adaptacao  = atoi(argv[4]);
    bool metricas  = atoi(argv[5]) != 0;
    bool comprime  = atoi(argv[6]) != 0;

    if(comprime){
        Ppm modelo(k,j,adaptacao);
        uintmax_t tamanho_original = 0;
        uintmax_t arquivos_count = 0;
        error_code ec;

        for(auto& entrada : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec)){
            if(ec){ 
                cerr << "Erro: " << ec.message() << endl; return 1; 
            }

            if(fs::is_regular_file(entrada.status())){
                tamanho_original += fs::file_size(entrada.path(), ec);
                arquivos_count++;

                ifstream arquivo(entrada.path(), ios::binary);
                if(!arquivo.is_open()){
                    cerr << "Erro ao abrir: " << entrada.path() << endl;
                    return 1;
                }
                codifica_arquivo(arquivo, modelo, metricas);
                //análise das métricas parciais(lf > lo*(1+p)? -> pode ou clear)
            }
        }

        cout << "[INFO] Arquivos encontrados: " << arquivos_count << endl;
        cout << "[INFO] Tamanho original: " << tamanho_original << " bytes" << endl;

        // flush final: emite os bits restantes para fechar o intervalo
        modelo.aritmetico.finaliza_codificacao();

        // Imprime estatísticas da trie
        modelo.arvore.imprime_estatisticas_trie(k);

        // salva o stream de bits resultante em arquivo
        modelo.aritmetico.salva_arquivo("saida.bin");

        cout << "[INFO] Tamanho comprimido: " << modelo.aritmetico.tamanho_comprimido() << " bytes" << endl;
        cout << "Razão de Compressão: " 
             << (100.0 *(1- (double)modelo.aritmetico.tamanho_comprimido() / tamanho_original))
             << " %" << endl;

    }else{
        // Inicia o descompressor
    }

    return 0;
}