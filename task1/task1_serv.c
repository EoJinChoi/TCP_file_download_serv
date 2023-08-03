//https://blog.opid.kr/402

/*
TCP 기반 파일 다운로드 프로그램 구현
• 1. 클라이언트가 서버에 접속 (TCP 이용)
• 2.서버프로그램이실행중인디렉토리의모든파일목록(파일이름, 파일 크기)을 클라이언트에게 전송
• 3.클라이언트는서버가보내온목록을보고파일하나를선택
• 4. 서버는 클라이언트가 선택한 파일을 클라이언트에게 전송
• 5. 전송된 파일은 클라이언트 프로그램이 실행 중인 디렉토리에 동일한 이름으로 저장됨.
• 6. 2~5번 과정 반복
• 사용자Interface는자유롭게해도됨.단,사용하기 쉽도록 메뉴나 명령어에 대한 설명 필요
• 텍스트 파일 뿐만 아니라 바이너리 파일도 전송할 수 있어야 함
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>

#define BUF_SIZE 1024
void error_handling(char *message);

typedef struct {
    char fileName[BUF_SIZE];
    char fileSize[BUF_SIZE];
    char fileContent[BUF_SIZE];
    int size;
} pkt_t;
pkt_t *send_pkt;

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    FILE *file;
    char buf[BUF_SIZE];
    char file_name[BUF_SIZE];
    int read_cnt;
    int menu;
    DIR* dir;
    struct dirent* entry;
    char msg[] = "end";

    send_pkt = (pkt_t *) malloc(sizeof(pkt_t));
    memset(send_pkt, 0, sizeof(pkt_t));

    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;

    if(argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    clnt_addr_size = sizeof(clnt_addr);
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
    if(clnt_sock == -1)
        error_handling("accept() error");


    char* folderPath = ".";

    while(1)
    { 
        dir = opendir(".");
        if(dir == NULL) {
            printf("Failed to open directory.\n");
        }

        //폴더 내 파일 목록 write
        while((entry = readdir(dir)) != NULL) {
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char name[BUF_SIZE];
            snprintf(name, sizeof(name), "%s", entry->d_name);
            printf("%s\n", name);
            strcpy(send_pkt->fileName, entry->d_name);

            // 파일 크기 구하기
            file = fopen(entry->d_name, "rb");
            size_t fsize;
            fseek(file, 0, SEEK_END);
            fsize=ftell(file);
            fseek(file, 0, SEEK_SET);
            fclose(file);

            char size[BUF_SIZE];
            snprintf(size, sizeof(size), "%zu", fsize);
            strcpy(send_pkt->fileSize, size);

            strcpy(send_pkt->fileContent, "");

            write(clnt_sock, send_pkt, sizeof(pkt_t));
        }
        strcpy(send_pkt->fileName, msg);
        write(clnt_sock, send_pkt, sizeof(pkt_t));

        // 선택된 파일 이름 read
        read_cnt = read(clnt_sock, file_name, BUF_SIZE);
        if(read_cnt == -1)
            error_handling("read() error");
        if(read_cnt == 0)
            break;

        strcpy(send_pkt->fileName, file_name);
        printf("%s", send_pkt->fileName);

        if(access(folderPath, F_OK) == -1) {
            printf("Directory not found.\n");
            exit(1);
        }

        file = fopen(file_name, "rb");
        if(file == NULL) {
            printf("Failed to open file.\n");
            // continue;
            exit(1);
        }

        size_t fsize, nsize = 0;

        //파일 크기 구하기
        fseek(file, 0, SEEK_END);
        fsize=ftell(file);
        fseek(file, 0, SEEK_SET);

        char size[BUF_SIZE];
        snprintf(size, sizeof(size), "%zu", fsize);
        strcpy(send_pkt->fileSize, size);

        // File write
        while(nsize != fsize)
        {
            int fpsize = fread(buf, 1, BUF_SIZE, file);
            nsize += fpsize;
            
            for(int i = 0; i < fpsize; i++)
                send_pkt->fileContent[i] = buf[i];

            if(fpsize < BUF_SIZE)
            {
                send_pkt->size = fpsize;
                write(clnt_sock, send_pkt, sizeof(pkt_t));
                break;
            }
            
            send_pkt->size = fpsize;
            write(clnt_sock, send_pkt, sizeof(pkt_t));
        }

        fclose(file);
        printf("file send complete\n");
        closedir(dir);
    }
    close(serv_sock);
    close(clnt_sock);

    return 0;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}