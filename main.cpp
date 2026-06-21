#include <iostream>
#include <vector>
#include<fstream>
#include<filesystem>
#include"codificador_aritmetico.hpp"
#include"estrutura_contexto.hpp"
#include"ppm.hpp"
#include <cstdlib>
using namespace std;
namespace fs = filesystem;

// intervalo de amostragem para o gráfico: grava 1 linha a cada
// AMOSTRA_INTERVALO símbolos processados, em vez de 1 por símbolo.
// Isso evita CSVs com centenas de milhões de linhas em corpus grandes
// (ex: Silesia) e mantém o gnuplot responsivo.
const uint64_t AMOSTRA_INTERVALO = 5000;

void gera_grafico() {

    FILE* gp = popen("gnuplot", "w");

    if(!gp){
        std::cerr << "Erro ao abrir gnuplot\n";
        return;
    }

    // Gera um arquivo de imagem (PNG) em vez de uma janela interativa.
    // Isso evita depender de qualquer backend gráfico (Qt/wxt/x11), que
    // pode ter conflitos de biblioteca dependendo da instalação do
    // sistema (ex: conflito com libs empacotadas via snap).
    fprintf(gp, "set terminal pngcairo size 1200,700\n");
    fprintf(gp, "set output 'grafico_comprimento_medio.png'\n");

    fprintf(gp, "set title 'Comprimento medio progressivo durante a compressao'\n");
    fprintf(gp, "set xlabel 'Simbolos processados (n)'\n");
    fprintf(gp, "set ylabel 'Comprimento medio (bits/simbolo)'\n");
    fprintf(gp, "set grid\n");
    // Evita que o gnuplot interprete a vírgula do CSV como separador
    // decimal (problema comum em sistemas com locale pt_BR) -- aqui
    // deixamos explícito que vírgula é separador de COLUNA e ponto é
    // separador decimal, independente do locale do sistema.
    fprintf(gp, "set datafile separator ','\n");
    fprintf(gp, "set decimalsign '.'\n");

    fprintf(gp,
        "plot 'resultado_compressao.csv' "
        "using 1:2 with lines title 'PPM'\n"
    );

    pclose(gp);

    cout << "[INFO] Grafico salvo em grafico_comprimento_medio.png" << endl;
}


void codifica_arquivo(ifstream& arquivo,ofstream& dados_grafico, Ppm& modelo, bool metricas){
    char byte;
    while (arquivo.get(byte)){
        // processa o byte visto
        modelo.processa_simbolo((uint8_t)byte);

        

        if (metricas){
            // analisa as metricas na ultima janela de J bytes do ppm
            // amostragem: só grava 1 linha a cada AMOSTRA_INTERVALO símbolos,
            // em vez de 1 linha por símbolo (inviável para corpus grandes).
            // Métrica gravada: comprimento médio PROGRESSIVO, ou seja,
            // janela J usada internamente para a decisão de poda/reset).
            if(modelo.total_simbolos_processados % AMOSTRA_INTERVALO == 0){
                double media_progressiva =
                    (double)modelo.aritmetico.bits_buffer.size()
                    / (double)modelo.total_simbolos_processados;

                dados_grafico << modelo.total_simbolos_processados << ","
                            << media_progressiva << "\n";
            }
        }
    }
    arquivo.close();
}

//  path, Kmax, J, opção de poda ou clear(0,1,2), opção de analise de metricas(1,0) comprime(1,0)
int main(int argc , char* argv[]) {
    if(argc < 7){
        cout <<"Uso: ./a path_pasta k j adaptacao metricas comprime [log_adaptacao]\n";
        return 1;
    }
    string path = argv[1];
    int k = atoi(argv[2]);
    int j = atoi(argv[3]);
    int adaptacao  = atoi(argv[4]);
    bool metricas  = atoi(argv[5]) != 0;
    bool comprime  = atoi(argv[6]) != 0;
    // parâmetro opcional (8º argumento): ativa log detalhado por evento de
    // poda/reset (posição no arquivo + tamanho da trie antes/depois).
    // Default desligado para não poluir a saída em corpus grandes -- o
    // resumo agregado (contagem total) é sempre impresso no final,
    // independente desta flag.
    bool log_adaptacao = (argc >= 8) ? (atoi(argv[7]) != 0) : false;

    ofstream csv("resultado_compressao.csv");

    if(comprime){
        Ppm modelo(k,j,adaptacao,0.1f,log_adaptacao);
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
                codifica_arquivo(arquivo,csv, modelo, metricas);
                //análise das métricas parciais(lf > lo*(1+p)? -> pode ou clear)
            }
        }

        cout << "[INFO] Arquivos encontrados: " << arquivos_count << endl;
        cout << "[INFO] Tamanho original: " << tamanho_original << " bytes" << endl;

        // IMPORTANTE: força a escrita física do CSV no disco antes de
        // chamar o gnuplot. Sem isso, os dados podem ainda estar só no
        // buffer interno do ofstream (o destructor de "csv" só roda
        // quando main() retorna, o que é DEPOIS de gera_grafico()),
        // e o gnuplot lê um arquivo vazio/incompleto.
        csv.flush();
        csv.close();

        // flush final: emite os bits restantes para fechar o intervalo
        modelo.aritmetico.finaliza_codificacao();

        // Imprime estatísticas da trie
        modelo.arvore.imprime_estatisticas_trie(k);

        // Imprime o resumo de eventos de adaptação (poda/reset): quantos
        // dispararam ao longo de toda a compressão, independente da flag
        // de log detalhado por evento.
        modelo.imprime_resumo_adaptacao();

        // salva o stream de bits resultante em arquivo
        modelo.aritmetico.salva_arquivo("saida.bin");
        // gera o gráfico de comprimento médio no final da compressão
        if(metricas) gera_grafico();

        cout << "[INFO] Tamanho comprimido: " << modelo.aritmetico.tamanho_comprimido() << " bytes" << endl;
        cout << "Razão de Compressão: " 
             << (100.0 *(1- (double)modelo.aritmetico.tamanho_comprimido() / tamanho_original))
             << " %" << endl;

    }else{
        // Inicia o descompressor
        csv.close();
    }

    return 0;
}