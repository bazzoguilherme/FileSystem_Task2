
/**
*/
#include "../include/t2fs.h"
#include "../include/apidisk.h"
#include "../include/t2disk.h"
#include "../include/bitmap2.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define TAM_SUPERBLOCO 1 // Tamanho do superbloco em blocos
#define MAX_OPEN_FILE 10
#define TAM_SETOR 256    // Tamanho de um setor em bytes


typedef struct file_t2fs {
    char filename[MAX_FILE_NAME_SIZE+1];
    int current_pointer;
}FILE_T2FS;

typedef struct linked_list {
    struct t2fs_record registro;
    struct linked_list* next;
} Linked_List;

/// Funcoes Linked-List

/*-----------------------------------------------------------------------------
Inicialização de lista encadeada
-----------------------------------------------------------------------------*/
Linked_List* create_linked_list(){
    return NULL;
}

/*-----------------------------------------------------------------------------
Insercao de elemento ao final da lista encadeada
-----------------------------------------------------------------------------*/
Linked_List* insert_element(Linked_List* list, struct t2fs_record registro_) {
    Linked_List* new_node = (Linked_List*)malloc(sizeof(Linked_List));
    new_node->registro = registro_;
    new_node->next = NULL;

    if (list == NULL){
        return new_node;
    }

    Linked_List* aux = list;
    while (aux->next != NULL){
        aux = aux->next;
    }
    aux->next = new_node;

    return list;
}

/*-----------------------------------------------------------------------------
Coletade elemento da lista - int indica se ocorreu certo
    0 - okay
   -1 - erro
-----------------------------------------------------------------------------*/
int get_element(Linked_List* list, char* nome_registro_, struct t2fs_record* registro){
    Linked_List* aux = list;
    while (aux!=NULL){
        if(strcmp(aux->registro.name, nome_registro_) == 0){
            registro = &aux->registro;
            return 0;
        }
        aux = aux->next;
    }

    return -1;
}

/*-----------------------------------------------------------------------------
Deleta um elemento da lista encadeada
-----------------------------------------------------------------------------*/
Linked_List* delete_element(Linked_List* list, char* nome_registro_){
    Linked_List* aux = NULL;
    Linked_List* freed_node = list;

    if (list == NULL){
        return NULL;
    }

    if (strcmp(list->registro.name, nome_registro_) == 0){
        aux = list->next;
        free(list);
        return aux;
    }

    while(freed_node->next!=NULL && strcmp(freed_node->registro.name, nome_registro_) != 0 ){
        aux = freed_node;
        freed_node = freed_node->next;
    }
    if (strcmp(freed_node->registro.name, nome_registro_) == 0){
        aux->next = freed_node->next;
        free(freed_node);
    }

    return list;
}

/*-----------------------------------------------------------------------------
Verificacao se existe um dado elemento (nome) no diretorio
-----------------------------------------------------------------------------*/
boolean contains(Linked_List* list, char* nome_registro_){
    Linked_List* aux = list;
    while (aux!=NULL){
        if(strcmp(aux->registro.name, nome_registro_) == 0){
            return true;
        }
        aux = aux->next;
    }
    return false;
}


/// VARIAVEIS GLOBAIS
struct t2fs_superbloco superbloco_montado; // Variavel que guarda as informacoes do superbloco da particao montada
boolean tem_particao_montada = false; // Indica se tem alguma particao ja montada
FILE_T2FS open_files[MAX_OPEN_FILE] = {}; // Tabela de arquivos abertos (Maximo 10 por vez)
Linked_List* arquivos_diretorio = NULL; // Lista de arquivos do diretorio

/// FUNCOES AUXILIARES
/*-----------------------------------------------------------------------------
Le do buffer, a partir do endereço especificado, a quantidade de bytes informada.
Retorna um int com os valores do buffer, considerando que os dados estao em little-endian
-----------------------------------------------------------------------------*/
int getDado(unsigned char buffer[], int end_inicio, int qtd){
    int i;
    int dado = buffer[end_inicio+qtd-1]; // Dados em little-endian

    for(i=qtd-2; i>=0; i--){
        dado <<= 8;
        dado |= buffer[end_inicio+i];
    }

    return dado;
}

