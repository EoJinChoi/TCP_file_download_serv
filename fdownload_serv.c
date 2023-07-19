//https://blog.opid.kr/402

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>

#define BUF_SIZE 1024
void error_handling(char *message);

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


    char* folderPath = "/Users/eojinchoi/Desktop/VMshare/file_test";

    while(1)
    { 
        dir = opendir(folderPath);
        if(dir == NULL) {
            printf("Failed to open directory.\n");
        }

        //폴더 내 파일 write
        while((entry = readdir(dir)) != NULL) {
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            printf("%s\n", entry->d_name);
            int a = write(clnt_sock, entry->d_name, sizeof(entry->d_name));
            if(a == -1)
                error_handling("write() error");
        }
        write(clnt_sock, msg, sizeof(msg));
        // printf("send file list\n");

        // 선택된 파일 이름 read
        read_cnt = read(clnt_sock, file_name, BUF_SIZE);
        if(read_cnt == -1)
            error_handling("read() error");

        if(access(folderPath, F_OK) == -1) {
            printf("Directory not found.\n");
            exit(1);
        }

        file = fopen(file_name, "rb");
        if(file == NULL) {
            printf("Failed to open file.\n");
            continue;
            // exit(1);
        }

        size_t fsize, nsize = 0;

        //파일 크기 구하기
        fseek(file, 0, SEEK_END);
        fsize=ftell(file);
        fseek(file, 0, SEEK_SET);

        size_t size = htonl(fsize);
        write(clnt_sock, &size, sizeof(fsize)); // 파일 크기 전송

        while(nsize != fsize)
        {
            int fpsize = fread(buf, 1, BUF_SIZE, file);
            nsize += fpsize;
            write(clnt_sock, buf, fpsize);
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