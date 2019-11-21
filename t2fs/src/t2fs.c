
/**
*/
#include "../include/t2fs.h"
#include "../include/apidisk.h"
#include "../include/t2disk.h"
#include "../include/bitmap2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define TAM_SUPERBLOCO 1 // Tamanho do superbloco em blocos
#define MAX_OPEN_FILE 10
#define TAM_SETOR 256    // Tamanho de um setor em bytes

#define DEBUG_MODE 1

typedef struct file_t2fs
{
    char filename[MAX_FILE_NAME_SIZE+1];
    int current_pointer;
} FILE_T2FS;

typedef struct linked_list
{
    struct t2fs_record registro;
    struct linked_list* next;
} Linked_List;

/// VARIAVEIS GLOBAIS
struct t2fs_superbloco superbloco_montado; // Variavel que guarda as informacoes do superbloco da particao montada
boolean tem_particao_montada = false; // Indica se tem alguma particao ja montada
FILE_T2FS open_files[MAX_OPEN_FILE] = {}; // Tabela de arquivos abertos (Maximo 10 por vez)
Linked_List* arquivos_diretorio = NULL; // Lista de registros do diretorio
Linked_List* current_dentry = NULL;
boolean diretorio_aberto = false;
int base = 0;   // Endereco de inicio da particao montada


/// FUNCOES AUXILIARES

/// Funcoes Linked-List
/*-----------------------------------------------------------------------------
Inicialização de lista encadeada
-----------------------------------------------------------------------------*/
Linked_List* create_linked_list()
{
    return NULL;
}

/*-----------------------------------------------------------------------------
Insercao de elemento ao final da lista encadeada
-----------------------------------------------------------------------------*/
Linked_List* insert_element(Linked_List* list, struct t2fs_record registro_)
{
    Linked_List* new_node = (Linked_List*)malloc(sizeof(Linked_List));
    new_node->registro = registro_;
    new_node->next = NULL;

    //printf("++ Insercao: %s -- %p\n", registro_.name, new_node);

    if (list == NULL)
    {
        return new_node;
    }

    Linked_List* aux = list;
    while (aux->next != NULL)
    {
        aux = aux->next;
    }
    aux->next = new_node;

    return list;
}

/*-----------------------------------------------------------------------------
Coleta de elemento da lista - int indica se ocorreu certo
    0 - okay
   -1 - erro
-----------------------------------------------------------------------------*/
int get_element(Linked_List* list, char* nome_registro_, struct t2fs_record* registro)
{
    Linked_List* aux = list;
    while (aux!=NULL)
    {
        if(strcmp(aux->registro.name, nome_registro_) == 0)
        {
            *registro = aux->registro;
            return 0;
        }
        aux = aux->next;
    }

    return -1;
}

/*-----------------------------------------------------------------------------
Deleta um elemento da lista encadeada
-----------------------------------------------------------------------------*/
Linked_List* delete_element(Linked_List* list, char* nome_registro_)
{
    //printf("+-+ Pedido para deletar: %s\n", nome_registro_);
    Linked_List* aux = NULL;
    Linked_List* freed_node = list;

    if (list == NULL)
    {
        return NULL;
    }

    if (strcmp(list->registro.name, nome_registro_) == 0)
    {
        aux = list->next;
        //printf("1++ Deleta: %s -- %p\n", list->registro.name, list);
        free(list);
        list = NULL;
        return aux;
    }

    while(freed_node->next!=NULL && strcmp(freed_node->registro.name, nome_registro_) != 0 )
    {
        aux = freed_node;
        freed_node = freed_node->next;
    }
    if (strcmp(freed_node->registro.name, nome_registro_) == 0)
    {
        aux->next = freed_node->next;
        //printf("2++ Deleta: %s\n", freed_node->registro.name);
        //free(freed_node);
    }
    return list;
}

/*-----------------------------------------------------------------------------
Verificacao se existe um dado elemento (nome) no diretorio
-----------------------------------------------------------------------------*/
boolean contains(Linked_List* list, char* nome_registro_)
{
    Linked_List* aux = list;
    while (aux!=NULL)
    {
        printf("arq1: %s X arq2: %s == %d\n", aux->registro.name, nome_registro_, strcmp(aux->registro.name, nome_registro_));
        if(strcmp(aux->registro.name, nome_registro_) == 0)
        {
            return true;
        }
        aux = aux->next;
    }
    return false;
}

/*-----------------------------------------------------------------------------
Le do buffer, a partir do endereço especificado, a quantidade de bytes informada.
Retorna um int com os valores do buffer, considerando que os dados estao em little-endian
-----------------------------------------------------------------------------*/
int getDado(unsigned char buffer[], int end_inicio, int qtd)
{
    int i;
    int dado = buffer[end_inicio+qtd-1]; // Dados em little-endian

    for(i=qtd-2; i>=0; i--)
    {
        dado <<= 8;
        dado |= buffer[end_inicio+i];
    }

    return dado;
}

/*-----------------------------------------------------------------------------
Calculo do checksum de um superbloco
-----------------------------------------------------------------------------*/
unsigned int checksum(struct t2fs_superbloco *superbloco)
{
    unsigned int bytes_iniciais[5];
    memcpy(bytes_iniciais, superbloco, 20); // 20 = numero de bytes a serem copiados
    unsigned int checksum = 0;
    int i=0;
    for(i=0; i<5; i++)
    {
        checksum += bytes_iniciais[i];
    }
    checksum = ~checksum; // Complemento de 1
    return checksum;
}

/*-----------------------------------------------------------------------------
Procuna nos arquivos abertos se há um arquivo de nome 'filename'
    Retorno:
        - True: caso arquivo encontrado
        - False: caso contrario
-----------------------------------------------------------------------------*/
boolean open_files_contem_arquivo(char* filename)
{
    int i = 0;
    for (i=0; i<MAX_OPEN_FILE; i++)
    {
        if (open_files[i].current_pointer != -1 && strcmp(open_files[i].filename, filename) == 0)
        {
            return true;
        }
    }
    return false;
}

/*-----------------------------------------------------------------------------
Função:	Verifica se tem um bloco livre e, caso sim, limpa-o e o marca no bitmap de blocos
como ocupado. Retorno: -1 caso erro, 0 caso nao houver um bloco livre, ou o indice do bloco
alocado.
-----------------------------------------------------------------------------*/
int aloca_bloco()
{
    int bloco;
    int i;
    if((bloco = searchBitmap2(BITMAP_DADOS, 0)) < 0)   // Se retorno da funcao for negativo, deu erro
    {
        return bloco;
    }

    int setor_inicio = bloco * superbloco_montado.blockSize; // Setor de inicio do bloco

    // Limpa bloco
    unsigned char buffer[TAM_SETOR] = {0};
    for(i=0; i<superbloco_montado.blockSize; i++)
    {
        write_sector(base + setor_inicio + i, buffer);
    }

    return bloco;
}


