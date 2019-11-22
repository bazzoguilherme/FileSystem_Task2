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

	printf("--t1: Cria arq 1\n");
	getchar();
    printf("t1: Arq1: %d\n", create2("arq1.txt"));
    printf("t1:PARA AQUI\n");
    getchar();
	printf("Gera hard-link");
    if (hln2("link_errado.txt", "arqx.txt") != 0){
		printf("\tok-error: erro gerar hard link\n");
	}
	if (hln2("link_cert.txt", "arq1.txt") != 0){
		printf("\tERRRRRRRRO: erro gerar hard link\n");
	}

	
	printf("Gera soft-link\n");
	getchar();
	if (sln2("soft_errado.txt", "arqx.txt") != 0){
		printf("\tok-error: erro gerar hard link\n");
	}
	if (sln2("soft_cert.txt", "arq1.txt") != 0){
		printf("\tERRRRRRRRO: erro gerar hard link\n");
	}


	printf("Lista-dir\n");
	getchar();
	lista_dir();
	
	return 0;

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

