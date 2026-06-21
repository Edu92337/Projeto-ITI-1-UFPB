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

    Ppm(int k, int tamanho,int adaptacao,float per = 0.1f, bool log_eventos = false){
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

    // Frequência fixa atribuída aos marcadores de adaptação (SYM_RESET/SYM_PODA)
    // dentro do contexto equiprovaveis. É um valor pequeno e CONSTANTE -- não
    // decai, não decrementa, sempre presente -- porque o encoder e o decoder
    // precisam concordar sem ambiguidade sobre a distribuição usada para
    // codificar/decodificar esse evento raro. Usamos 1, igual aos símbolos de
    // dado em equiprovaveis, para manter a tabela simétrica e simples.
    static const uint32_t FREQ_MARCADOR_ADAPTACAO = 1;

    void inicia_equiprovaveis(){
        for(int i = 0;i<256;i++)equiprovaveis -> frequencias[i]=1;
        // Mantém SYM_RESET e SYM_PODA sempre presentes neste contexto, com
        // frequência fixa, para que possam ser codificados/decodificados a
        // qualquer momento sem precisar de injeção/remoção temporária (ao
        // contrário do ESCAPE, que é injetado só na hora do uso).
        equiprovaveis->frequencias[SYM_RESET] = FREQ_MARCADOR_ADAPTACAO;
        equiprovaveis->frequencias[SYM_PODA]  = FREQ_MARCADOR_ADAPTACAO;
        equiprovaveis->total = 256 + 2 * FREQ_MARCADOR_ADAPTACAO;
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
            sinaliza_e_aplica_adaptacao();
        }
    }

    // Emite explicitamente, no stream de bits, um símbolo reservado
    // (SYM_PODA ou SYM_RESET) indicando qual mecanismo de adaptação foi
    // disparado, e só então aplica esse mecanismo ao modelo local.
    //
    // Por quê emitir o símbolo ANTES de aplicar a adaptação: o decoder,
    // simetricamente, precisa decodificar esse marcador usando o modelo
    // de equiprovaveis tal como ele estava ANTES da poda/reset (já que é
    // exatamente esse estado que o encoder usou para codificar o
    // marcador). Se a adaptação fosse aplicada primeiro, o contexto
    // equiprovaveis usado para codificar/decodificar o marcador poderia
    // já ter sido alterado (no caso do reset, ele É reinicializado dentro
    // de executa_reset), quebrando a simetria.
    //
    // A condição "lf > (1+p)*l0" que leva até aqui é determinística e
    // calculada a partir de informação que o decoder também possui no
    // mesmo instante (bits consumidos pelo símbolo recém-decodificado),
    // então o decoder sabe, sem nenhum bit adicional, QUANDO esperar um
    // marcador. O símbolo em si (PODA vs RESET) é que precisa ser
    // codificado, pois é uma decisão de configuração (parâmetro `adapta`)
    // que os dados sozinhos não permitem deduzir.
    void sinaliza_e_aplica_adaptacao(){
        // excluidos já foi limpo antes desta chamada (linha acima, no fim
        // do processamento do símbolo de dado) -- o marcador nunca deve
        // ser afetado por exclusões herdadas do símbolo anterior.
        if(adapta == 1){
            bool ok = aritmetico.encode_byte(SYM_PODA, equiprovaveis, excluidos);
            (void)ok; // a frequência do marcador é fixa e nunca zero: encode_byte sempre deve suceder aqui
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
            bool ok = aritmetico.encode_byte(SYM_RESET, equiprovaveis, excluidos);
            (void)ok;
            total_resets++;
            if(log_eventos_adaptacao){
                cout << "[RESET #" << total_resets
                     << "] simbolo=" << total_simbolos_processados
                     << " nos_trie_antes=" << conta_nos_trie()
                     << endl;
            }
            executa_reset();
        }
        // adapta == 0: sem reset nem poda, nenhum marcador é emitido
        // (o decoder, com adapta==0, também nunca vai checar por marcador,
        // pois a condição de disparo nem é avaliada nesse modo -- ver
        // observação na função de decodificação correspondente).
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

};