/// FUNCOES DA BIBLIOTECA
/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size)
{
    char alunos[] = "Guilherme B. B. O. Malta - 00278237\nGuilherme T. Bazzo - 00287687\nNicolas C. Duranti - 00287679\n";
    // Caso o nome dos alunos (com seus respectivos numeros de matricula) nao caibam no espaco dado, retorna erro
    if(strlen(alunos) >= size)
    {
        return -1;
    }
    strcpy(name, alunos);
    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Formata logicamente uma partição do disco virtual t2fs_disk.dat para o sistema de
		arquivos T2FS definido usando blocos de dados de tamanho
		corresponde a um múltiplo de setores dados por sectors_per_block.
-----------------------------------------------------------------------------*/
int format2(int partition, int sectors_per_block)
{
    if (DEBUG_MODE){printf("> FORMAT\n");}

    unsigned char buffer[TAM_SETOR] = {0};
    int i;

    if(read_sector(0, buffer))  // Le o MBR, localizado no primeiro setor
    {
        if (DEBUG_MODE){printf("**Erro ao ler setor**\n");}
        return -1;
    }

    int num_particoes = getDado(buffer, 6, 2);
    if(partition < 0 || partition >= num_particoes)  // Verifica se a particao é valida (entre 0 e num-1)
    {
        if (DEBUG_MODE){printf("**Erro com validez de particao**\n");}
        return -1;
    }

    int tam_setor = getDado(buffer, 2, 2);
    int setor_inicio = getDado(buffer, 8 + 32*partition, 4);
    int setor_fim = getDado(buffer, 12 + 32*partition, 4);
    int num_setores = setor_fim - setor_inicio + 1;
    int num_blocos = num_setores / sectors_per_block;
    int num_blocos_inodes = ceil(0.1 * num_blocos);

    int tam_bloco = sectors_per_block * tam_setor; // Em bytes
    int num_inodes = num_blocos_inodes * tam_bloco / sizeof(struct t2fs_inode);
    int num_blocos_bitmap_inode = ceil((float)num_inodes / tam_bloco);
    int num_blocos_bitmap_blocos = ceil((float)num_blocos / tam_bloco);

    struct t2fs_superbloco superbloco;
    char id[4] = "T2FS";
    strcpy(superbloco.id, id);
    superbloco.version = 0x7E32;
    superbloco.superblockSize = 1;
    superbloco.freeBlocksBitmapSize = num_blocos_bitmap_blocos;	// Número de blocos do bitmap de blocos de dados
    superbloco.freeInodeBitmapSize = num_blocos_bitmap_inode;	// Número de blocos do bitmap de i-nodes
    superbloco.inodeAreaSize = num_blocos_inodes;  // Número de blocos reservados para os i-nodes
    superbloco.blockSize = sectors_per_block;      // Número de setores que formam um bloco
    superbloco.diskSize = num_blocos;              // Número total de blocos da partição
    superbloco.Checksum = checksum(&superbloco);   // Soma dos 5 primeiros inteiros de 32 bits do superbloco, complementado de 1


    if(write_sector(setor_inicio, (unsigned char *)&superbloco))
    {
        if (DEBUG_MODE){printf("**Erro escrita de setor**\n");}
        return -1;
    }



    /// Inicializacao dos Bitmaps
    if(openBitmap2(setor_inicio))
    {
        if (DEBUG_MODE){printf("**Erro abertura bitmap**\n");}
        return -1;
    }

    // Zera o bitmap de inodes
    do
    {
        if((i = searchBitmap2(BITMAP_INODE, 1)) < -1)  // Se retorno da funcao for menor que -1, deu erro (-1 == não encontrado ????)
        {
            if (DEBUG_MODE){printf("**Erro bitmap -- invalido 0**\n");}
            return -1;
        }
        if(i != -1)   // Se i for positivo, achou bit com valor 1
        {
            if(setBitmap2(BITMAP_INODE, i, 0))
            {
                if (DEBUG_MODE){printf("**Erro ao setar bitmap**\n");}
                return -1;
            }
        }
    }
    while(i != -1);


    // Zera o bitmap de dados
    do
    {
        if((i = searchBitmap2(BITMAP_DADOS, 1)) < -1)  // Se retorno da funcao for menor que -1, deu erro (-1 == não encontrado ????)
        {
            if (DEBUG_MODE){printf("**Erro bitmap -- invalido 1\n");}
            return -1;
        }
        if(i != -1)   // Se i for positivo, achou bit com valor 1
        {
            if(setBitmap2(BITMAP_DADOS, i, 0))
            {
                if (DEBUG_MODE){printf("**Erro ao setar bitmap**\n");}
                return -1;
            }
        }
    }
    while(i != -1);

    // Marca os blocos onde estao o superbloco, os bitmaps e os blocos de inodes como ocupados
    for(i=0; i < TAM_SUPERBLOCO + num_blocos_bitmap_blocos + num_blocos_bitmap_inode + num_blocos_inodes; i++)
    {
        if(setBitmap2(BITMAP_DADOS, i, 1))
        {
            if (DEBUG_MODE){printf("**Erro ao setar bitmap**\n");}
            return -1;
        }
    }

    /// Inicializacao do bloco de dados do diretorio raiz
    int bloco_raiz = aloca_bloco();
    if(bloco_raiz <= -1)  // Se erro ou se nao ha um bloco livre, retorna erro
    {
        if (DEBUG_MODE){printf("**Bloco invalido para raiz**\n");}
        return -1;
    }

    /// Inicializacao do i-node do diretorio raiz (i-node 0)
    struct t2fs_inode inode_raiz;
    inode_raiz.blocksFileSize = 1;
    inode_raiz.bytesFileSize = 0;
    inode_raiz.dataPtr[0] = bloco_raiz;
    inode_raiz.dataPtr[1] = -1;
    inode_raiz.singleIndPtr = -1;
    inode_raiz.doubleIndPtr = -1;
    inode_raiz.RefCounter = 1;

    // Bloco de inicio da area de inodes
    int inicio_area_inodes = TAM_SUPERBLOCO + num_blocos_bitmap_blocos + num_blocos_bitmap_inode;
    int setor_inicio_area_inodes = inicio_area_inodes * sectors_per_block;
    if(write_sector(setor_inicio + setor_inicio_area_inodes, (unsigned char *)&inode_raiz))
    {
        return -1;
    }

    if(setBitmap2(BITMAP_INODE, 0, 1))  // Marca o bit do i-node como ocupado
    {
        return -1;
    }

    if(closeBitmap2())
    {
        if (DEBUG_MODE){printf("**Erro ao fechar bitmap**\n");}
        return -1;
    }

    if (DEBUG_MODE){printf("> FORMAT end\n");}
    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Monta a partição indicada por "partition" no diretório raiz
-----------------------------------------------------------------------------*/
int mount(int partition)
{
    if (DEBUG_MODE){printf("> MOUNT\n");}

    unsigned char buffer[256];
    int i;

    if(tem_particao_montada)
    {
        return -1;
    }

    if(read_sector(0, buffer))  // Le o MBR, localizado no primeiro setor
    {
        return -1;
    }

    int num_particoes = getDado(buffer, 6, 2);
    if(partition < 0 || partition >= num_particoes)  // Verifica se a particao é valida (entre 0 e num-1)
    {
        return -1;
    }

    int setor_inicio = getDado(buffer, 8 + 24*partition, 4);
    if(read_sector(setor_inicio, buffer))  // Le o superbloco da particao
    {
        return -1;
    }
    memcpy(&superbloco_montado, buffer, sizeof(struct t2fs_superbloco));

    // Verifica se o superbloco foi formatado
    if(!(checksum(&superbloco_montado) == superbloco_montado.Checksum))
    {
        return -1;
    }

    tem_particao_montada = true;
    base = setor_inicio;

    for(i=0; i<MAX_OPEN_FILE; i++)  // Inicializa/Limpa a tabela de arquivos abertos
    {
        open_files[i].current_pointer = -1;
    }

    if(openBitmap2(setor_inicio))   // Abre os bitmaps da particao montada
    {
        return -1;
    }

    if (DEBUG_MODE){printf("> MOUNT end\n");}
    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Desmonta a partição atualmente montada, liberando o ponto de montagem.
-----------------------------------------------------------------------------*/
int umount(void)
{

    if(closeBitmap2())  // Fecha os bitmaps da particao
    {
        return -1;
    }
    tem_particao_montada = false;
    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo arquivo no disco e abrí-lo,
		sendo, nesse último aspecto, equivalente a função open2.
		No entanto, diferentemente da open2, se filename referenciar um
		arquivo já existente, o mesmo terá seu conteúdo removido e
		assumirá um tamanho de zero bytes.
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename)
{
    if (DEBUG_MODE){printf("> CREATE\n");}

    if(!tem_particao_montada)
    {
        return -1;
    }

    if (!filename)  // Caso filename seja NULL retorna -1
    {
        return -1;
    }
    if (filename[0] == '\0')  // Caso filename seja um nome vazio retorna -1
    {
        return -1;
    }

    if(opendir2()){
        if (DEBUG_MODE){printf("**Erro abrir dir - inicio**\n");}
        return -1;
    }

    // Procurar se há algum arquivo no disco com mesmo nome.
    // Caso ja existir, remove o conteudo, assumindo tamanho de zero bytes. (Deleta o arquivo e continua a criacao)
    if(contains(arquivos_diretorio, filename))
    {
        if(closedir2()){
            if (DEBUG_MODE){printf("*Erro fechar dir - ja contem arquivo**\n");}
            return -1;
        }
        if(delete2(filename) != 0)
        {
            if (DEBUG_MODE){printf("**Erro deletar arquivo**\n");}
            return -1;
        }
    }

    if(diretorio_aberto){
        if(closedir2()){
            if (DEBUG_MODE){printf("**Erro fechar dir - inicio**\n");}
            return -1;
        }
    }

    /// T2FS Record
    struct t2fs_record created_file_record;
    created_file_record.TypeVal = TYPEVAL_REGULAR; // Registro regular
    if (strlen(filename) > 50)  // Caso o nome passado como parametro seja maior que o tamanho máximo
    {
        if (DEBUG_MODE){printf("**Erro nome invalido**\n");}
        return -1;              //   do campo `name` para registro.
    }
    strcpy(created_file_record.name, filename);


    // Selecao de um bloco de dados para o arquivo
    int indice_bloco_dados = aloca_bloco();
    if(indice_bloco_dados <= -1)     // Se retorno da funcao for negativo, deu erro
    {
        if (DEBUG_MODE){printf("**Erro indice de dados alocado invalido**\n");}
        return -1;              // Se for 0, nao ha blocos livres
    }

    // Selecao de um i-node para o arquivo
    int indice_inode;
    if((indice_inode = searchBitmap2(BITMAP_INODE, 0)) < 0)  // Se retorno da funcao for negativo, deu erro
    {
        if (DEBUG_MODE){printf("**Erro ao procurar bitmap para arquivo criado**\n");}
        return -1;
    }

    /// inicializacao i-node
    struct t2fs_inode new_inode;
    new_inode.blocksFileSize = 1;
    new_inode.bytesFileSize = 0;
    new_inode.dataPtr[0] = indice_bloco_dados;
    new_inode.dataPtr[1] = -1;
    new_inode.singleIndPtr = -1;
    new_inode.doubleIndPtr = -1;
    new_inode.RefCounter = 1;

    created_file_record.inodeNumber = indice_inode;

    /// Escrita do i-node no disco
    int inicio_area_inodes = TAM_SUPERBLOCO + superbloco_montado.freeBlocksBitmapSize + superbloco_montado.freeInodeBitmapSize;
    int setor_inicio_area_inodes = inicio_area_inodes * superbloco_montado.blockSize;
    int inodes_por_setor = TAM_SETOR / sizeof(struct t2fs_inode);
    int setor_novo_inode = setor_inicio_area_inodes + (indice_inode / inodes_por_setor);
    int end_novo_inode = indice_inode % inodes_por_setor;

    unsigned char buffer[TAM_SETOR];
    if(read_sector(base + setor_novo_inode, buffer))
    {
        if (DEBUG_MODE){printf("**Erro leitura disco**\n");}
        return -1;
    }

    memcpy(&buffer[end_novo_inode*sizeof(struct t2fs_inode)], &new_inode, sizeof(struct t2fs_inode));
    if(write_sector(base + setor_novo_inode, buffer))
    {
        if (DEBUG_MODE){printf("**Erro escrita de inode para novo arquivo em disco**\n");}
        return -1;
    }

    if(setBitmap2(BITMAP_INODE, indice_inode, 1))
    {
        if (DEBUG_MODE){printf("**Erro ao setar bitmap novo arquivo - inode**\n");}
        return -1;
    }
    if(setBitmap2(BITMAP_DADOS, indice_bloco_dados, 1))
    {
        if (DEBUG_MODE){printf("**Erro ao setar bitmap novo arquivo - bloco de dados**\n");}
        setBitmap2(BITMAP_INODE, indice_inode, 0);
        return -1;
    }

    // Adiciona registro na lista de registros
    if(opendir2()){
        if (DEBUG_MODE){printf("**Erro opendir - fim**\n");}
        return -1;
    }

    arquivos_diretorio = insert_element(arquivos_diretorio, created_file_record);
    if(closedir2()){
        if (DEBUG_MODE){printf("**Erro closedir - fim**\n");}
        return -1;
    }

    if (DEBUG_MODE){printf("> CREATE end - chamando open\n");}
    return open2(filename);
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco.
-----------------------------------------------------------------------------*/
int delete2 (char *filename)
{
    if (DEBUG_MODE){printf("> DELETE\n");}
    if(!tem_particao_montada)
    {
        return -1;
    }

    if(open_files_contem_arquivo(filename))  // Caso o arquivo esteja aberto, retorna erro
    {
        if (DEBUG_MODE){printf("**Erro arquivo aberto ao deletar**\n");}
        return -1;
    }

    if(opendir2())
    {
        if (DEBUG_MODE){printf("**Erro opendir - inicio**\n");}
        return -1;
    }

    if (!contains(arquivos_diretorio, filename))  // Caso não haja o arquivo no diretorio, retorna erro
    {
        if (DEBUG_MODE){printf("**Erro arquivo nao em diretorio**\n");}
        closedir2();
        return -1;
    }


    struct t2fs_record registro;
    if(get_element(arquivos_diretorio, filename, &registro))
    {
        if (DEBUG_MODE){printf("**Erro ao pegar arquivo de lista de arquivos do dir**\n");}
        closedir2();
        return -1;
    }
    printf("<< Elemento pego: %s\n", registro.name);
    if(closedir2()){
        if (DEBUG_MODE){printf("**Erro closedir - inicio**\n");}
        return -1;
    }

    // DESALOCA BLOCOS DE DADOS E I-NODE
    int i,j,k,l;
    unsigned char buffer[TAM_SETOR];
    unsigned char buffer2[TAM_SETOR];
    int indice_inode = registro.inodeNumber;
    struct t2fs_inode inode;

    int inicio_area_inodes = TAM_SUPERBLOCO + superbloco_montado.freeBlocksBitmapSize + superbloco_montado.freeInodeBitmapSize;
    int setor_inicio_area_inodes = inicio_area_inodes * superbloco_montado.blockSize;
    int inodes_por_setor = TAM_SETOR / sizeof(struct t2fs_inode);
    int setor_inode = setor_inicio_area_inodes + (indice_inode / inodes_por_setor);
    int end_inode = indice_inode % inodes_por_setor;

    if(read_sector(base + setor_inode, buffer))
    {
        if (DEBUG_MODE){printf("**Erro leitura setor 1**\n");}
        return -1;
    }
    memcpy(&inode, &buffer[end_inode*sizeof(struct t2fs_inode)], sizeof(struct t2fs_inode));

    // Se houver hardlinks, apenas decrementa o contator e deleta o registro
    if(inode.RefCounter > 1)
    {
        inode.RefCounter--;
        arquivos_diretorio = delete_element(arquivos_diretorio, filename);
        return 0;
    }

    int qtd_blocos = inode.blocksFileSize;
    int blocos_liberados = 0;

    for(i=0; i<2; i++)   // Indica blocos apontados diretamente como livres
    {
        if(inode.dataPtr[i] != -1)
        {
            if(setBitmap2(BITMAP_DADOS, inode.dataPtr[i], 0))
            {
                if (DEBUG_MODE){printf("**Erro ao setar bitmap 1**\n");}
                return -1;
            }
            blocos_liberados ++;
        }
    }

    int qtd_ponteiros_por_setor = TAM_SETOR / sizeof(DWORD);
    if(blocos_liberados != qtd_blocos)  // APRESENTA PONTEIRO DE INDIRECAO SIMPLES
    {
        int indice_bloco_indice = inode.singleIndPtr;
        int setor_inicio_bloco = indice_bloco_indice * superbloco_montado.blockSize; //Localizacao do bloco de ind simples

        i=0;
        // Para todo setor do bloco de ind simples
        while(i<superbloco_montado.blockSize && blocos_liberados != qtd_blocos)
        {
            if(read_sector(base + setor_inicio_bloco + i, buffer))  // Le o setor
            {
                if (DEBUG_MODE){printf("**Erro ao ler setor 2**\n");}
                return -1;
            }

            j=0;
            // Para todo ponteiro (para um bloco de dados) nesse setor
            while(j<qtd_ponteiros_por_setor && blocos_liberados != qtd_blocos)
            {
                DWORD ponteiro;
                memcpy(&ponteiro, &buffer[j*sizeof(DWORD)], sizeof(DWORD)); // Le o ponteiro
                if(setBitmap2(BITMAP_DADOS, ponteiro, 0))  // Libera o bloco de dados
                {
                    if (DEBUG_MODE){printf("**Erro setar bitmap 2**\n");}
                    return -1;
                }
                blocos_liberados++;
                j++;
            }
            i++;
        }
        if(setBitmap2(BITMAP_DADOS, indice_bloco_indice, 0))  // Libera o bloco de ind simples
        {
            if (DEBUG_MODE){printf("**Erro ao setar bitmap 3**\n");}
            return -1;
        }
    }

    if(blocos_liberados != qtd_blocos)   // APRESENTA BLOCOS DE INDIRECAO DUPLA
    {
        int bloco_indD = inode.doubleIndPtr;
        int setor_inicio_bloco_indD = bloco_indD * superbloco_montado.blockSize; // Localizacao do bloco de ind dupla

        k=0;
        // Para todo setor no bloco de ind dupla
        while(k<superbloco_montado.blockSize && blocos_liberados!=qtd_blocos)
        {
            if(read_sector(base + setor_inicio_bloco_indD + k, buffer))  // Le o setor
            {
                if (DEBUG_MODE){printf("**Erro ao ler setor 3**\n");}
                return -1;
            }
            l = 0;
            // Para todo ponteiro (que aponta para um bloco de ind simples) nesse setor
            while(i<qtd_ponteiros_por_setor && blocos_liberados!=qtd_blocos)
            {
                DWORD pt1;
                memcpy(&pt1, &buffer[sizeof(DWORD)*i], sizeof(DWORD)); // Le o ponteiro
                int setor_inicio_bloco = pt1 * superbloco_montado.blockSize; // Localizacao do bloco de ind simples

                i=0;
                // Para todo setor no bloco de ind simples
                while(i<superbloco_montado.blockSize && blocos_liberados != qtd_blocos)
                {
                    if(read_sector(base + setor_inicio_bloco + i, buffer2))  // Le o setor
                    {
                        if (DEBUG_MODE){printf("**Erro ao ler setor 4**\n");}
                        return -1;
                    }
                    j=0;
                    // Para todo ponteiro (para um bloco de dados) nesse setor
                    while(j<qtd_ponteiros_por_setor && blocos_liberados != qtd_blocos)
                    {
                        DWORD pt2;
                        memcpy(&pt2, &buffer2[j*sizeof(DWORD)], sizeof(DWORD)); // Le o ponteiro
                        if(setBitmap2(BITMAP_DADOS, pt2, 0))  // Libera o bloco de dados
                        {
                            if (DEBUG_MODE){printf("**Erro ao setar bitmap 4**\n");}
                            return -1;
                        }
                        blocos_liberados++;
                        j++;
                    }
                    i++;
                }
                if(setBitmap2(BITMAP_DADOS, pt1, 0))  // Libera o bloco de ind simples
                {
                    if (DEBUG_MODE){printf("**Erro ao setar bitmap 5**\n");}
                    return -1;
                }
                l++;
            }
            k++;
        }
        if(setBitmap2(BITMAP_DADOS, bloco_indD, 0))  // Libera o bloco de ind dupla
        {
            if (DEBUG_MODE){printf("**Erro ao setar bitmap 6**\n");}
            return -1;
        }
    }

    // Libera i-node
    if(setBitmap2(BITMAP_INODE, indice_inode, 0))
    {
        if (DEBUG_MODE){printf("**Erro ao setar bitmap 7**\n");}
        return -1;
    }


    if(opendir2()){
        if (DEBUG_MODE){printf("**Erro opendir - fim**\n");}
        return -1;
    }

    // Deleta o arquivo da lista de arquivos do diretorio
    arquivos_diretorio = delete_element(arquivos_diretorio, filename);

    if(closedir2())
    {
        if (DEBUG_MODE){printf("**Erro closedir - fim**\n");}
        return -1;
    }
    if (DEBUG_MODE){printf("> DELETE end\n");}
    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename)
{
    if (DEBUG_MODE){printf("> OPEN\n");}
    if(!tem_particao_montada)
    {
        return -1;
    }

    if(opendir2())
    {
        if (DEBUG_MODE){printf("**Erro opendir**\n");}
        return -1;
    }

    // Pega o registro do arquivo. Se o arquivo nao existe no diretorio, retorna -1
    struct t2fs_record registro;
    if(get_element(arquivos_diretorio, filename, &registro))
    {
        if (DEBUG_MODE){printf("**Erro arquivo nao existente em diretorio**\n");}
        closedir2();
        return -1;
    }

    // Procura posicao para inserir arquivo
    int pos_insercao_open_file = 0;
    while ((open_files[pos_insercao_open_file].current_pointer >= 0) && (pos_insercao_open_file < MAX_OPEN_FILE))
    {
        // Varre array de arquivos abertos para adicionar o novo arquivo a eles.
        pos_insercao_open_file++;
    }

    if (pos_insercao_open_file >= 10)  // Retorna -1 caso já haja 10 ou mais arquivos abertos
    {
        if (DEBUG_MODE){printf("-- nao ha mais espaco para abrir arquivos\n");}
        closedir2();
        return -1;                      // Isso significa que não há espaço disponível para abrir arquivo
    }


    // Verifica se arquivo é um soft link
    char nome_arq_referenciado[MAX_FILE_NAME_SIZE];
    if(registro.TypeVal == TYPEVAL_LINK)
    {

        unsigned char buffer[TAM_SETOR];

        // I-node do link
        int indice_inode = registro.inodeNumber;

        // Leitura do i-node no disco
        int inicio_area_inodes = TAM_SUPERBLOCO + superbloco_montado.freeBlocksBitmapSize + superbloco_montado.freeInodeBitmapSize;
        int setor_inicio_area_inodes = inicio_area_inodes * superbloco_montado.blockSize;
        int inodes_por_setor = TAM_SETOR / sizeof(struct t2fs_inode);
        int setor_inode = setor_inicio_area_inodes + (indice_inode / inodes_por_setor);
        int end_inode = indice_inode % inodes_por_setor;

        if(read_sector(base + setor_inode, buffer))
        {
            if (DEBUG_MODE){printf("**Erro leitura setor 1**\n");}
            closedir2();
            return -1;
        }

        struct t2fs_inode inode;
        memcpy(&inode, &buffer[end_inode*sizeof(struct t2fs_inode)], sizeof(struct t2fs_inode));

        // Bloco contendo o nome do arquivo referenciado
        int indice_bloco = inode.dataPtr[0];
        int setor_bloco = indice_bloco * superbloco_montado.blockSize;
        if(read_sector(base + setor_bloco, buffer))
        {
            if (DEBUG_MODE){printf("**Erro leitura setor 2**\n");}
            closedir2();
            return -1;
        }

        // Leitura do nome do arquivo referenciado
        memcpy(nome_arq_referenciado, buffer, MAX_FILE_NAME_SIZE);

        if(!contains(arquivos_diretorio, nome_arq_referenciado))
        {
            closedir2();
            return -1; // Arquivo referenciado inexistente
        }
    }

    if(closedir2())
    {
        if (DEBUG_MODE){printf("**Erro closedir**\n");}
        return -1;
    }

    /// File
    FILE_T2FS new_open_file;
    new_open_file.current_pointer = 0;
    if(registro.TypeVal == TYPEVAL_LINK)
    {
        strcpy(new_open_file.filename, nome_arq_referenciado);
    }
    else  // Arquivo regular
    {
        strcpy(new_open_file.filename, filename);
    }

    open_files[pos_insercao_open_file] = new_open_file; // Insere em arquivos abertos

    if (DEBUG_MODE){printf("> OPEN end\n");}
    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um arquivo.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle)
{
    if (DEBUG_MODE){printf("> CLOSE\n");}
    if(!tem_particao_montada)
    {
        return -1;
    }

    // handle: identificador de arquivo para fechar
    if (handle < 0 || handle > MAX_OPEN_FILE){
        if (DEBUG_MODE){printf("**Erro handle invalido**\n");}
        return -1;
    }

    // Caso arquivo já esteja marcado como fechado (current_pointer == -1)
    if (open_files[handle].current_pointer < 0)  // (<= 0)??
    {
        if (DEBUG_MODE){printf("**Erro arquivo ja fechado**\n");}
        return -1;
    }

    open_files[handle].current_pointer = -1; // Marca arquivo como fechado (current_pointer == -1)

    if (DEBUG_MODE){printf("> CLOSE\n");}
    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a leitura de uma certa quantidade
		de bytes (size) de um arquivo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size)
{
    if (DEBUG_MODE){printf("> READ\n");}
    // Se particao nao foi montada, se a quantidade de bytes para leitura for negativa ou se
    // o arquivo indicado por handle na open_files table tiver um current_pointer invalido,
    // retorna erro.
    if(!tem_particao_montada || size < 0 || open_files[handle].current_pointer < 0)
    {
        return -1;
    }

    if(opendir2())
    {
        if (DEBUG_MODE){printf("**Erro opendir**\n");}
        return -1;
    }

    // Le registro do arquivo indicado por filename
    struct t2fs_record registro;
    if(get_element(arquivos_diretorio, open_files[handle].filename, &registro))
    {
        if (DEBUG_MODE){printf("**Erro arquivo**\n");}
        closedir2();
        return -1;
    }

    if(closedir2())
    {
        if (DEBUG_MODE){printf("**Erro closedir**\n");}
        return -1;
    }

    // Calcula o indice do inode do arquivo indicado por filename
    int indice_inode = registro.inodeNumber;
    int inicio_area_inodes = TAM_SUPERBLOCO + superbloco_montado.freeBlocksBitmapSize + superbloco_montado.freeInodeBitmapSize;
    int setor_inicio_area_inodes = inicio_area_inodes * superbloco_montado.blockSize;
    int inodes_por_setor = TAM_SETOR / sizeof(struct t2fs_inode);
    int setor_inode = setor_inicio_area_inodes + (indice_inode / inodes_por_setor);
    int end_inode = indice_inode % inodes_por_setor;

    int qtd_ponteiros_por_setor = TAM_SETOR / sizeof(DWORD);

    // Le inode do arquivo indicado por filename
    struct t2fs_inode inode;
    unsigned char buffer_inode[TAM_SETOR];
    unsigned char buffer_setor[TAM_SETOR];
    if(read_sector(base + setor_inode, buffer_inode))
    {
        if (DEBUG_MODE){printf("**Erro leitura setor 1**\n");}
        return -1;
    }
    memcpy(&inode, &buffer_inode[end_inode*sizeof(struct t2fs_inode)], sizeof(struct t2fs_inode));

    unsigned int bytes_por_bloco = TAM_SETOR * superbloco_montado.blockSize;

    int sizeR = size;
    if(inode.bytesFileSize - open_files[handle].current_pointer < size)
    {
        sizeR = inode.bytesFileSize - open_files[handle].current_pointer;
    }

    // ind_byte: armazena o indice do byte atual para ser lido do setor (para qualquer um dos blocos apontados pelo inode)
    // valor_byte: variavel para armazenar byte lido do setor
    // bytes_read: total de bytes lidos do arquivo
    unsigned int ind_byte = open_files[handle].current_pointer;
    unsigned char valor_byte;
    unsigned int bytes_read = 0;
    int ind_setor_dados;

    /// Faz a leitura dos dados do primeiro ponteiro direto do inode
    if(open_files[handle].current_pointer < bytes_por_bloco && bytes_read < sizeR)
    {

        if(inode.dataPtr[0] != -1)
        {
            // Calcula setor de inicio do bloco de dados do ponteiro direto
            int indice_bloco_direto_a = inode.dataPtr[0];
            int setor_inicio_direto_a = indice_bloco_direto_a * superbloco_montado.blockSize;

            ind_setor_dados = floor(ind_byte/TAM_SETOR);

            while(ind_setor_dados < superbloco_montado.blockSize && bytes_read < sizeR && bytes_read < inode.bytesFileSize)
            {
                // Le o setor do disco para buffer_setor
                if(read_sector(base + setor_inicio_direto_a + ind_setor_dados, buffer_setor))  // Le o setor
                {
                    if (DEBUG_MODE){printf("**Erro leitura setor 2**\n");}
                    return -1;
                }
                ind_byte = ind_byte%TAM_SETOR;
                // Percorre o setor gravando byte a byte no buffer enquanto:
                // a) nao chegar ao final do setor
                // b) nao ultrapassar o tamanho maximo do arquivo
                // c) nao ultrapassar o request de bytes a serem lidos

                while(ind_byte < TAM_SETOR && open_files[handle].current_pointer < inode.bytesFileSize && bytes_read < sizeR)
                {
                    memcpy(&valor_byte, &buffer_setor[ind_byte], 1);
                    *(buffer + bytes_read) = valor_byte;
                    ind_byte++;
                    bytes_read++;
                    open_files[handle].current_pointer++;
                }
                ind_setor_dados++;

                ind_byte = 0; // Reseta indice do byte para entrar no proximo laco, caso se aplique
            }
        }
    }
    else
    {
        ind_byte -= bytes_por_bloco; // Caso nao entre no primeiro caso, subtrai tamanho do bloco em bytes da
        // variavel de byte atual para preparar para proximo condicional
    }

    /// Faz leitura do segundo bloco apontado por ponteiro direto do inode
    if(open_files[handle].current_pointer < 2*bytes_por_bloco && bytes_read < sizeR)
    {
        if(inode.dataPtr[1] != -1)
        {
            // Calcula setor de inicio do bloco de dados do ponteiro direto
            int indice_bloco_direto_b = inode.dataPtr[1];
            int setor_inicio_direto_b = indice_bloco_direto_b * superbloco_montado.blockSize;

            ind_setor_dados = floor(ind_byte/TAM_SETOR);
            while(ind_setor_dados < superbloco_montado.blockSize && bytes_read < sizeR && bytes_read < inode.bytesFileSize)
            {

                // Le o setor do disco para buffer_setor
                if(read_sector(base + setor_inicio_direto_b, buffer_setor))  // Le o setor
                {
                    if (DEBUG_MODE){printf("**Erro leitura setor 3**\n");}
                    return -1;
                }
                ind_byte = ind_byte%TAM_SETOR;

                // Percorre o setor gravando byte a byte no buffer enquanto:
                // a) nao chegar ao final do setor
                // b) nao ultrapassar o tamanho maximo do arquivo
                // c) nao ultrapassar o request de bytes a serem lido

                while(ind_byte < TAM_SETOR && open_files[handle].current_pointer < inode.bytesFileSize && bytes_read < sizeR)
                {
                    memcpy(&valor_byte, &buffer_setor[ind_byte], 1);
                    *(buffer + bytes_read) = valor_byte;
                    ind_byte++;
                    bytes_read++;
                    open_files[handle].current_pointer++;
                }
                ind_setor_dados++;
            }
            ind_byte = 0;
        }
    }
    else
    {
        ind_byte -= bytes_por_bloco;
    }

    /// Leitura de dados por INDIREÇÃO SIMPLES
    unsigned char buffer_setor_indirecao[TAM_SETOR], buffer_setor_dados[TAM_SETOR];
    unsigned int i, j, k;
    unsigned int indice_bloco_indirecao, setor_inicio_bloco_indirecao;
    unsigned indice_bloco_dados, setor_inicio_bloco_dados;
    DWORD ponteiro;

    if(open_files[handle].current_pointer < (bytes_por_bloco/sizeof(DWORD))*bytes_por_bloco && bytes_read < sizeR)
    {
        indice_bloco_indirecao = inode.singleIndPtr;
        setor_inicio_bloco_indirecao = indice_bloco_indirecao * superbloco_montado.blockSize; //Localizacao do bloco de ind simples

        int setor_indirecao_comeca_leitura = floor(ind_byte / (qtd_ponteiros_por_setor * bytes_por_bloco));

        i = setor_indirecao_comeca_leitura;
        // Para CADA SETOR do bloco de ind simples, enquanto nao ultrapassar:
        // - tamanho do arquivo
        // - request de bytes a serem lidos
        while(i<superbloco_montado.blockSize && bytes_read < sizeR && bytes_read < inode.bytesFileSize)
        {
            // Le i-esimo setor de ponteiros para buffer
            if(read_sector(base + setor_inicio_bloco_indirecao + i, buffer_setor_indirecao))
            {
                if (DEBUG_MODE){printf("**Erro leitura setor 4**\n");}
                return -1;
            }

            ind_byte = ind_byte % (qtd_ponteiros_por_setor * bytes_por_bloco);
            int ponteiro_comeca_leitura = floor(ind_byte / bytes_por_bloco); // Indice do ponteiro (dentro do setor) em que comeca a leitura

            j = ponteiro_comeca_leitura;
            // Para todo ponteiro (para um bloco de dados) nesse setor
            while(j < qtd_ponteiros_por_setor && bytes_read < sizeR && bytes_read <inode.bytesFileSize)
            {

                memcpy(&ponteiro, &buffer_setor_indirecao[j*sizeof(DWORD)], sizeof(DWORD)); // Le o ponteiro

                // Calcula localizacao do bloco de dados apontado pelo j-esimo ponteiro
                indice_bloco_dados = ponteiro;
                setor_inicio_bloco_dados = indice_bloco_dados * superbloco_montado.blockSize; //Localizacao do bloco de dados

                ind_byte = ind_byte % bytes_por_bloco;
                int setor_bloco_comeca_leitura = floor(ind_byte / TAM_SETOR);
                k = setor_bloco_comeca_leitura;

                while(k < superbloco_montado.blockSize && bytes_read < sizeR && bytes_read < inode.bytesFileSize)
                {
                    //Le setor para o buffer de dados
                    if(read_sector(base + setor_inicio_bloco_dados + k, buffer_setor_dados))
                    {
                        if (DEBUG_MODE){printf("**Erro leitura setor 5*\n");}
                        return -1;
                    }

                    // Percorre o setor gravando byte a byte no buffer enquanto:
                    // a) nao chegar ao final do setor
                    // b) nao ultrapassar o tamanho maximo do arquivo
                    // c) nao ultrapassar o request de bytes a serem lidos

                    // Byte (dentro do setor) em que comeca a leitura
                    ind_byte = ind_byte % TAM_SETOR;

                    while(ind_byte < TAM_SETOR && open_files[handle].current_pointer < inode.bytesFileSize && bytes_read < sizeR)
                    {
                        memcpy(&valor_byte, &buffer_setor_dados[ind_byte], 1);
                        *(buffer + bytes_read) = valor_byte;
                        ind_byte++;
                        bytes_read++;
                        open_files[handle].current_pointer++;
                    }
                    k++;
                    ind_byte = 0;
                }
                j++;
            }
            i++;
        }
    }
    else
    {
        ind_byte -= (qtd_ponteiros_por_setor * superbloco_montado.blockSize * bytes_por_bloco);
    }


    /// Leitura de dados por INDIREÇÃO DUPLA
    unsigned char buffer_setor_indirecao_dupla[TAM_SETOR];
    int l, m;

    // Le dos blocos de indirecao dupla e atualiza current_pointer
    if(open_files[handle].current_pointer < ((bytes_por_bloco/sizeof(DWORD))^2)*bytes_por_bloco && bytes_read < sizeR)
    {

        int setor_inicio_bloco_indirecao_dupla = inode.doubleIndPtr * superbloco_montado.blockSize; // Localizacao do bloco de ind dupla

        // Quantidade total de bytes "contido"/"apontado" em um setor de indirecao dupla
        int qtd_bytes_setor_ind_dupla = (qtd_ponteiros_por_setor * (qtd_ponteiros_por_setor * superbloco_montado.blockSize) * bytes_por_bloco);
        int setor_ind_dup_comeca_leitura = floor(ind_byte / qtd_bytes_setor_ind_dupla);

        k = setor_ind_dup_comeca_leitura;
        // Para todo setor no bloco de ind dupla
        while(k < superbloco_montado.blockSize && bytes_read < inode.bytesFileSize && bytes_read < sizeR)
        {

            if(read_sector(base + setor_inicio_bloco_indirecao_dupla + k, buffer_setor_indirecao_dupla))  // Le o setor
            {
                if (DEBUG_MODE){printf("**Erro leitura setor 6**\n");}
                return -1;
            }

            ind_byte = ind_byte % qtd_bytes_setor_ind_dupla;
            int ponteiro_ind_dupla_comeca_leitura = floor(ind_byte / (qtd_ponteiros_por_setor * superbloco_montado.blockSize * bytes_por_bloco));

            l = ponteiro_ind_dupla_comeca_leitura;

            // Para todo ponteiro (que aponta para um bloco de ind simples) nesse setor
            while(l < qtd_ponteiros_por_setor && bytes_read < inode.bytesFileSize && bytes_read < sizeR)
            {
                DWORD pt1;
                memcpy(&pt1, &buffer_setor_indirecao_dupla[sizeof(DWORD)*l], sizeof(DWORD)); // Le o ponteiro
                int setor_inicio_bloco = pt1 * superbloco_montado.blockSize; // Localizacao do bloco de ind simples

                ind_byte = ind_byte % (qtd_ponteiros_por_setor * superbloco_montado.blockSize * bytes_por_bloco);
                int setor_ind_simp_comeca_leitura = ind_byte / (qtd_ponteiros_por_setor * bytes_por_bloco);

                i = setor_ind_simp_comeca_leitura;

                // Para todo setor no bloco de ind simples
                while(i<superbloco_montado.blockSize && bytes_read < inode.bytesFileSize && bytes_read < sizeR)
                {
                    if(read_sector(base + setor_inicio_bloco + i, buffer_setor_indirecao))  // Le o setor
                    {
                        if (DEBUG_MODE){printf("**Erro leitura setor 7**\n");}
                        return -1;
                    }

                    ind_byte = ind_byte % (qtd_ponteiros_por_setor * bytes_por_bloco);
                    int ponteiro_ind_simp_comeca_leitura = floor(ind_byte / bytes_por_bloco);

                    j = ponteiro_ind_simp_comeca_leitura;

                    // Para todo ponteiro (para um bloco de dados) nesse setor
                    while(j<qtd_ponteiros_por_setor && bytes_read < inode.bytesFileSize && bytes_read < sizeR)
                    {
                        DWORD pt2;
                        memcpy(&pt2, &buffer_setor_indirecao[j*sizeof(DWORD)], sizeof(DWORD)); // Le o ponteiro

                        setor_inicio_bloco_dados = pt2 * superbloco_montado.blockSize; // Localizacao do bloco de ind simples

                        ind_byte = ind_byte % bytes_por_bloco;
                        int setor_dados_comeca_leitura = floor(ind_byte / TAM_SETOR);

                        m = setor_dados_comeca_leitura;
                        // Agora traz setores do bloco apontado pelo ponteiro pt2
                        while(m<superbloco_montado.blockSize && bytes_read < inode.bytesFileSize && bytes_read < sizeR)
                        {
                            if(read_sector(base + setor_inicio_bloco_dados + m, buffer_setor_dados))  // Le o setor
                            {
                                if (DEBUG_MODE){printf("**Erro leitura setor 8 **\n");}
                                return -1;
                            }

                            ind_byte = ind_byte % TAM_SETOR;

                            while(ind_byte < TAM_SETOR && bytes_read < inode.bytesFileSize && bytes_read < sizeR)
                            {
                                memcpy(&valor_byte, &buffer_setor_dados[ind_byte], 1);
                                *(buffer + bytes_read) = valor_byte;
                                ind_byte++;
                                bytes_read++;
                                open_files[handle].current_pointer++;
                            }
                            m++;
                        }
                        j++;
                    }
                    i++;
                }
                l++;
            }
            k++;
        }
    }
    if (DEBUG_MODE){printf("> READ end\n");}
    return bytes_read;
}


/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a escrita de uma certa quantidade
		de bytes (size) de  um arquivo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size)
{
    if (DEBUG_MODE){printf("> WRITE\n");}
// Se particao nao foi montada, se a quantidade de bytes para escrita for negativa ou se
    // o arquivo indicado por handle na open_files table tiver um current_pointer invalido,
    // retorna erro.
    if(!tem_particao_montada || size < 0 || open_files[handle].current_pointer < 0)
    {
        return -1;
    }

    if(opendir2())
    {
        if (DEBUG_MODE){printf("**Erro opendir**\n");}
        return -1;
    }

    // Le registro do arquivo indicado por filename
    struct t2fs_record registro;
    if(get_element(arquivos_diretorio, open_files[handle].filename, &registro))
    {
        if (DEBUG_MODE){printf("**Erro ao pegar arquivo**\n");}
        return -1;
    }

    if(closedir2()){
        if (DEBUG_MODE){printf("**Erro closedir**\n");}
        return -1;
    }

    // Calcula o indice do inode do arquivo indicado por filename
    int indice_inode = registro.inodeNumber;
    int inicio_area_inodes = TAM_SUPERBLOCO + superbloco_montado.freeBlocksBitmapSize + superbloco_montado.freeInodeBitmapSize;
    int setor_inicio_area_inodes = inicio_area_inodes * superbloco_montado.blockSize;
    int inodes_por_setor = TAM_SETOR / sizeof(struct t2fs_inode);
    int setor_inode = setor_inicio_area_inodes + (indice_inode / inodes_por_setor);
    int end_inode = indice_inode % inodes_por_setor;

    int qtd_ponteiros_por_setor = TAM_SETOR / sizeof(DWORD);

    // Le inode do arquivo indicado por filename
    struct t2fs_inode inode;
    unsigned char buffer_inode[TAM_SETOR];
    unsigned char buffer_setor[TAM_SETOR];
    if(read_sector(base + setor_inode, buffer_inode))
    {
        if (DEBUG_MODE){printf("**Erro leitura setor 1**\n");}
        return -1;
    }
    memcpy(&inode, &buffer_inode[end_inode*sizeof(struct t2fs_inode)], sizeof(struct t2fs_inode));

    unsigned int qtde_blocos_arquivo = inode.blocksFileSize;
    unsigned int bytes_por_bloco = TAM_SETOR * superbloco_montado.blockSize;


    // ind_byte: armazena o indice do byte atual para ser escrito do setor (para qualquer um dos blocos apontados pelo inode)
    // bytes_write: total de bytes escritos do arquivo
    unsigned int ind_byte = open_files[handle].current_pointer;
    unsigned int bytes_write= 0;
    int ind_setor_dados;

    ///Caso a escrita seja no primeiro bloco direto
    if(open_files[handle].current_pointer < bytes_por_bloco && bytes_write < size)
    {
        int indice_bloco_direto_a;
        if(inode.dataPtr[0] > -1)
        {
            indice_bloco_direto_a = inode.dataPtr[0];
        }
        else
        {
            indice_bloco_direto_a = aloca_bloco();
            if(indice_bloco_direto_a <= -1)
            {
                return bytes_write;
            }
            inode.dataPtr[0] = indice_bloco_direto_a;
            qtde_blocos_arquivo++;
        }

        // Calcula setor de inicio do bloco de dados do ponteiro direto
        int setor_inicio_direto_a = indice_bloco_direto_a * superbloco_montado.blockSize;
        ind_setor_dados = floor(ind_byte/TAM_SETOR);

        while(ind_setor_dados < superbloco_montado.blockSize && bytes_write < size)
        {
            // Le o setor do disco para buffer_setor
            if(read_sector(base + setor_inicio_direto_a + ind_setor_dados, buffer_setor))  // Le o setor
            {
                if (DEBUG_MODE){printf("**Erro leitura setor 2**\n");}
                return -1;
            }
            ind_byte = ind_byte%TAM_SETOR;
            // Percorre o buffer gravando byte a byte no buffer do setor enquanto:
            // a) nao chegar ao final do setor
            // b) nao ultrapassar o request de bytes a serem escritos

            while(ind_byte < TAM_SETOR && bytes_write < size)
            {
                memcpy(buffer_setor+ind_byte, buffer+bytes_write, 1);
                ind_byte++;
                bytes_write++;
                open_files[handle].current_pointer++;
            }

            if(write_sector(base + setor_inicio_direto_a + ind_setor_dados, buffer_setor))  // Escreve o setor
            {
                if (DEBUG_MODE){printf("**Erro escrita setor 1**\n");}
                return -1;
            }

            ind_setor_dados++;
            ind_byte = 0; // Reseta indice do byte para entrar no proximo laco, caso se aplique
        }
    }

    else
    {
        ind_byte -= bytes_por_bloco; // Caso nao entre no primeiro caso, subtrai tamanho do bloco em bytes da
        // variavel de byte atual para preparar para proximo condicional
    }

    /// Caso a escrita seja no segundo bloco direto
    if(open_files[handle].current_pointer < 2*bytes_por_bloco && bytes_write < size)
    {
        int indice_bloco_direto_b;
        if(inode.dataPtr[1] > -1)
        {
            indice_bloco_direto_b = inode.dataPtr[1];
        }
        else
        {
            indice_bloco_direto_b = aloca_bloco();
            if(indice_bloco_direto_b <= -1)
            {
                return bytes_write;
            }
            inode.dataPtr[1] = indice_bloco_direto_b;
            qtde_blocos_arquivo++;
        }

        // Calcula setor de inicio do bloco de dados do ponteiro direto
        int setor_inicio_direto_b = indice_bloco_direto_b * superbloco_montado.blockSize;
        ind_setor_dados = floor(ind_byte/TAM_SETOR);
        while(ind_setor_dados < superbloco_montado.blockSize && bytes_write < size)
        {

            // Le o setor do disco para buffer_setor
            if(read_sector(base + setor_inicio_direto_b, buffer_setor))  // Le o setor
            {
                if (DEBUG_MODE){printf("**Erro leitura setor 3**\n");}
                return -1;
            }
            ind_byte = ind_byte%TAM_SETOR;

            // Percorre o buffer gravando byte a byte no buffer do setor enquanto:
            // a) nao chegar ao final do setor
            // b) nao ultrapassar o request de bytes a serem escritos

            while(ind_byte < TAM_SETOR && bytes_write < size)
            {
                memcpy(buffer_setor+ind_byte, buffer+bytes_write, 1);
                ind_byte++;
                bytes_write++;
                open_files[handle].current_pointer++;
            }
            ind_setor_dados++;
            if(write_sector(base + setor_inicio_direto_b, buffer_setor))  // Escreve o setor
            {
                if (DEBUG_MODE){printf("**Erro escrita setor 2**\n");}
                return -1;
            }
        }
        ind_byte = 0;
    }
    else
    {
        ind_byte -= bytes_por_bloco;
    }

/// Escrita de dados por INDIREÇÃO SIMPLES
    unsigned char buffer_setor_indirecao[TAM_SETOR], buffer_setor_dados[TAM_SETOR];
    unsigned int i, j, k;
    unsigned int indice_bloco_indirecao, setor_inicio_bloco_indirecao;
    unsigned indice_bloco_dados, setor_inicio_bloco_dados;
    DWORD ponteiro;

    if(open_files[handle].current_pointer < (bytes_por_bloco/sizeof(DWORD))*bytes_por_bloco && bytes_write < size)
    {
        if(inode.singleIndPtr == -1)
        {
            indice_bloco_indirecao = aloca_bloco();
            if(indice_bloco_indirecao <= -1)
            {
                return bytes_write;
            }
            inode.singleIndPtr = indice_bloco_indirecao;
        }
        else
        {
            indice_bloco_indirecao = inode.singleIndPtr;
        }

        setor_inicio_bloco_indirecao = indice_bloco_indirecao * superbloco_montado.blockSize; //Localizacao do bloco de ind simples
        int setor_indirecao_comeca_escrita = floor(ind_byte / (qtd_ponteiros_por_setor * bytes_por_bloco));
        i = setor_indirecao_comeca_escrita;

        // Para CADA SETOR do bloco de ind simples, enquanto nao ultrapassar:
        // - request de bytes a serem escritos
        while(i<superbloco_montado.blockSize && bytes_write < size)
        {
            // Le i-esimo setor de ponteiros para buffer
            if(read_sector(base + setor_inicio_bloco_indirecao + i, buffer_setor_indirecao))
            {
                if (DEBUG_MODE){printf("**Erro leitura setor 4**\n");}
                return -1;
            }

            ind_byte = ind_byte % (qtd_ponteiros_por_setor * bytes_por_bloco);
            int ponteiro_comeca_leitura = floor(ind_byte / bytes_por_bloco); // Indice do ponteiro (dentro do setor) em que comeca a leitura

            j = ponteiro_comeca_leitura;
            // Para todo ponteiro (para um bloco de dados) nesse setor
            while(j < qtd_ponteiros_por_setor && bytes_write < size)
            {
                memcpy(&ponteiro, &buffer_setor_indirecao[j*sizeof(DWORD)], sizeof(DWORD)); // Le o ponteiro

                if(ponteiro == 0)
                {
                    indice_bloco_dados = aloca_bloco();
                    if(indice_bloco_dados <= -1)
                    {
                        return bytes_write;
                    }
                    memcpy(&buffer_setor_indirecao[j*sizeof(DWORD)], &indice_bloco_dados, sizeof(DWORD));
                    qtde_blocos_arquivo++;
                }
                else
                {
                    indice_bloco_dados = ponteiro;
                }
                // Calcula localizacao do bloco de dados apontado pelo j-esimo ponteiro

                setor_inicio_bloco_dados = indice_bloco_dados * superbloco_montado.blockSize; //Localizacao do bloco de dados

                ind_byte = ind_byte % bytes_por_bloco;
                int setor_bloco_comeca_escrita = floor(ind_byte / TAM_SETOR);
                k = setor_bloco_comeca_escrita;

                while(k < superbloco_montado.blockSize && bytes_write < size)
                {
                    //Le setor para o buffer de dados
                    if(read_sector(base + setor_inicio_bloco_dados + k, buffer_setor_dados))
                    {
                        if (DEBUG_MODE){printf("**Erro leitura setor 5**\n");}
                        return -1;
                    }

                    // Percorre o buffer gravando byte a byte no buffer do setor enquanto:
                    // a) nao chegar ao final do setor
                    // b) nao ultrapassar o request de bytes a serem escritos

                    // Byte (dentro do setor) em que comeca a leitura
                    ind_byte = ind_byte % TAM_SETOR;

                    while(ind_byte < TAM_SETOR && bytes_write < size)
                    {
                        memcpy(buffer_setor_dados+ind_byte, buffer+bytes_write, 1);
                        ind_byte++;
                        bytes_write++;
                        open_files[handle].current_pointer++;
                    }
                    if(write_sector(base + setor_inicio_bloco_dados + k, buffer_setor_dados))
                    {
                        if (DEBUG_MODE){printf("**Erro escrita setor 3**\n");}
                        return -1;
                    }
                    k++;
                    ind_byte = 0;
                }
                j++;
            }
            // Escrita do setor para escrever os possiveis ponteiros criados
            if(write_sector(base + setor_inicio_bloco_indirecao + i, buffer_setor_indirecao))
            {
                return -1;
            }
            i++;
        }
    }
    else
    {
        ind_byte -= (qtd_ponteiros_por_setor * superbloco_montado.blockSize * bytes_por_bloco);
    }


/// Escrita de dados por INDIREÇÃO DUPLA
    unsigned char buffer_setor_indirecao_dupla[TAM_SETOR];
    int l, m;


    if(open_files[handle].current_pointer < ((bytes_por_bloco/sizeof(DWORD))^2)*bytes_por_bloco && bytes_write < size)
    {
        if(inode.doubleIndPtr == -1)
        {
            inode.doubleIndPtr = aloca_bloco();
            if(inode.doubleIndPtr <= -1)
            {
                inode.doubleIndPtr = -1;
                return bytes_write;
            }
        }
        int setor_inicio_bloco_indirecao_dupla = inode.doubleIndPtr * superbloco_montado.blockSize; // Localizacao do bloco de ind dupla

        // Quantidade total de bytes "contido"/"apontado" em um setor de indirecao dupla
        int qtd_bytes_setor_ind_dupla = (qtd_ponteiros_por_setor * (qtd_ponteiros_por_setor * superbloco_montado.blockSize) * bytes_por_bloco);
        int setor_ind_dup_comeca_escrita = floor(ind_byte / qtd_bytes_setor_ind_dupla);

        k = setor_ind_dup_comeca_escrita;
        // Para todo setor no bloco de ind dupla
        while(k < superbloco_montado.blockSize && bytes_write < size)
        {
            if(read_sector(base + setor_inicio_bloco_indirecao_dupla + k, buffer_setor_indirecao_dupla))  // Le o setor
            {
                if (DEBUG_MODE){printf("**Erro leitura setor 6**\n");}
                return -1;
            }

            ind_byte = ind_byte % qtd_bytes_setor_ind_dupla;
            int ponteiro_ind_dupla_comeca_escrita = floor(ind_byte / (qtd_ponteiros_por_setor * superbloco_montado.blockSize * bytes_por_bloco));

            l = ponteiro_ind_dupla_comeca_escrita;

            // Para todo ponteiro (que aponta para um bloco de ind simples) nesse setor
            while(l < qtd_ponteiros_por_setor && bytes_write < size)
            {
                DWORD pt1;
                memcpy(&pt1, &buffer_setor_indirecao_dupla[sizeof(DWORD)*l], sizeof(DWORD)); // Le o ponteiro

                if(pt1 == 0)
                {
                    pt1 = aloca_bloco();
                    if(pt1 <= -1)
                    {
                        return bytes_write;
                    }
                    memcpy(&buffer_setor_indirecao_dupla[sizeof(DWORD)*l], &pt1, sizeof(DWORD));
                }

                int setor_inicio_bloco = pt1 * superbloco_montado.blockSize; // Localizacao do bloco de ind simples

                ind_byte = ind_byte % (qtd_ponteiros_por_setor * superbloco_montado.blockSize * bytes_por_bloco);
                int setor_ind_simp_comeca_escrita = ind_byte / (qtd_ponteiros_por_setor * bytes_por_bloco);

                i = setor_ind_simp_comeca_escrita;

                // Para todo setor no bloco de ind simples
                while(i<superbloco_montado.blockSize && bytes_write < size)
                {
                    if(read_sector(base + setor_inicio_bloco + i, buffer_setor_indirecao))  // Le o setor
                    {
                        if (DEBUG_MODE){printf("**Erro leitura setor 7**\n");}
                        return -1;
                    }

                    ind_byte = ind_byte % (qtd_ponteiros_por_setor * bytes_por_bloco);
                    int ponteiro_ind_simp_comeca_escrita = floor(ind_byte / bytes_por_bloco);

                    j = ponteiro_ind_simp_comeca_escrita;

                    // Para todo ponteiro (para um bloco de dados) nesse setor
                    while(j<qtd_ponteiros_por_setor && bytes_write < size)
                    {
                        DWORD pt2;
                        memcpy(&pt2, &buffer_setor_indirecao[j*sizeof(DWORD)], sizeof(DWORD)); // Le o ponteiro

                        if(pt2 == 0)
                        {
                            pt2 = aloca_bloco();
                            if(pt2 <= -1)
                            {
                                return bytes_write;
                            }
                            memcpy(&buffer_setor_indirecao[j*sizeof(DWORD)], &pt2, sizeof(DWORD)); // Escreve o ponteiro
                            qtde_blocos_arquivo++;
                        }

                        setor_inicio_bloco_dados = pt2 * superbloco_montado.blockSize; // Localizacao do bloco de ind simples

                        ind_byte = ind_byte % bytes_por_bloco;
                        int setor_dados_comeca_escrita = floor(ind_byte / TAM_SETOR);

                        m = setor_dados_comeca_escrita;
                        // Agora traz setores do bloco apontado pelo ponteiro pt2
                        while(m<superbloco_montado.blockSize && bytes_write < size)
                        {
                            if(read_sector(base + setor_inicio_bloco_dados + m, buffer_setor_dados))  // Le o setor
                            {
                                if (DEBUG_MODE){printf("**Erro leitura setor 8**\n");}
                                return -1;
                            }

                            ind_byte = ind_byte % TAM_SETOR;

                            while(ind_byte < TAM_SETOR && bytes_write < size)
                            {
                                memcpy(buffer_setor_dados+ind_byte, buffer+bytes_write, 1);
                                ind_byte++;
                                bytes_write++;
                                open_files[handle].current_pointer++;
                            }
                            if(write_sector(base + setor_inicio_bloco_dados + m, buffer_setor_dados))  // Escreve o setor
                            {
                                if (DEBUG_MODE){printf("**Erro escrita setor 4**\n");}
                                return -1;
                            }
                            m++;
                        }
                        j++;
                    }
                    if(write_sector(base + setor_inicio_bloco + i, buffer_setor_indirecao))  // Le o setor
                    {
                        if (DEBUG_MODE){printf("**Erro escrita setor 5**\n");}
                        return -1;
                    }
                    i++;
                }
                l++;
            }
            if(write_sector(base + setor_inicio_bloco_indirecao_dupla + k, buffer_setor_indirecao_dupla))  // Le o setor
            {
                if (DEBUG_MODE){printf("**Erro escrita setor 6**\n");}
                return -1;
            }
            k++;
        }
    }

    /// Escrita do inode (possivelmente) atualizado
    inode.blocksFileSize = qtde_blocos_arquivo;
    if(open_files[handle].current_pointer > inode.bytesFileSize)
    {
        inode.bytesFileSize = open_files[handle].current_pointer;
    }

    memcpy(&buffer_inode[end_inode*sizeof(struct t2fs_inode)], &inode, sizeof(struct t2fs_inode));
    if(write_sector(base + setor_inode, buffer_inode))
    {
        if (DEBUG_MODE){printf("**Erro escrita - final**\n");}
        return -1;
    }

    return bytes_write;
}

int carregaRegistrosDataPtr(DWORD blockNumber)
{
    unsigned char buffer[TAM_SETOR] = {0};
    int records_por_setor = TAM_SETOR/ sizeof(struct t2fs_record);
    int i, j;
    struct t2fs_record registro;

    for(i = 0; i < superbloco_montado.blockSize; i++)
    {
        int sector = blockNumber*superbloco_montado.blockSize + i;
        if (read_sector(base + sector, buffer) == -1)
        {
            if (DEBUG_MODE){printf("**Erro ao ler setor em carregamento de registros**\n");}
            return -1;
        }
        for(j = 0; j < records_por_setor; j++)
        {
            memcpy(&registro, buffer + j * sizeof(struct t2fs_record), sizeof(struct t2fs_record));
            if(registro.TypeVal != 0x00)
            {
                printf("&Arquivo achado: %s\n", registro.name);
                arquivos_diretorio = insert_element(arquivos_diretorio, registro); // Adiciona registro em lista de arquivos abertos
            }
            else
            {
                return 0; // Ao chegar ao arquivo invalido, retorna - sem erros
            }
        }
    }
    return 0;
}


int carregaRegistrosSingleIndPtr(DWORD blockNumber)
{
    int qtd_ponteiros_por_setor = TAM_SETOR / sizeof(DWORD);
    DWORD bloco_dataPtrDireto; // Bloco que contera' os ponteiros diretos obtidos da indirecao simples

    unsigned char buffer[TAM_SETOR] = {0}; // Buffer para leitura

    boolean controle_fluxo = true; // controle do fluxo de leitura dos blocos
    int ptr_setor = 0; // utilizado para leitura dos setores
    int i_bloco = 0; // utilizado para leitura coletar os dados a partir do setor lido

    while (controle_fluxo)
    {
        int setor = blockNumber * superbloco_montado.blockSize + ptr_setor; // construcao do setor para leitura
        if (read_sector(base + setor, buffer) == -1)  // le do setor (arrumando valor com base)
        {
            if (DEBUG_MODE){printf("**Erro leitura setor 1 - carrega ind simples**\n");}
            return -1;
        }

        do
        {
            memcpy(&bloco_dataPtrDireto, buffer + i_bloco * sizeof(DWORD), sizeof(DWORD)); // coleta bloco do setor e insere em variavel de bloco direto
            i_bloco++;

            if (bloco_dataPtrDireto != 0)  // Caso o valor nao seja invalido, carrega registros a partir dele
            {
                if (carregaRegistrosDataPtr(bloco_dataPtrDireto) == -1)
                {
                    return -1;
                }
            }

        }
        while(bloco_dataPtrDireto != 0 && i_bloco < qtd_ponteiros_por_setor);
        // Caso passe por blocos nao mais validos ou passa dos blocos validos, sai do laço

        if (bloco_dataPtrDireto == 0)  // se blocos deixaram de ser validos, encerra laço
        {
            controle_fluxo = false;
        }

        ptr_setor++;
        if (ptr_setor >= superbloco_montado.blockSize)  // se passou dos setores considerando o tamanho do bloco, encerra laço
        {
            controle_fluxo = false;
        }

        i_bloco = 0;
    }
    return 0;
}


int carregaRegistrosDoubleIndPtr(DWORD blockNumber)
{
    int qtd_ponteiros_por_setor = TAM_SETOR / sizeof(DWORD);
    DWORD bloco_dataPtrIndireto; // Bloco que contera' os ponteiros de indirecao simples

    unsigned char buffer[TAM_SETOR] = {0}; // Buffer para leitura

    boolean controle_fluxo = true; // controle do fluxo de leitura dos blocos
    int ptr_setor = 0; // utilizado para leitura dos setores
    int i_bloco = 0; // utilizado para leitura coletar os dados a partir do setor lido

    while (controle_fluxo)
    {
        int setor = blockNumber * superbloco_montado.blockSize + ptr_setor; // construcao do setor para leitura
        if (read_sector(base + setor, buffer) == -1)  // le do setor (arrumando valor com base)
        {
            if (DEBUG_MODE){printf("**Erro leitura setor - carrega ind duplo**\n");}
            return -1;
        }

        do
        {
            memcpy(&bloco_dataPtrIndireto, buffer + i_bloco * sizeof(DWORD), sizeof(DWORD)); // coleta bloco do setor e insere em variavel de bloco de indirecao simples
            i_bloco++;

            if (bloco_dataPtrIndireto != 0)  // Caso o valor nao seja invalido, carrega registros a partir dele
            {
                if (carregaRegistrosSingleIndPtr(bloco_dataPtrIndireto) == -1)
                {
                    return -1;
                }
            }

        }
        while(bloco_dataPtrIndireto != 0 && i_bloco < qtd_ponteiros_por_setor);
        // Caso passe por blocos nao mais validos ou passa dos blocos validos, sai do laço

        if (bloco_dataPtrIndireto == 0)  // se blocos deixaram de ser validos, encerra laço
        {
            controle_fluxo = false;
        }

        ptr_setor++;
        if (ptr_setor >= superbloco_montado.blockSize)  // se passou dos setores considerando o tamanho do bloco, encerra laço
        {
            controle_fluxo = false;
        }

        i_bloco = 0;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um diretório existente no disco.
-----------------------------------------------------------------------------*/
int opendir2 (void)
{
    if (DEBUG_MODE){printf("> OPENDIR\n");}
    if(!tem_particao_montada)
    {

        return -1;
    }

    if (diretorio_aberto)  // Caso diretorio ja esteja aberto, retorna erro
    {
        if (DEBUG_MODE){printf("**Erro Diretorio ja aberto**\n");}
        return -1;
    }

    arquivos_diretorio = create_linked_list(); // Inicializa lista de arquivos do diretorio

    unsigned char buffer[TAM_SETOR] = {0};
    struct t2fs_inode* inode = (struct t2fs_inode*) malloc (sizeof(struct t2fs_inode));

    int inicio_area_inodes = TAM_SUPERBLOCO + superbloco_montado.freeBlocksBitmapSize + superbloco_montado.freeInodeBitmapSize;
    int setor_inicio_area_inodes = inicio_area_inodes * superbloco_montado.blockSize;
    int setor_inode = setor_inicio_area_inodes;

    if(read_sector(base + setor_inode, buffer))
    {
        if (DEBUG_MODE){printf("**Erro leitura setor**\n");}
        return -1;
    }
    memcpy(inode, buffer, sizeof(struct t2fs_inode)); // Diretorio raiz esta associado ao inode 0


    int i = 0;
    for(i=0; i<2; i++)   // Indica blocos apontados diretamente como livres
    {
        if(inode->dataPtr[i] != -1)  // ponteiro invalido
        {
            if (carregaRegistrosDataPtr(inode->dataPtr[i]) == -1)
            {
                if (DEBUG_MODE){printf("**Erro carregamento dados direos %d**\n", i);}
                return -1;
            }
        }
    }
    if (inode->singleIndPtr != -1)  // ponteiro invalido
    {
        if (carregaRegistrosSingleIndPtr(inode->singleIndPtr) == -1)
        {
            if (DEBUG_MODE){printf("**Erro blocos ind simples**\n");}
            return -1;
        }
    }
    if (inode->doubleIndPtr != -1)  // ponteiro invalido
    {
        if (carregaRegistrosDoubleIndPtr(inode->doubleIndPtr) == -1)
        {
            if (DEBUG_MODE){printf("**Erro blocos ind duplos**\n");}
            return -1;
        }
    }

    current_dentry = arquivos_diretorio;

    diretorio_aberto = true;

    if (DEBUG_MODE){printf("> OPENDIR end\n");}
    return 0;
}


/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2 (DIRENT2 *dentry)
{
    if (DEBUG_MODE){printf("> READDIR\n");}

    if(!tem_particao_montada)
    {
        return -1;
    }

    if(!diretorio_aberto)  // Caso diretorio nao esteja aberto, retorna erro
    {
        if (DEBUG_MODE){printf("**Erro dir fechado**\n");}
        return -1;
    }

    if(current_dentry == NULL){
        if (DEBUG_MODE){printf("Current dentry null\n");}
        return -1;
    }

    struct t2fs_record registro = current_dentry->registro;
    //printf("inodenum: %d\n", registro.inodeNumber);

    DIRENT2* entradaDir = (DIRENT2*) malloc (sizeof(DIRENT2));

    memcpy(entradaDir->name, registro.name, 51);
    entradaDir->fileType = registro.TypeVal;

    // Procura por inode para adicionar valor de tamanho em dentry
    unsigned char buff[TAM_SETOR] = {0};
    int inicio_area_inodes = TAM_SUPERBLOCO + superbloco_montado.freeBlocksBitmapSize + superbloco_montado.freeInodeBitmapSize;
    int setor_inicio_area_inodes = inicio_area_inodes * superbloco_montado.blockSize;
    int inodes_por_setor = TAM_SETOR / sizeof(struct t2fs_inode);
    int setor_inode = setor_inicio_area_inodes + (registro.inodeNumber / inodes_por_setor);
    int end_inode = registro.inodeNumber % inodes_por_setor;

    read_sector(base + setor_inode, buff);

    struct t2fs_inode inode;
    memcpy(&inode, &buff[end_inode*sizeof(struct t2fs_inode)], sizeof(struct t2fs_inode));

    entradaDir->fileSize = inode.bytesFileSize;

    *dentry = *entradaDir;
    current_dentry = current_dentry->next;

    if (DEBUG_MODE){printf("> READDIR end\n");}
    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.
-----------------------------------------------------------------------------*/
int closedir2 (void)
{
    if (DEBUG_MODE){printf("> CLOSEDIR\n");}
    if(!tem_particao_montada)
    {
        return -1;
    }

    if(!diretorio_aberto)
    {
        if (DEBUG_MODE){printf("**Erro diretorio fechado**\n");}
        return -1;
    }

    int qtd_ponteiros_por_setor = TAM_SETOR / sizeof(DWORD);
    unsigned int bytes_por_bloco = TAM_SETOR * superbloco_montado.blockSize;
    unsigned char buffer_inode[TAM_SETOR];
    unsigned char buffer[TAM_SETOR];
    int registros_escritos = 0;
    int ind_setor_dados, ind_reg;
    int registros_por_setor = TAM_SETOR / sizeof(struct t2fs_record);
    int tam_registro = sizeof(struct t2fs_record);

    // Calcula o setor de inicio da area de inodes
    int inicio_area_inodes = TAM_SUPERBLOCO + superbloco_montado.freeBlocksBitmapSize + superbloco_montado.freeInodeBitmapSize;

    // Le inode do diretorio raiz (i-node 0)
    struct t2fs_inode inode;
    if(read_sector(base + (inicio_area_inodes * superbloco_montado.blockSize), buffer_inode))
    {
        if (DEBUG_MODE){printf("**Erro leitura setor - inode raiz**\n");}
        return -1;
    }
    memcpy(&inode, buffer_inode, sizeof(struct t2fs_inode));
    if (DEBUG_MODE){printf(">> Insere tipo invalido\n");}
    // Adiciona um registro invalido ao fim da lista, para usar como criterio de parada na opendir2
    struct t2fs_record registro_invalido;
    registro_invalido.inodeNumber = -1;
    strcpy(registro_invalido.name, "invalid.invalid");
    registro_invalido.TypeVal = TYPEVAL_INVALIDO;
    arquivos_diretorio = insert_element(arquivos_diretorio, registro_invalido);

    // Calcula o tamanho da lista de registros
    int qtd_registros = 0;
    Linked_List *aux = arquivos_diretorio;
    while(aux != NULL)
    {
        qtd_registros++;
        aux = aux->next;
    }

    printf("<> Quant arquivos: %d\n", qtd_registros);

    aux = arquivos_diretorio;

    /// Escrita no primeiro bloco direto
    if(registros_escritos < qtd_registros)
    {
        if (DEBUG_MODE){printf("*Direto A*\n");}
        int indice_bloco_direto_a;

        if(inode.dataPtr[0] == -1)
        {
            indice_bloco_direto_a = aloca_bloco();
            if(indice_bloco_direto_a <= -1)
            {
                if (DEBUG_MODE){printf("**Erro indice bloco direto a invalido**\n");}
                return -1;
            }
            inode.dataPtr[0] = indice_bloco_direto_a;
        }

        indice_bloco_direto_a = inode.dataPtr[0];

        // Calcula setor de inicio do bloco de dados do ponteiro direto
        int setor_inicio_direto_a = indice_bloco_direto_a * superbloco_montado.blockSize;
        ind_setor_dados = 0;

        while(ind_setor_dados < superbloco_montado.blockSize && registros_escritos < qtd_registros)
        {
            ind_reg = 0;
            while(registros_escritos < qtd_registros &&  ind_reg < registros_por_setor)
            {
                memcpy(buffer + registros_escritos*tam_registro, &aux->registro, tam_registro);
                printf("\t--Arquivo escrito: %s\n", aux->registro.name);
                aux = aux->next;
                registros_escritos++;
                ind_reg++;
            }

            if(write_sector(base + setor_inicio_direto_a + ind_setor_dados, buffer))  // Escreve o setor
            {
                if (DEBUG_MODE){printf("**Erro escrita setor direto a**\n");}
                return -1;
            }

            ind_setor_dados++;
        }
    }

    /// Escrita no segundo bloco direto
    if(registros_escritos < qtd_registros)
    {
        if (DEBUG_MODE){printf("*Direto B*\n");}
        int indice_bloco_direto_b;

        if(inode.dataPtr[1] == -1)
        {
            indice_bloco_direto_b = aloca_bloco();
            if(indice_bloco_direto_b <= -1)
            {
                if (DEBUG_MODE){printf("**Erro indice bloco direto b invalido**\n");}
                return -1;
            }
            inode.dataPtr[1] = indice_bloco_direto_b;
        }

        indice_bloco_direto_b = inode.dataPtr[1];

        // Calcula setor de inicio do bloco de dados do ponteiro direto
        int setor_inicio_direto_b = indice_bloco_direto_b * superbloco_montado.blockSize;
        ind_setor_dados = 0;

        while(ind_setor_dados < superbloco_montado.blockSize && registros_escritos < qtd_registros)
        {
            ind_reg = 0;
            while(registros_escritos < qtd_registros &&  ind_reg < registros_por_setor)
            {
                memcpy(buffer + ind_reg*tam_registro, &aux->registro, tam_registro);
                aux = aux->next;
                registros_escritos++;
                ind_reg++;
            }

            if(write_sector(base + setor_inicio_direto_b + ind_setor_dados, buffer))  // Escreve o setor
            {
                if (DEBUG_MODE){printf("**Erro escrita setor direto b**\n");}
                return -1;
            }

            ind_setor_dados++;
        }
    }


/// Escrita por INDIREÇÃO SIMPLES
    unsigned char buffer_setor_indirecao[TAM_SETOR];
    unsigned int i, j, k;
    unsigned int indice_bloco_indirecao, setor_inicio_bloco_indirecao;
    unsigned indice_bloco_dados, setor_inicio_bloco_dados;
    DWORD ponteiro;

    if(registros_escritos < qtd_registros)
    {
        if (DEBUG_MODE){printf("*Ind Simples*\n");}

        if(inode.singleIndPtr == -1)
        {
            indice_bloco_indirecao = aloca_bloco();
            if(indice_bloco_indirecao <= -1)
            {
                if (DEBUG_MODE){printf("**Erro bloco indirecao simples**\n");}
                return -1;
            }
            inode.singleIndPtr = indice_bloco_indirecao;
        }
        else
        {
            indice_bloco_indirecao = inode.singleIndPtr;
        }

        setor_inicio_bloco_indirecao = indice_bloco_indirecao * superbloco_montado.blockSize; //Localizacao do bloco de ind simples
        i = 0;

        while(i<superbloco_montado.blockSize && registros_escritos < qtd_registros)
        {
            // Le i-esimo setor de ponteiros para buffer
            if(read_sector(base + setor_inicio_bloco_indirecao + i, buffer_setor_indirecao))
            {
                if (DEBUG_MODE){printf("**Erro leiruta setor indSimples**\n");}
                return -1;
            }

            j = 0;
            // Para todo ponteiro (para um bloco de dados) nesse setor
            while(j < qtd_ponteiros_por_setor && registros_escritos < qtd_registros)
            {
                memcpy(&ponteiro, &buffer_setor_indirecao[j*sizeof(DWORD)], sizeof(DWORD)); // Le o ponteiro

                if(ponteiro == 0)
                {
                    indice_bloco_dados = aloca_bloco();
                    if(indice_bloco_dados <= -1)
                    {
                        if (DEBUG_MODE){printf("**Erro indice bloco de dados indSimples**\n");}
                        return -1;
                    }
                    memcpy(&buffer_setor_indirecao[j*sizeof(DWORD)], &indice_bloco_dados, sizeof(DWORD));
                }
                else
                {
                    indice_bloco_dados = ponteiro;
                }

                // Calcula localizacao do bloco de dados apontado pelo j-esimo ponteiro
                setor_inicio_bloco_dados = indice_bloco_dados * superbloco_montado.blockSize; //Localizacao do bloco de dados

                k = 0;
                while(k < superbloco_montado.blockSize && registros_escritos < qtd_registros)
                {
                    int ind_reg = 0;
                    while(registros_escritos < qtd_registros &&  ind_reg < registros_por_setor)
                    {
                        memcpy(buffer + ind_reg*tam_registro, &aux->registro, tam_registro);
                        aux = aux->next;
                        registros_escritos++;
                        ind_reg++;
                    }

                    if(write_sector(base + setor_inicio_bloco_dados + k, buffer))
                    {
                        if (DEBUG_MODE){printf("**Erro escrita setor indSimples 1**\n");}
                        return -1;
                    }
                    k++;
                }
                j++;
            }
            // Escrita do setor para escrever os possiveis ponteiros criados
            if(write_sector(base + setor_inicio_bloco_indirecao + i, buffer_setor_indirecao))
            {
                if (DEBUG_MODE){printf("**Erro escrita setor indSimples 2**\n");}
                return -1;
            }
            i++;
        }
    }


/// Escrita por INDIREÇÃO DUPLA
    unsigned char buffer_setor_indirecao_dupla[TAM_SETOR];
    int l, m;

    if(registros_escritos < qtd_registros)
    {
        if (DEBUG_MODE){printf("*Ind Dupla*\n");}
        if(inode.doubleIndPtr == -1)
        {
            inode.doubleIndPtr = aloca_bloco();
            if(inode.doubleIndPtr <= -1)
            {
                if (DEBUG_MODE){printf("**Erro indice bloco indDupla**\n");}
                inode.doubleIndPtr = -1;
                return -1;
            }
        }

        int setor_inicio_bloco_indirecao_dupla = inode.doubleIndPtr * superbloco_montado.blockSize; // Localizacao do bloco de ind dupla
        k = 0;
        // Para todo setor no bloco de ind dupla
        while(k < superbloco_montado.blockSize && registros_escritos < qtd_registros)
        {
            if(read_sector(base + setor_inicio_bloco_indirecao_dupla + k, buffer_setor_indirecao_dupla))  // Le o setor
            {
                if (DEBUG_MODE){printf("**Erro leitura setor indDupla 1**\n");}
                return -1;
            }

            l = 0;
            // Para todo ponteiro (que aponta para um bloco de ind simples) nesse setor
            while(l < qtd_ponteiros_por_setor && registros_escritos < qtd_registros)
            {
                DWORD pt1;
                memcpy(&pt1, &buffer_setor_indirecao_dupla[sizeof(DWORD)*l], sizeof(DWORD)); // Le o ponteiro

                if(pt1 == 0)
                {
                    pt1 = aloca_bloco();
                    if(pt1 <= -1)
                    {
                        if (DEBUG_MODE){printf("**Erro alocacao bloco 1**\n");}
                        return -1;
                    }
                    memcpy(&buffer_setor_indirecao_dupla[sizeof(DWORD)*l], &pt1, sizeof(DWORD));
                }

                int setor_inicio_bloco = pt1 * superbloco_montado.blockSize; // Localizacao do bloco de ind simples
                i = 0;

                // Para todo setor no bloco de ind simples
                while(i<superbloco_montado.blockSize && registros_escritos < qtd_registros)
                {
                    if(read_sector(base + setor_inicio_bloco + i, buffer_setor_indirecao))  // Le o setor
                    {
                        if (DEBUG_MODE){printf("**Erro leitura setor indDupla 2**\n");}
                        return -1;
                    }

                    j = 0;
                    // Para todo ponteiro (para um bloco de dados) nesse setor
                    while(j<qtd_ponteiros_por_setor && registros_escritos < qtd_registros)
                    {
                        DWORD pt2;
                        memcpy(&pt2, &buffer_setor_indirecao[j*sizeof(DWORD)], sizeof(DWORD)); // Le o ponteiro

                        if(pt2 == 0)
                        {
                            pt2 = aloca_bloco();
                            if(pt2 <= -1)
                            {
                                if (DEBUG_MODE){printf("**Erro alocacao bloco 2**\n");}
                                return -1;
                            }
                            memcpy(&buffer_setor_indirecao[j*sizeof(DWORD)], &pt2, sizeof(DWORD)); // Escreve o ponteiro
                        }

                        setor_inicio_bloco_dados = pt2 * superbloco_montado.blockSize; // Localizacao do bloco de ind simples

                        m = 0;
                        // Agora traz setores do bloco apontado pelo ponteiro pt2
                        while(m<superbloco_montado.blockSize && registros_escritos < qtd_registros)
                        {
                            int ind_reg = 0;
                            while(registros_escritos < qtd_registros &&  ind_reg < registros_por_setor)
                            {
                                memcpy(buffer + ind_reg*tam_registro, &aux->registro, tam_registro);
                                aux = aux->next;
                                registros_escritos++;
                                ind_reg++;
                            }
                            if(write_sector(base + setor_inicio_bloco_dados + m, buffer))  // Escreve o setor
                            {
                                if (DEBUG_MODE){printf("**Erro escrita setor indDupla 1**\n");}
                                return -1;
                            }
                            m++;
                        }
                        j++;
                    }
                    if(write_sector(base + setor_inicio_bloco + i, buffer_setor_indirecao))  // Le o setor
                    {
                        if (DEBUG_MODE){printf("**Erro escrita setor indDupla 2**\n");}
                        return -1;
                    }
                    i++;
                }
                l++;
            }
            if(write_sector(base + setor_inicio_bloco_indirecao_dupla + k, buffer_setor_indirecao_dupla))  // Le o setor
            {
                if (DEBUG_MODE){printf("**Erro escrita setor indDupla 3**\n");}
                return -1;
            }
            k++;
        }
    }

    /// Escrita do inode (possivelmente) atualizado
    inode.bytesFileSize = registros_escritos * tam_registro;
    inode.blocksFileSize = ceil((float)inode.bytesFileSize / bytes_por_bloco);

    memcpy(buffer_inode, &inode, sizeof(struct t2fs_inode));
    if(write_sector(base + inicio_area_inodes, buffer_inode))
    {
        if (DEBUG_MODE){printf("**Erro escrita inode raiz (atualizado)**\n");}
        return -1;
    }
    if (DEBUG_MODE){printf(">> Limpa memoria de arquivos antigos\n");}
    // Limpa memória dos arquivos que estavam em lista de arquivos de diretorio
//    aux = arquivos_diretorio;
//    while(aux != NULL){
//        printf("Arq removido: %s\n", aux->registro.name);
//        aux = aux->next;
//        printf("\taux pega proximo\n");
//        free(arquivos_diretorio);
//        printf("\tlimpa arq\n");
//        arquivos_diretorio = aux;
//        printf("\tpega novo de aux\n");
//    }
    while (arquivos_diretorio!=NULL){
        printf("~~%s\n", arquivos_diretorio->registro.name);
        arquivos_diretorio = delete_element(arquivos_diretorio, arquivos_diretorio->registro.name);
    }
    //arquivos_diretorio = NULL;
//    printf("~~%s\n", arquivos_diretorio->registro.name);
//    if (arquivos_diretorio->next != NULL){
//        printf("~~%s\n", arquivos_diretorio->next->registro.name);
//    }
    current_dentry = NULL;

    diretorio_aberto = false;

    if (DEBUG_MODE){printf("> CLOSE end\n");}
    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (softlink)
-----------------------------------------------------------------------------*/
int sln2 (char *linkname, char *filename)
{

    if(!tem_particao_montada)
    {
        return -1;
    }

    if(opendir2())
    {
        return -1;
    }

    // Verifica se ja nao existe um arquivo com mesmo nome que o link
    if(contains(arquivos_diretorio, linkname))
    {
        closedir2();
        return -1;
    }

    // Verifica se o arquivo referenciado existe
    if(!contains(arquivos_diretorio, filename))
    {
        closedir2();
        return -1;
    }

    // Aloca um bloco de dados que vai conter o nome do arquivo referenciado
    int indice_bloco = aloca_bloco();
    if(indice_bloco <= -1)
    {
        closedir2();
        return -1;
    }

    unsigned char buffer[TAM_SETOR];
    memcpy(buffer, filename, sizeof(filename));

    // Escreve o bloco de dados no disco
    int setor_inicio_bloco = indice_bloco * superbloco_montado.blockSize;
    if(write_sector(base + setor_inicio_bloco, buffer))
    {
        closedir2();
        return -1;
    }

    /// T2FS Record
    struct t2fs_record registro;
    registro.TypeVal = TYPEVAL_LINK;
    if (strlen(linkname) > 50)  // Caso o nome passado como parametro seja maior que o tamanho máximo
    {
        closedir2();
        return -1;              //   do campo `name` para registro.
    }
    strcpy(registro.name, linkname);


    // Selecao de um i-node para o arquivo
    int indice_inode;
    if((indice_inode = searchBitmap2(BITMAP_INODE, 0)) < 0)  // Se retorno da funcao for negativo, deu erro
    {
        closedir2();
        return -1;
    }

    /// inicializacao i-node
    struct t2fs_inode inode;
    inode.blocksFileSize = 1;
    inode.bytesFileSize = sizeof(filename);
    inode.dataPtr[0] = indice_bloco;
    inode.dataPtr[1] = -1;
    inode.singleIndPtr = -1;
    inode.doubleIndPtr = -1;
    inode.RefCounter = 1;

    registro.inodeNumber = indice_inode;

    /// Escrita do i-node no disco
    int inicio_area_inodes = TAM_SUPERBLOCO + superbloco_montado.freeBlocksBitmapSize + superbloco_montado.freeInodeBitmapSize;
    int setor_inicio_area_inodes = inicio_area_inodes * superbloco_montado.blockSize;
    int inodes_por_setor = TAM_SETOR / sizeof(struct t2fs_inode);
    int setor_novo_inode = setor_inicio_area_inodes + (indice_inode / inodes_por_setor);
    int end_novo_inode = indice_inode % inodes_por_setor;

    if(read_sector(base + setor_novo_inode, buffer))
    {
        closedir2();
        return -1;
    }
    memcpy(&buffer[end_novo_inode*sizeof(struct t2fs_inode)], &inode, sizeof(struct t2fs_inode));
    if(write_sector(base + setor_novo_inode, buffer))
    {
        closedir2();
        return -1;
    }

    if(setBitmap2(BITMAP_INODE, indice_inode, 1))
    {
        closedir2();
        return -1;
    }
    if(setBitmap2(BITMAP_DADOS, indice_bloco, 1))
    {
        setBitmap2(BITMAP_INODE, indice_inode, 0);
        closedir2();
        return -1;
    }

    // Adiciona registro na lista de registros
    arquivos_diretorio = insert_element(arquivos_diretorio, registro);

    if(closedir2())
    {
        return -1;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (hardlink)
-----------------------------------------------------------------------------*/
int hln2(char *linkname, char *filename)
{

    if(!tem_particao_montada)
    {
        return -1;
    }

    if(opendir2())
    {
        return -1;
    }

    // Verifica se ja nao existe um arquivo com mesmo nome que o link
    if(contains(arquivos_diretorio, linkname))
    {
        closedir2();
        return -1;
    }

    // Verifica se o arquivo referenciado existe
    if(!contains(arquivos_diretorio, filename))
    {
        closedir2();
        return -1;
    }

    /// T2FS Record
    struct t2fs_record registro_hardlink;
    registro_hardlink.TypeVal = TYPEVAL_REGULAR; // Hard link eh um registro regular
    if (strlen(linkname) > 50)  // Caso o nome passado como parametro seja maior que o tamanho máximo
    {
        closedir2();
        return -1;              //   do campo `name` para registro.
    }
    strcpy(registro_hardlink.name, linkname);

    // Identificacao do i-node do arquivo "original"
    struct t2fs_record registro;
    if(get_element(arquivos_diretorio, filename, &registro))
    {
        closedir2();
        return -1;
    }
    int indice_inode = registro.inodeNumber;
    struct t2fs_inode inode;

    // Leitura do i-node no disco
    int inicio_area_inodes = TAM_SUPERBLOCO + superbloco_montado.freeBlocksBitmapSize + superbloco_montado.freeInodeBitmapSize;
    int setor_inicio_area_inodes = inicio_area_inodes * superbloco_montado.blockSize;
    int inodes_por_setor = TAM_SETOR / sizeof(struct t2fs_inode);
    int setor_inode = setor_inicio_area_inodes + (indice_inode / inodes_por_setor);
    int end_inode = indice_inode % inodes_por_setor;

    unsigned char buffer[TAM_SETOR];
    if(read_sector(base + setor_inode, buffer))
    {
        closedir2();
        return -1;
    }
    memcpy(&inode, &buffer[end_inode*sizeof(struct t2fs_inode)], sizeof(struct t2fs_inode));

    // Incrementa contador de referencia. Inode do hardlink eh o mesmo que o inode do arquivo referenciado
    inode.RefCounter++;
    registro_hardlink.inodeNumber = indice_inode;
    arquivos_diretorio = insert_element(arquivos_diretorio, registro_hardlink);

    if(closedir2())
    {
        return -1;
    }

    return 0;
}
