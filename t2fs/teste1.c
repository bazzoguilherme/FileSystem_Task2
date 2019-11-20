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
    char buffer[20000] = {0};
    char buffer2[20000] = {0};
    char buffer3[20000] = {0};
    int i;

	printf("t1: Inicio..\n\n");
	getchar();

    strcpy(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa (bloco de indices duplo)\n");
    strcat(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa (bloco de indices duplo)\n");
    strcat(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa (bloco de indices duplo)\n");
    strcat(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa (bloco de indices duplo)\n");
    for(i = 0; i < 20; i++)
        strcat(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa (bloco de indices duplo)\n");
    //for(i = 0; i < 40; i++)
    //    strcat(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa... Ta dificil! (bloco de indices duplo)\n");
    //for(i = 0; i < 60; i++)
    //    strcat(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa... Ta dificil mesmo! (bloco de indices duplo)\n");
    //for(i = 0; i < 40; i++)
    //    strcat(buffer, "Agora vai, o bloco duplo vai ser usado, por favor funcione, vai ser muito tri se funcionar (bloco de indices duplo)\n");
    strcat(buffer, "Deu :)\n");

    int szb = strlen(buffer) + 1;

	printf("t1: Buffers padroes (ivan) inicializados\n");
	getchar();

	printf("\n --t1:  FORMAT\n");
    if (format2(0, 1) == -1){
		printf("t1: Erro FORMAT\n");
	}

	printf("t1: format done\n");
	printf("\n --t1:  MOUNT\n");
	getchar();

    if (mount(0) == -1){
		printf("t1: Erro MOUNT\n");
	}

	printf("Lista 2 vzs.\n");
	getchar();
	lista_dir();
	lista_dir();
	getchat();

	printf("t1: mount done\n");
	printf("t1: Listagens e criacao de arquivos\n");
	getchar();

	lista_dir();
	getchar();
	printf("--t1: Cria arq 1\n");
    printf("t1: Arq1: %d\n", create2("arq1.txt"));
    printf("t1:PARA AQUI\n");
    //getchar();
	//lista_dir();
	getchar();
	printf("--t1: Cria arq 2\n");
	printf("t1: Arq2: %d\n", create2("arq2.txt"));
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

    write2(3, buffer, szb);
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

    strcpy(buffer, "Espalha rama pelo chao.");

    opendir2();

    //delete2("Senhas.txt");
    //delete2("linktop.txt");

    while(!readdir2(&de)){
         printf("Nome: %s\nTamanho: %d Bytes\n\n", de.name, de.fileSize);
    }

    FILE2 hand0 = open2("Senhas.txt");
    FILE2 hand1 = open2("linktop.txt");
    printf("");

    strcpy(buffer, "AAAAA");
    for(i = 0; i < 500; i++)
       strcat(buffer, "BBBB");

    write2(hand0, buffer, strlen(buffer));
    close2(hand0);
    printf("Buffer2: %s\n", buffer2);

    read2(hand1, buffer3, szb);
    close2(hand1);
    printf("Buffer3: %s\n", buffer3);

    closedir2();

    umount();
    return 0;
}

