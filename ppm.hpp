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

    // ---- Contadores e log de eventos de adaptação (poda/reset) ----
    uint64_t total_podas;
    uint64_t total_resets;
    bool log_eventos_adaptacao; // se true, imprime uma linha por evento (poda ou reset)

    Ppm(int k, int tamanho,int adaptacao,float per = 0.4f, bool log_eventos = false){
        Kmax = k;
        J = tamanho;
        equiprovaveis = new No();
        inicia_equiprovaveis();
        p = per;
        adapta = adaptacao;
        total_simbolos_processados = 0;
        total_podas = 0;
        total_resets = 0;
        log_eventos_adaptacao = log_eventos;
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
        while(contexto) {
            if(excluidos.count(atual) == 0) {
                // Injeta ESCAPE temporariamente para que decode_byte() 
                //veja a mesma distribuição de probabilidade que encode_byte()
                // estava dando erro de sincronia, o decode não conseguia 
                // ver a mesma distribuição
                uint32_t freq_esc = calcula_escape(contexto);
                contexto->frequencias[ESCAPE] = freq_esc;
                contexto->total += freq_esc;

                if(existe_contexto(contexto, atual)) {
                    // Símbolo existe: codifica ele usando a tabela que INCLUI o ESCAPE
                    aritmetico.encode_byte(atual, contexto, excluidos);
                    codificado = true;

                    // Remove a injeção temporária antes de terminar
                    contexto->frequencias[ESCAPE] = 0;
                    contexto->total -= freq_esc;

                    if(equiprovaveis->frequencias[atual] > 0) {
                        equiprovaveis->frequencias[atual]--;
                        equiprovaveis->total--;
                    }
                    break; // Termina o loop, encontramos o símbolo
                } 
                else {
                    // Símbolo não existe: codifica o ESCAPE
                    aritmetico.encode_byte(ESCAPE, contexto, excluidos);

                    // Remove a injeção temporária
                    contexto->frequencias[ESCAPE] = 0;
                    contexto->total -= freq_esc;

                    insere_em_excluidos(contexto);
                }
            }
            // Se o símbolo já foi excluído por um nível superior, sobe silenciosamente
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
            sinaliza_e_aplica_adaptacao();
        }
    }

    void sinaliza_e_aplica_adaptacao(){
        // excluidos já foi limpo antes desta chamada (linha acima, no fim
        // do processamento do símbolo de dado) -- o marcador nunca deve
        // ser afetado por exclusões herdadas do símbolo anterior.
        if(adapta == 1){
            total_podas++;
            if(log_eventos_adaptacao){
                cout << "[PODA #" << total_podas
                     << "] simbolo=" << total_simbolos_processados
                     << " nos_trie_antes=" << conta_nos_trie()
                     << endl;
            }
            arvore.poda();
            if(log_eventos_adaptacao){
                cout << "         nos_trie_depois=" << conta_nos_trie() << endl;
            }
        }else if(adapta == 2){
            total_resets++;
            if(log_eventos_adaptacao){
                cout << "[RESET #" << total_resets
                     << "] simbolo=" << total_simbolos_processados
                     << " nos_trie_antes=" << conta_nos_trie()
                     << endl;
            }
            executa_reset();
        }
    }

    // Conta o número total de nós atualmente na trie (DFS simples).
    // Usado só para logging/diagnóstico dos eventos de adaptação -- não é
    // chamado no caminho principal de codificação, então seu custo não
    // afeta o desempenho de compressão fora do log_eventos_adaptacao.
    uint64_t conta_nos_trie(){
        return conta_nos_recursivo(arvore.raiz);
    }
    uint64_t conta_nos_recursivo(No* no){
        if(!no) return 0;
        uint64_t total = 1;
        for(auto& [_, filho] : no->filhos){
            total += conta_nos_recursivo(filho);
        }
        return total;
    }

    // Imprime um resumo final dos eventos de adaptação ocorridos durante
    // toda a compressão. Pensado para ser chamado uma vez, no fim do
    // processamento (em main.cpp), independente de log_eventos_adaptacao
    // estar ativo ou não -- ou seja, o sumário sempre fica disponível,
    // mesmo que o log detalhado por evento tenha sido desligado para não
    // poluir a saída em corpus grandes.
    void imprime_resumo_adaptacao(){
        cout << "\n=== RESUMO DE ADAPTAÇÃO (poda/reset) ===" << endl;
        cout << "Modo de adaptação configurado (adapta=" << adapta << "): ";
        if(adapta == 0) cout << "nenhum (sem poda, sem reset)";
        else if(adapta == 1) cout << "poda por envelhecimento";
        else if(adapta == 2) cout << "reset total";
        cout << endl;
        cout << "Total de PODAS disparadas: " << total_podas << endl;
        cout << "Total de RESETS disparados: " << total_resets << endl;
        if(total_simbolos_processados > 0 && (total_podas + total_resets) > 0){
            double simbolos_por_evento =
                (double)total_simbolos_processados / (double)(total_podas + total_resets);
            cout << "Média de símbolos processados entre eventos: "
                 << simbolos_por_evento << endl;
        }
        cout << "==========================================\n" << endl;
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

    // Decodifica um símbolo do arquivo comprimido.
    //
    // Espelha exatamente a lógica de processa_simbolo(): percorre a trie
    // do maior contexto para o menor, injetando ESCAPE temporariamente
    // antes de cada decode_byte() — assim o decodificador vê a mesma
    // distribuição de probabilidade que o codificador usou.
    //
    // Bug corrigido: a versão anterior chamava decode_byte() duas vezes
    // por nível (uma para tentar o símbolo, outra para "confirmar" o
    // ESCAPE), consumindo o dobro de bits e dessincronizando o fluxo.
    // Agora há UMA ÚNICA chamada por nível: se retornar ESCAPE, subimos;
    // se retornar qualquer outro valor, é o símbolo decodificado.

    
    uint8_t decodifica_simbolo(ifstream& arquivo_bits) {
        uint32_t simbolo_decodificado = ESCAPE;
        double l0 = calcula_comprimento_medio();
        
        // Começa do maior contexto possível, igual ao codificador
        No* contexto = arvore.busca_contexto_byte(janela_atual);
        No* contexto_inicial = contexto;

        // Percorre subindo na árvore até encontrar o símbolo ou esgotar contextos
        while(contexto) {
            // Injeta ESCAPE temporariamente (exatamente como no encoder)
            uint32_t freq_esc = calcula_escape(contexto);
            contexto->frequencias[ESCAPE] = freq_esc;
            contexto->total += freq_esc;

            // Tenta decodificar o próximo símbolo/escape neste contexto
            uint32_t resultado = aritmetico.decode_byte(contexto, excluidos, arquivo_bits);

            // Desfaz a injeção temporária
            contexto->frequencias[ESCAPE] = 0;
            contexto->total -= freq_esc;

            // Se o resultado não for ESCAPE, encontramos o símbolo decodificado com sucesso
            if(resultado != ESCAPE) {
                simbolo_decodificado = resultado;
                if(equiprovaveis->frequencias[resultado] > 0){
                    equiprovaveis->frequencias[resultado]--;
                    equiprovaveis->total--;
                }
                break;
            }

            // Se decodificou ESCAPE → insere os símbolos deste contexto na lista de exclusão e sobe
            insere_em_excluidos(contexto);
            contexto = contexto->pai;
        }

        // Se nenhum contexto superior decodificou o símbolo, decodifica com a tabela de equiprováveis
        if(simbolo_decodificado == ESCAPE) {
            simbolo_decodificado = aritmetico.decode_byte(equiprovaveis, excluidos, arquivo_bits);
            if(simbolo_decodificado != ESCAPE && equiprovaveis->frequencias[simbolo_decodificado] > 0) {
                equiprovaveis->frequencias[simbolo_decodificado]--;
                equiprovaveis->total--;
            }
        }

        // Atualizações de estatísticas e contextos (idêntico ao encoder)
        arvore.atualiza_frequencia(contexto_inicial, (uint8_t)simbolo_decodificado);
        atualiza_contexto((uint8_t)simbolo_decodificado);
        total_simbolos_processados++;

        double lf = calcula_comprimento_medio();
        if(lf > (1+p)*l0){
            comprimento_emitido = calcula_comprimento_medio();
            sinaliza_e_aplica_adaptacao();
        }

        excluidos.clear();
        return (uint8_t)simbolo_decodificado;
    }

};