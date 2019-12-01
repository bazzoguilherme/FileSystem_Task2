#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "include/t2fs.h"

void lista_dir(){
	//printf("Abre para listar\n");
	if (opendir2() == -1)
		printf("t1: Erro opendir lista_dir\n");

	DIRENT2* d = (DIRENT2*) malloc (sizeof(DIRENT2*));
	if (readdir2(d) == -1){
		printf("t1: Sem arquivos\n");
	} else {
        do {
            printf("~> file: %s\n", d->name);
        }while(readdir2(d) != -1);
	}
	if (closedir2() == -1)
		printf("t1: Erro closedir lista_dir\n");
}

int main(){
{

	printf("t1: Inicio..\n\n");
	getchar();

	printf("\n --t1:  FORMAT\n");
    if (format2(0, 1) == -1){
		printf("t1: Erro FORMAT\n");
	}

	printf("t1: format done\n");
    getchar();


	printf("\n --t1:  MOUNT\n");
    if (mount(0) == -1){
		printf("t1: Erro MOUNT\n");
	}

	printf("t1: mount done\n");
	getchar();

	printf("Printa diretorio de nova paricao\n");
	getchar();
	lista_dir();
	getchar();

    char buffer_grande[20000] = {0};
    char str[22] = "Frase de 22 caracteres\n";
    strcpy(buffer_grande, str);
    int i=0;
    for (i=0;i<40;i++){
        strcat(buffer_grande, str);
    }
    strcat(buffer_grande, "FIM");

    printf("t1: cria arquivo\n");
    getchar();
    FILE2 file1 = create2("sisop.txt");
    printf("Arquivo criado com id: %d\n", file1);
    getchar();
    char buffer[256] = {"Texto teste"};
    printf("Write >> %d\n", write2(file1, buffer_grande, strlen(buffer_grande)));
    getchar();
    close2(file1);
    file1 = open2("sisop.txt");
    char buffer_leitura[906] = {0};
    int lidos = read2(file1, buffer_leitura, 905);
    close2(file1);
    printf("Read >> %d %s\n", lidos, buffer_leitura);
    getchar();
    if (hln2("hardlink.txt", "sisop.txt") == 0){
        FILE2 hl = open2("hardlink.txt");
        char buffer_leitura_hl[906] = {0};
        int lidos_hl = read2(hl, buffer_leitura_hl, 905);
        printf("Read_hl >> %d %s\n", lidos_hl, buffer_leitura_hl);
        close2(hl);
    }
    getchar();
    if (sln2("soft.txt", "sisop.txt") == 0){
        FILE2 sl = open2("soft.txt");
        char buffer_leitura_sl[906] = {0};
        int lidos_sl = read2(sl, buffer_leitura_sl, 905);
        printf("Read_sl >> %d %s\n", lidos_sl, buffer_leitura_sl);
        close2(sl);
    }

    printf("Listando diretorio");
    getchar();
    lista_dir();
    printf("\nAgora deletando arq hardlink\nJa verificado que deletando original o hardlink funciona!\n");
    getchar();
    delete2("hardlink.txt");

    lista_dir();

    printf("Tenta printar conteudo hardlink\n");
    getchar();
    FILE2 hl = open2("hardlink.txt");
    {
        char buffer_leitura_hl[906] = {0};
        int lidos_hl = read2(hl, buffer_leitura_hl, 905);
        printf("Read_hl >> %d %s\n", lidos_hl, buffer_leitura_hl);
        close2(hl);
    }
    getchar();
    printf("Tenta printar conteudo softlink\n");
    getchar();
    {
        FILE2 sl = open2("soft.txt");
        char buffer_leitura_sl[906] = {0};
        int lidos_sl = read2(sl, buffer_leitura_sl, 905);
        printf("Read_sl >> %d %s\n", lidos_sl, buffer_leitura_sl);
        close2(sl);
    }

    printf("Abrindo arquivo e escrevendo novamente nele\n");
    getchar();
    file1 = open2("sisop.txt");
    char escrita_nova[200] = "Novo texto sendo inserido em arquivo";
    printf("Escrita nova: %d\n", write2(file1, escrita_nova, strlen(escrita_nova)));
    getchar();
    close2(file1);
    file1 = open2("sisop.txt");
    lidos = read2(file1, buffer_leitura, 200);
    close2(file1);
    printf("Leitura: %d %s\n", lidos, buffer_leitura);

    printf("Umount : %d\n\n", umount());
    getchar();
}
//--------------------------------------------------------------------------------------------------
{
    printf("t1: Inicio..\n\n");
	getchar();

	printf("\n --t1:  FORMAT\n");
    if (format2(1, 1) == -1){
		printf("t1: Erro FORMAT\n");
	}

	printf("t1: format done\n");
    getchar();


	printf("\n --t1:  MOUNT\n");
    if (mount(1) == -1){
		printf("t1: Erro MOUNT\n");
	}

	printf("t1: mount done\n");
	getchar();

	printf("Printa diretorio de nova paricao\n");
	getchar();
	lista_dir();
	getchar();

    char buffer_grande[20000] = {0};
    char str[22] = "Frase de 22 caracteres\n";
    strcpy(buffer_grande, str);
    int i=0;
    for (i=0;i<40;i++){
        strcat(buffer_grande, str);
    }
    strcat(buffer_grande, "FIM");

    printf("t1: cria arquivo\n");
    getchar();
    FILE2 file1 = create2("sisop.txt");
    printf("Arquivo criado com id: %d\n", file1);
    getchar();
    char buffer[256] = {"Texto teste"};
    printf("Write >> %d\n", write2(file1, buffer_grande, strlen(buffer_grande)));
    getchar();
    close2(file1);
    file1 = open2("sisop.txt");
    char buffer_leitura[906] = {0};
    int lidos = read2(file1, buffer_leitura, 905);
    close2(file1);
    printf("Read >> %d %s\n", lidos, buffer_leitura);
    getchar();
    if (hln2("hardlink.txt", "sisop.txt") == 0){
        FILE2 hl = open2("hardlink.txt");
        char buffer_leitura_hl[906] = {0};
        int lidos_hl = read2(hl, buffer_leitura_hl, 905);
        printf("Read_hl >> %d %s\n", lidos_hl, buffer_leitura_hl);
        close2(hl);
    }
    getchar();
    if (sln2("soft.txt", "sisop.txt") == 0){
        FILE2 sl = open2("soft.txt");
        char buffer_leitura_sl[906] = {0};
        int lidos_sl = read2(sl, buffer_leitura_sl, 905);
        printf("Read_sl >> %d %s\n", lidos_sl, buffer_leitura_sl);
        close2(sl);
    }

    printf("Listando diretorio");
    getchar();
    lista_dir();
    printf("\nAgora deletando arq hardlink\nJa verificado que deletando original o hardlink funciona!\n");
    getchar();
    delete2("hardlink.txt");

    lista_dir();

    printf("Tenta printar conteudo hardlink\n");
    getchar();
    FILE2 hl = open2("hardlink.txt");
    {
        char buffer_leitura_hl[906] = {0};
        int lidos_hl = read2(hl, buffer_leitura_hl, 905);
        printf("Read_hl >> %d %s\n", lidos_hl, buffer_leitura_hl);
        close2(hl);
    }
    getchar();
    printf("Tenta printar conteudo softlink\n");
    getchar();
    {
        FILE2 sl = open2("soft.txt");
        char buffer_leitura_sl[906] = {0};
        int lidos_sl = read2(sl, buffer_leitura_sl, 905);
        printf("Read_sl >> %d %s\n", lidos_sl, buffer_leitura_sl);
        close2(sl);
    }

    printf("Abrindo arquivo e escrevendo novamente nele\n");
    getchar();
    file1 = open2("sisop.txt");
    char escrita_nova[200] = "Novo texto sendo inserido em arquivo";
    printf("Escrita nova: %d\n", write2(file1, escrita_nova, strlen(escrita_nova)));
    getchar();
    close2(file1);
    file1 = open2("sisop.txt");
    lidos = read2(file1, buffer_leitura, 200);
    close2(file1);
    printf("Leitura: %d %s\n", lidos, buffer_leitura);

    getchar();
    printf("Tentando criar outro arquivo com mesmo nome\n");
    getchar();
    FILE2 new_file = create2("sisop.txt");
    printf("Criacao de mesmo nome : %d\n", new_file);
    close2(new_file);
    new_file = open2("sisop.txt");
    printf("Escrita nova: %d\n", write2(new_file, escrita_nova, strlen(escrita_nova)));
    getchar();
    close2(new_file);
    new_file = open2("sisop.txt");
    lidos = read2(new_file, buffer_leitura, 200);
    close2(new_file);
    printf("Leitura: %d %s\n", lidos, buffer_leitura);

    getchar();
    printf("Criacao de varios arquivos.. testando a criacao deles e handle de arquivos abertos\n");
    getchar();

    printf("\nnew_file : %d\n", create2("taylor.com"));
    printf("new_file : %d\n", create2("file.json"));
    printf("new_file : %d\n", create2("sos.socorro"));
    printf("new_file : %d\n", create2("new.tzt"));
    printf("new_file : %d\n", create2("teste.xml"));
    printf("new_file : %d\n", create2("mais_files.md"));
    printf("new_file : %d\n", create2("arq.txt"));
    printf("new_file : %d\n", create2("musica.zip"));
    printf("new_file : %d\n", create2("backup.bk"));
    printf("new_file : %d\n", create2("mais.txt"));
    printf("new_file : %d\n", create2("fim.final"));

    getchar();
    printf("Deletando alguns para dar espaço para criar mais\n");
    getchar();

    //close2();
    printf("no_file : %d\n", delete2("teste.xml"));
    return 0;
    printf("no_file : %d\n", delete2("mais_files.md"));
    printf("no_file : %d\n", delete2("arq.txt"));

    printf("Cria final : %d\n", create2("fim.final"));

}
    return 0;
}

