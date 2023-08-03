/*
Simple Remote Shell 프로그램 구현
• 클라이언트가 서버에 접속하여 서버의 디렉토리와 파일 정보를 확인할 수 있는 프로그램을 작성하세요.
• 클라이언트가 서버에 접속하면, 서버는 서버프로그램이 실행되고 있는 디렉토리 위치 및 파일 정보(파일명 + 파일 크기)를 클라이언트에게 전달합니다.
• 클라이언트는 서버로부터 수신한 정보들을 출력합니다.
• 클라이언트는 서버의 디렉토리를 변경해가며 디렉토리 정보와 디렉토리 안에 있는 파일 정보(파일명 + 파일크기)를 확인할 수 있습니다.
• 클라이언트가 원하는 파일을 서버로부터 다운로드받을 수 있어야 합니다.
• 클라이언트는 자신이 소유한 파일을 서버에게 업로드할 수 있어야 합니다.
• 클라이언트가 서버에 접속하면 클라이언트는 서버프로그램의 권한과 동일한 권한을 갖습니다. 즉, 서버 프로그램에게 허용되지 않은 디렉토리와 파일은 클라이언트도 접근할 수 없습니다.
• 서버는 여러 클라이언트의 요청을 동시에 지원할 수 있어야 합니다. →I/O Multiplexing 사용!
• Makefile을 만들어서 컴파일할 수 있어야 합니다.
*/

#define _XOPEN_SOURCE 200

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/select.h>

#define BUF_SIZE 1024
#define PATH_MAX 1024
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
    struct sockaddr_in serv_adr, clnt_adr;
    struct timeval timeout;
    fd_set reads, cpy_reads;

    int fd_max, fd_num;
    socklen_t adr_sz;
    int str_len;
    char buf[BUF_SIZE];
    char file_name[BUF_SIZE];
    DIR* dir;
    struct dirent* entry;
    FILE *file;
    char msg[] = "end";

    send_pkt = (pkt_t *) malloc(sizeof(pkt_t));
    memset(send_pkt, 0, sizeof(pkt_t));

    if(argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    FD_ZERO(&reads);
    FD_SET(serv_sock, &reads);
    fd_max = serv_sock;

    char path[PATH_MAX];

    if ( getcwd(path, PATH_MAX) == NULL ) {
        fprintf(stderr, "Error: getcwd() cannot execute\n") ;
        exit(1); 
    } 

    while(1)
    {
        cpy_reads = reads;
        timeout.tv_sec = 5;
        timeout.tv_usec = 5000;

        if((fd_num = select(fd_max + 1, &cpy_reads, 0, 0, &timeout)) == -1)
            break;
        if(fd_num ==  0)
            continue;

        for(int i = 0; i < fd_max+1; i++)
        {
            if(FD_ISSET(i, &cpy_reads))
            {
                printf("int %d\n", i);
                printf("hi\n");
                if( i == serv_sock)
                {
                    adr_sz = sizeof(clnt_adr);
                    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);

                    FD_SET(clnt_sock, &reads);
                    if(fd_max < clnt_sock)
                        fd_max = clnt_sock;
                    printf("Connected client: %d \n", clnt_sock);
                }
                else
                {
                    str_len = recv(i, buf, BUF_SIZE, MSG_PEEK);
                    if(str_len == 0)
                    {
                        FD_CLR(i, &reads);
                        close(i);
                        printf("Closed client: %d \n", i);
                        continue;
                    }

                    int read_num;

                    while(1)
                    {
                        read_num = recv(i, buf, BUF_SIZE, MSG_PEEK);
                        if(read_num == BUF_SIZE)
                            break;
                        if(read_num == 0)
                            break;
                    }
                    if(read_num == 0)
                    {
                        FD_CLR(i, &reads);
                        close(i);
                        printf("Closed client: %d \n", i);
                        continue;
                    }

                    recv(i, buf, BUF_SIZE, 0);

                    printf("1. %s\n", path);
                    write(i, path, sizeof(path));
                    dir = opendir(path);//
                    if(dir == NULL) {
                        printf("Failed to open directory.\n");
                    }

                    //폴더 내 파일 목록 write
                    while((entry = readdir(dir)) != NULL) {
                        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                            continue;

                        char name[BUF_SIZE];
                        snprintf(name, sizeof(name), "%s", entry->d_name);
                        strcpy(send_pkt->fileName, name);
                        printf("%s   ", send_pkt->fileName);

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
                        printf("%s bytes\n", send_pkt->fileSize);

                        strcpy(send_pkt->fileContent, "");

                        write(i, send_pkt, sizeof(pkt_t));
                    }
                    strcpy(send_pkt->fileName, msg);
                    write(i, send_pkt, sizeof(pkt_t));

                    int num;
                    int read_cnt = read(i, &num, sizeof(int));
                    puts(buf);

                    // 1. Change directory-----------------
                    if(num == 1)
                    {
                        char tmp[BUF_SIZE];
                        read(i, tmp, BUF_SIZE);
                        printf("fp: %s\n", path);
                        closedir(dir);

                        if(access(tmp, F_OK) == -1) {
                            printf("Directory not found.\n");   
                        }

                        else {
                            int ch = chdir(tmp); 
                            if( ch == 0 )
                            {
                                printf("Change directory\n") ; // 디렉토리 이동 성공 
                                strcpy(path, tmp);
                            }
                            else
                                printf("Failed change directory!\n") ; // 디렉토리 이동 실패 
                                
                            continue;
                        }
                    }

                    // 2. Download-------------------------
                    else if(num == 2)
                    {
                        printf("2. %s\n", path);
                        read_cnt = read(i, file_name, BUF_SIZE);
                        if(read_cnt == -1)
                            error_handling("read() error");
                        if(read_cnt == 0)
                            break;

                        strcpy(send_pkt->fileName, file_name);
                        printf("%s", send_pkt->fileName);

                        if(access(path, F_OK) == -1) {
                            printf("Directory not found.\n");
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
                                write(i, send_pkt, sizeof(pkt_t));
                                break;
                            }
                                        
                            send_pkt->size = fpsize;
                            write(i, send_pkt, sizeof(pkt_t));
                        }

                        fclose(file);
                        printf("file send complete\n");
                    }

                    // 3. Upload------------------------------
                    else if(num == 3)
                    {
                        // 파일 수신
                        read(i, file_name, BUF_SIZE);
                                
                        file = fopen(file_name, "wb");
                        if(file == NULL) {
                            printf("Failed to create file.\n");
                            continue;
                        }
                        while(1)
                        {
                            while(1)
                            {
                                read_cnt = recv(i, send_pkt, sizeof(pkt_t), MSG_PEEK);
                                if(read_cnt == sizeof(pkt_t))
                                    break;

                                printf("\nsize: %d\n", read_cnt);
                            }
                            recv(i, send_pkt, sizeof(pkt_t), 0);
                            fwrite(send_pkt->fileContent, 1, send_pkt->size, file);

                            if(send_pkt->size < BUF_SIZE)
                                break;
                        }

                        fclose(file);
                        printf("receive complete\n");
                    }

                    // Quit----------------------------------
                    else if(num == 4)
                        break;

                    closedir(dir);
                }
            }
        }
    }
    close(serv_sock);
    return 0;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}