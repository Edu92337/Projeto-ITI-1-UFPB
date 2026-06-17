#include <iostream>
#include <vector>
#include<fstream>
#include<filesystem>
#include"codificador_aritmetico.hpp"
#include"estrutura_contexto.hpp"
#include"ppm.hpp"

using namespace std;

void codifica_arquivo(ifstream &arquivo, Ppm &modelo, bool metricas){
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


//  path, Kmax, J, opção de poda ou clear(0,1,2), opção de analise de metricas(1,0) 
int main(int argc , char* argv[]) {
    if(argc < 6){
        cout <<"Uso: ./a path_pasta k j adaptacao metricas\n";
        return 1;
    }
    string path = argv[1];
    int k = atoi(argv[2]);
    int j = atoi(argv[3]);
    int adaptacao  = atoi(argv[4]);
    bool metricas  = atoi(argv[5]) != 0;

    Ppm modelo(k,j,adaptacao);
    //para cada arquivo do diretório codifica o arquivo
    for(auto arq : filesystem::directory_iterator(path)){
        ifstream arquivo(arq.path(),ios::binary);
        if(!arquivo.is_open()){
            cout <<"Erro ao abrir o arquivo!"<<endl;
            return 1;
        }
        codifica_arquivo(arquivo, modelo, metricas);
        //análise das métricas parciais(lf > lo*(1+p)? -> pode ou clear)
    }

    // flush final: emite os bits restantes para fechar o intervalo
    modelo.aritmetico.finaliza_codificacao();

    // Imprime estatísticas da trie
    modelo.arvore.imprime_estatisticas_trie(k);

    // salva o stream de bits resultante em arquivo
    modelo.aritmetico.salva_arquivo("saida.bin");

    cout << "Total de bits emitidos: " << modelo.aritmetico.tamanho_comprimido() * 8 << endl;

    return 0;
}