/*-----------------------------------------------------------------------------
Calculo do checksum de um superbloco
-----------------------------------------------------------------------------*/
unsigned int checksum(struct t2fs_superbloco *superbloco){
	unsigned int bytes_iniciais[5];
	memcpy(bytes_iniciais, superbloco, 20); // 20 = numero de bytes a serem copiados
	unsigned int checksum = 0;
	int i=0;
	for(i=0; i<5; i++){
        checksum += bytes_iniciais[i];
	}
	checksum = !checksum; // Complemento de 1
	return checksum;
}

/*-----------------------------------------------------------------------------
Procuna nos arquivos abertos se há um arquivo de nome 'filename'
    Retorno:
        - True: caso arquivo encontrado
        - False: caso contrario
-----------------------------------------------------------------------------*/
boolean open_files_contem_arquivo(char* filename){
	int i = 0;
	for (i=0; i<MAX_OPEN_FILE; i++){
        if (open_files[i].current_pointer != -1 && strcmp(open_files[i].filename, filename) == 0){
            return true;
        }
	}
	return false;
}

/// FUNCOES DA BIBLIOTECA
/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size) {
	char alunos[] = "Guilherme B. B. O. Malta - 00278237\nGuilherme T. Bazzo - 00287687\nNicolas C. Duranti - 00287679\n";
	// Caso o nome dos alunos (com seus respectivos numeros de matricula) nao caibam no espaco dado, retorna erro
	if(strlen(alunos) >= size){
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
int format2(int partition, int sectors_per_block) {

	unsigned char buffer[256];
	int i;

	if(read_sector(0, buffer)){ // Le o MBR, localizado no primeiro setor
        return -1;
	}

	int num_particoes = getDado(buffer, 6, 2);
	if(partition < 0 || partition >= num_particoes){ // Verifica se a particao é valida (entre 0 e num-1)
        return -1;
	}

	int tam_setor = getDado(buffer, 2, 2);
	int setor_inicio = getDado(buffer, 8 + 24*partition, 4);
	int setor_fim = getDado(buffer, 12 + 24*partition, 4);
	int num_setores = setor_fim - setor_inicio;
	int num_blocos = num_setores / sectors_per_block;
	int num_blocos_inodes = ceil(0.1 * num_blocos);

    int tam_bloco = sectors_per_block * tam_setor; // Em bytes
    int num_inodes = num_blocos_inodes * tam_bloco / sizeof(struct t2fs_inode);
    int num_blocos_bitmap_inode = ceil(num_inodes / tam_bloco);
    int num_blocos_bitmap_blocos = ceil(num_blocos / tam_bloco);

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


	if(write_sector(setor_inicio, (unsigned char *)&superbloco)){
        return -1;
	}



	/// Inicializacao dos Bitmaps
	if(openBitmap2(setor_inicio)){
        return -1;
	}

	// Zera o bitmap de inodes
	do{
        if((i = searchBitmap2(BITMAP_INODE, 1)) < 0){ // Se retorno da funcao for negativo, deu erro
            return -1;
        }
        if(i){  // Se i for positivo, achou bit com valor 1
            if(setBitmap2(BITMAP_INODE, i, 0)){
                return -1;
            }
        }
	}while(i);


    // Zera o bitmap de dados
	do{
        if((i = searchBitmap2(BITMAP_DADOS, 1)) < 0){ // Se retorno da funcao for negativo, deu erro
            return -1;
        }
        if(i){  // Se i for positivo, achou bit com valor 1
            if(setBitmap2(BITMAP_DADOS, i, 0)){
                return -1;
            }
        }
	}while(i);

	// Marca os blocos onde estao o superbloco e os bitmaps como ocupados
	for(i=0; i < TAM_SUPERBLOCO + num_blocos_bitmap_blocos + num_blocos_bitmap_inode + num_inodes; i++){
        if(setBitmap2(BITMAP_DADOS, i, 1)){
            return -1;
        }
	}


	/// Inicialização do i-node do diretorio raiz (i-node 0)
	struct t2fs_inode inode_raiz;
    inode_raiz.blocksFileSize = 0;
	inode_raiz.bytesFileSize = 0;
	inode_raiz.dataPtr[0] = -1;
	inode_raiz.dataPtr[1] = -1;
	inode_raiz.singleIndPtr = -1;
	inode_raiz.doubleIndPtr = -1;
	inode_raiz.RefCounter = 1;

	// Bloco de inicio da area de inodes
	int inicio_area_inodes = TAM_SUPERBLOCO + num_blocos_bitmap_blocos + num_blocos_bitmap_inode;
    if(write_sector(inicio_area_inodes, (unsigned char *)&inode_raiz)){
        return -1;
	}

    if(setBitmap2(BITMAP_INODE, 0, 1)){ // Marca o bit do i-node como ocupado
        return -1;
    }

	if(closeBitmap2()){
        return -1;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Monta a partição indicada por "partition" no diretório raiz
-----------------------------------------------------------------------------*/
int mount(int partition) {

    unsigned char buffer[256];
    int i;

    if(tem_particao_montada){
        return -1;
    }

    if(read_sector(0, buffer)){ // Le o MBR, localizado no primeiro setor
        return -1;
	}

    int num_particoes = getDado(buffer, 6, 2);
	if(partition < 0 || partition >= num_particoes){ // Verifica se a particao é valida (entre 0 e num-1)
        return -1;
	}

	int setor_inicio = getDado(buffer, 8 + 24*partition, 4);
	if(read_sector(setor_inicio, buffer)){ // Le o superbloco da particao
        return -1;
	}
	memcpy(&superbloco_montado, buffer, sizeof(struct t2fs_superbloco));

	// Verifica se o superbloco foi formatado
	if(!(checksum(&superbloco_montado) == superbloco_montado.Checksum)){
		return -1;
	}

    tem_particao_montada = true;

    for(i=0; i<MAX_OPEN_FILE; i++){ // Inicializa/Limpa a tabela de arquivos abertos
        open_files[i].current_pointer = -1;
    }

    if(openBitmap2(setor_inicio)){  // Abre os bitmaps da particao montada
        return -1;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Desmonta a partição atualmente montada, liberando o ponto de montagem.
-----------------------------------------------------------------------------*/
int unmount(void) {

    if(closeBitmap2()){ // Fecha os bitmaps da particao
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
FILE2 create2 (char *filename) {

    if(!tem_particao_montada){
        return -1;
    }

    if (!filename){ // Caso filename seja NULL retorna -1
        return -1;
    }
    if (filename[0] == '\0'){ // Caso filename seja um nome vazio retorna -1
        return -1;
    }

    // TODO: procurar se há algum arquivo no disco com mesmo nome.
        // Caso já existir, remove o conteúdo, assumindo tamanho de zero bytes.
    if (contains(arquivos_diretorio, filename)){
        if (delete2(filename) != 0){
            return -1;
        }
    }



    /// T2FS Record
    struct t2fs_record created_file_record;
    created_file_record.TypeVal = TYPEVAL_REGULAR; // Registro regular
    if (strlen(filename) > 50){ // Caso o nome passado como parametro seja maior que o tamanho máximo
        return -1;              //   do campo `name` para registro.
    }
    strcpy(created_file_record.name, filename);


    // Selecao de um bloco de dados para o arquivo
    int indice_bloco_dados;
    if((indice_bloco_dados = searchBitmap2(BITMAP_DADOS, 0)) <= 0){ // Se retorno da funcao for negativo, deu erro
            return -1;                                              // Se for 0, nao ha blocos livres
    }

    // Selecao de um i-node para o arquivo
    int indice_inode;
    if((indice_inode = searchBitmap2(BITMAP_INODE, 0)) <= 0){ // Se retorno da funcao for negativo, deu erro
            return -1;                                        // Se for 0, nao ha blocos livres
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
	if(read_sector(setor_novo_inode, buffer)){
        return -1;
	}
	memcpy(&buffer[end_novo_inode], &new_inode, sizeof(struct t2fs_inode));
	if(write_sector(setor_novo_inode, buffer)){
        return -1;
	}

    if(setBitmap2(BITMAP_INODE, indice_inode, 1)){
        return -1;
    }
    if(setBitmap2(BITMAP_DADOS, indice_bloco_dados, 1)){
        setBitmap2(BITMAP_INODE, indice_inode, 0);
        return -1;
    }

    // TODO: Adiciona registro na lista de registros

    return open2(filename);
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco.
-----------------------------------------------------------------------------*/
int delete2 (char *filename) {

    if(!tem_particao_montada){
        return -1;
    }

    if(open_files_contem_arquivo(filename)){ // Caso o arquivo esteja aberto, retorna erro
        return -1;
    }

    if (!contains(arquivos_diretorio, filename)){ // Caso não haja o arquivo no diretorio, retorna erro
        return -1;
    }
    arquivos_diretorio = delete_element(arquivos_diretorio, filename); // Deleta o arquivo da lista de arquivos do diretorio

    // Seta bitmaps como desocupados (limpa blocos de dados)

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename) {

    if(!tem_particao_montada){
        return -1;
    }

    // TODO - procura arquivo no disco
        // - Le arquivo do disco

    // Procura posicao para inserir arquivo
    int pos_insercao_open_file = 0;
    while ((open_files[pos_insercao_open_file].current_pointer >= 0) && (pos_insercao_open_file < MAX_OPEN_FILE)){
        // Varre array de arquivos abertos para adicionar o novo arquivo a eles.
        pos_insercao_open_file++;
    }

    if (pos_insercao_open_file >= 10){ // Retorna -1 caso já haja 10 ou mais arquivos abertos
        return -1;                      // Isso significa que não há espaço disponível para abrir arquivo
    }

    /// File
    FILE_T2FS new_open_file;
    new_open_file.current_pointer = 0;
    strcpy(new_open_file.filename, filename);

    open_files[pos_insercao_open_file] = new_open_file; // Insere em arquivos abertos

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um arquivo.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle) {

    if(!tem_particao_montada){
        return -1;
    }

    // handle: identificador de arquivo para fechar

	// Caso arquivo já esteja marcado como fechado (current_pointer == -1)
	if (open_files[handle].current_pointer < 0){ // (<= 0)??
        return -1;
	}

	open_files[handle].current_pointer = -1; // Marca arquivo como fechado (current_pointer == -1)

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a leitura de uma certa quantidade
		de bytes (size) de um arquivo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size) {

	if(!tem_particao_montada){
        return -1;
    }

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a escrita de uma certa quantidade
		de bytes (size) de  um arquivo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size) {

	if(!tem_particao_montada){
        return -1;
    }
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um diretório existente no disco.
-----------------------------------------------------------------------------*/
int opendir2 (void) {

	if(!tem_particao_montada){
        return -1;
    }
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2 (DIRENT2 *dentry) {

	if(!tem_particao_montada){
        return -1;
    }
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.
-----------------------------------------------------------------------------*/
int closedir2 (void) {

	if(!tem_particao_montada){
        return -1;
    }
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (softlink)
-----------------------------------------------------------------------------*/
int sln2 (char *linkname, char *filename) {

	if(!tem_particao_montada){
        return -1;
    }
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (hardlink)
-----------------------------------------------------------------------------*/
int hln2(char *linkname, char *filename) {

	if(!tem_particao_montada){
        return -1;
    }
	return -1;
}



