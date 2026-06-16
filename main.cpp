#include <iostream>
#include <vector>
#include<fstream>
#include"codificador_aritmetico.hpp"
#include"estrutura_contexto.hpp"
#include"ppm.hpp"

using namespace std;
//  path, Kmax, J, opção de poda ou clear(0,1,2), opção de analise de metricas(1,0) 
int main(int argc , char* argv[]) {
    string path    = argv[1];
    int k = atoi(argv[2]);
    int j = atoi(argv[3]);
    int adaptacao  = atoi(argv[4]);
    bool metricas  = atoi(argv[5]) != 0;
    if(argc < 6){
        cout <<"Uso: ./a path_arquivo k j adaptacao metricas\n";
        return 1;
    }
    Ppm modelo(k,j,adaptacao);
    ifstream arquivo(path,ios::binary);
    if(!arquivo.is_open()){
        cout <<"Erro ao abrir o arquivo!"<<endl;
        return 1;
    }
    char byte;
    while(arquivo.get(byte)){
        // processa o byte visto
        modelo.processa_simbolo(byte);
        if(metricas){
            //analisa as metricas na ultima janela de J bytes do ppm

        }
    }arquivo.close();
    //analise das metricas finais


    return 0;
}