# Sobre

Trabalho da disciplina de Sistemas Operacionais.
Criar um Sistema Operacional simulado para a arquitetura vista na disciplina de Arquitetura de Computadores.

---

## Sobre a arquitetura

Consultar no endereço do Assembler:
https://github.com/ehmcruz/arq-sim-assembler

---

## Dependências

Depende das seguintes bibliotecas:

- NCurses
- My-lib (https://github.com/ehmcruz/my-lib). O Makefile está configurado para buscar o projeto **my-lib** no mesmo diretório pai que este projeto.

---

# Guia no Linux (Ubuntu)

## Compilando no Linux

Pacotes:
- libncurses-dev

**make CONFIG_TARGET_LINUX=1**

## Rodando no Linux

**./arq-sim-so**

---

# Guia no Windows

## Compilando no Windows (usando MSYS2)

Pacotes:
- mingw-w64-ucrt-x86_64-ncurses

**make CONFIG_TARGET_WINDOWS=1**

## Rodando no Windows

Considerando o terminal do MSYS2:

**unset TERM**    
**./arq-sim-so.exe**



---

# Implementação do sistema operacional

O sistema operacional implementa o gerenciamento de
processos, memória virtual paginada e escalonamento
preemptivo utilizando o algoritmo Round Robin.

## Gerenciamento de processos

Os processos são armazenados em uma std::list<Process>.

Cada processo possui um PCB contendo:

- PID;
- Nome do programa;
- Estado do processo;
- Tamanho do programa em palavras;
- Registradores;
- Contador de programa (PC);
- Tabela de páginas;
- Identificação do processo idle;
- Instante inicial e duração do sleep.

O processo em execução e o processo idle são identificados
por iteradores da lista.

A utilização de std::list permite inserir e remover
processos sem invalidar os iteradores, ponteiros e
referências dos demais elementos.

## Gerenciamento de memória

A memória física é dividida em quadros de 16 palavras.

Cada processo possui sua própria tabela de páginas,
responsável pelo mapeamento entre páginas virtuais
e quadros físicos.

Durante o carregamento de um programa, são selecionados
quadros físicos livres para armazenar suas instruções
e dados.

O modo Paging é configurado uma única vez durante o boot.

Durante as trocas de contexto, somente a tabela de páginas
do processo selecionado é configurada na CPU.

## Escalonamento

O escalonador utiliza o algoritmo Round Robin.

A cada interrupção do timer, o sistema operacional:

1. Verifica os processos bloqueados por sleep.
2. Salva o contexto do processo em execução, quando necessário.
3. Procura o próximo processo no estado Ready.
4. Configura a tabela de páginas do processo selecionado.
5. Restaura os registradores e o contador de programa.

Quando não existem processos de usuário prontos,
o processo idle é selecionado.

## Estados dos processos

- Ready: processo pronto para executar.
- Running: processo em execução.
- Blocked: processo aguardando o término do sleep.
- Terminated: processo em encerramento.

Os processos terminados têm seus quadros físicos
liberados e seus PCBs removidos da lista.

Quando o processo encerrado está utilizando a CPU,
a troca de contexto ocorre antes da remoção do PCB.

## Comandos do terminal

| Comando | Descrição |
| --- | --- |
| help | Exibe os comandos disponíveis. |
| ps | Lista os processos ativos. |
| load <bin> | Carrega um programa. |
| kill | Encerra o processo de usuário em execução. |
| kill <pid> | Encerra um processo pelo PID. |
| exit | Encerra o simulador. |

## Chamadas de sistema (syscalls)

| Código | Descrição |
| --- | --- |
| 0 | Encerra o processo. |
| 1 | Imprime uma string. |
| 2 | Imprime uma quebra de linha. |
| 3 | Imprime um número inteiro. |
| 6 | Bloqueia o processo pelo tempo solicitado. |

## Carregamento de programas

O carregamento de processos de usuário utiliza o
dispositivo de disco disponibilizado pela arquitetura.

O carregamento é realizado de forma assíncrona,
permitindo que outros processos continuem executando
enquanto o disco realiza a operação de leitura.

O sistema permite apenas uma solicitação de carregamento
por vez. Caso o disco esteja ocupado, novas solicitações
são recusadas.

### Etapas do carregamento

1. O usuário executa o comando load.
2. O sistema verifica se o disco está disponível.
3. O sistema envia o nome do arquivo ao disco.
4. O arquivo é aberto e a leitura é solicitada.
5. O disco gera uma interrupção quando os dados
   estão disponíveis para transferência.
6. O sistema recebe os bytes pela porta DiskData.
7. Caso necessário, uma nova leitura é solicitada.
8. Após a leitura completa, o arquivo é fechado.
9. Os bytes são convertidos em palavras de 16 bits.
10. O processo é criado e carregado na memória.

### Controle das solicitações

As informações do carregamento pendente são armazenadas
na estrutura PendingLoad, contendo:

- Indicador de solicitação ativa;
- Nome do arquivo binário;
- Buffer de bytes recebidos.

Não existe uma fila de solicitações, pois somente um
arquivo pode ser carregado por vez.

Cada solicitação de leitura utiliza blocos de até
65535 bytes, respeitando o limite de comunicação
das portas de 16 bits.

### Tratamento de erros

O carregamento trata as seguintes situações:

- Disco ocupado;
- Arquivo inexistente;
- Tamanho inválido do arquivo;
- Falta de quadros físicos disponíveis;
- Erros reportados pelo dispositivo de disco.

### Inicialização do idle

O processo idle continua sendo carregado durante o boot
pela função Lib::load_from_disk_to_16bit_buffer(),
antes do início dos ciclos de execução dos dispositivos.

## Organização do código

| Arquivo | Responsabilidade |
| --- | --- |
| os/os.cpp | Boot e tratamento de interrupções. |
| os/process.cpp | Gerenciamento do ciclo de vida dos processos. |
| os/process.h | Definição do PCB e da lista de processos. |
| os/scheduler.cpp | Escalonamento e troca de contexto. |
| os/memory.cpp | Gerenciamento da memória física. |
| os/syscall.cpp | Tratamento das chamadas de sistema. |
| os/terminal.cpp | Interpretação dos comandos do terminal. |
