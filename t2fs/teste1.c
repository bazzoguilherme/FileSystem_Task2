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


}
    return 0;

    printf("t1 Printa dir e cria alguns arquivos\n");
    getchar();
	lista_dir();
	getchar();
	printf("--t1: Cria arq 1\n");
	getchar();
    printf("t1: Arq1: %d\n", create2("arq1.txt"));
    opendir2();
    printf("t1:PARA AQUI\n");
    getchar();
//	lista_dir();
//	getchar();
	printf("--t1: Cria arq 2\n");
	getchar();
	printf("t1: Arq2: %d\n", create2("arq2.txt"));
	return 0;
    create2("Foto.jpg");
	lista_dir();
    create2("TrabalhoSisop.c");
    create2("Senhas.txt");
    create2("Minecraft.exe");
    create2("Cogumelo.exe");
    create2("Sisop2.exe");
    create2("BYOB.mp3");
    create2("Novo Documento.txt");
    //create2("Receita de Miojo.txt");
    //create2("Notas da P2.pdf");
	lista_dir();
	getchar();

    hln2("linktop.txt", "Senhas.txt");

    //sln2("linktop.txt", "Senhas.txt");

    DIRENT2 de;

    //write2(3, buffer, szb);
    close2(0);
    close2(1);
    close2(2);
    close2(3);
    close2(4);
    close2(5);
    close2(6);
    close2(7);
    close2(8);
    close2(9);
    //close(10);

    closedir2();

    //strcpy(buffer, "Espalha rama pelo chao.");

    opendir2();

    //delete2("Senhas.txt");
    //delete2("linktop.txt");

    while(!readdir2(&de)){
         printf("Nome: %s\nTamanho: %d Bytes\n\n", de.name, de.fileSize);
    }

//    FILE2 hand0 = open2("Senhas.txt");
//    FILE2 hand1 = open2("linktop.txt");
//    printf("");

//    strcpy(buffer, "AAAAA");
//    for(i = 0; i < 500; i++)
//       strcat(buffer, "BBBB");
//
//    write2(hand0, buffer, strlen(buffer));
//    close2(hand0);
//    printf("Buffer2: %s\n", buffer2);
//
//    read2(hand1, buffer3, szb);
//    close2(hand1);
//    printf("Buffer3: %s\n", buffer3);
//
//    closedir2();

    umount();
    return 0;
}

