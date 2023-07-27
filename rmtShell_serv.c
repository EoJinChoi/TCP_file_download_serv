#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>

#define BUF_SIZE 1024
void error_handling(char *message);
void read_childproc(int sig);

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;

    pid_t pid;
    struct sigaction act;
    socklen_t adr_sz;
    int str_len, state;
    char buf[BUF_SIZE];
    char file_name[BUF_SIZE];
    DIR* dir;
    struct dirent* entry;
    FILE *file;
    char msg[] = "end";
    int nbyte;
    size_t filesize = 0;

    if(argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    act.sa_handler = read_childproc;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    state = sigaction(SIGCHLD, &act, 0);

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

    char* folderPath = "/Users/eojinchoi/Desktop/VMshare/file_test";
    char fp[] = "/Users/eojinchoi/Desktop/VMshare/file_test";

    // while(1)
    // {
        adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
        // if(clnt_sock == -1)
        //     continue;
        // else
        //     puts("new client connected...");

        pid = fork();
        if(pid == -1)
        {
            close(clnt_sock);
            // continue;
        }
        if(pid == 0)
        {
            close(serv_sock);

            while(1)
            {
                dir = opendir(folderPath);
                if(dir == NULL) {
                    printf("Failed to open directory.\n");
                }
                write(clnt_sock, fp, sizeof(fp));

                sleep(1);
                while((entry = readdir(dir)) != NULL) {
                    if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        continue;

                    char name[BUF_SIZE];
                    snprintf(name, sizeof(name), "%s", entry->d_name);
                    printf("%s\n", name);

                    // 파일 크기 구하기
                    file = fopen(entry->d_name, "rb");
                    size_t fsize;
                    fseek(file, 0, SEEK_END);
                    fsize=ftell(file);
                    fseek(file, 0, SEEK_SET);
                    fclose(file);

                    char size[BUF_SIZE];
                    snprintf(size, sizeof(size), "%zu", fsize);
                    strcat(name, "   ");
                    strcat(name, size);// 파일 이름, 크기 한 문자열로 합치기

                    write(clnt_sock, name, sizeof(name));
                }
                write(clnt_sock, msg, sizeof(msg)); // 파일 이름, 크기 write

                int read_cnt = read(clnt_sock, buf, BUF_SIZE);
                puts(buf);

                // 1. Change directory
                // 2. Download-------------------------
                if(!strcmp(buf, "2"))
                {
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
                    // closedir(dir);
                }

                // 3. Upload------------------------------
                if(!strcmp(buf, "3"))
                {
                    // 파일 수신
                    read(clnt_sock, file_name, BUF_SIZE);
                    
                    read(clnt_sock, &filesize, sizeof(filesize));
                    filesize = htonl(filesize);
                    file = fopen(file_name, "wb");
                    if(file == NULL) {
                        printf("Failed to create file.\n");
                        continue;
                    }

                    nbyte = BUF_SIZE;
                    while(filesize > 0)
                    {
                        if(filesize < BUF_SIZE)
                            nbyte = read(clnt_sock, buf, filesize);
                        else
                            nbyte = read(clnt_sock, buf, BUF_SIZE);

                        if(nbyte == -1)
                        {
                            error_handling("read() error");
                            break;
                        }

                        fwrite(buf, sizeof(char), nbyte, file);
                        filesize -= nbyte;
                    }
                    fclose(file);
                    printf("receive complete\n");
                }

                if(!strcmp(buf, "4"))
                    break;

                closedir(dir);
            }
            return 0;
        }
    // }
    return 0;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void read_childproc(int sig)
{
    pid_t pid;
    int status;pid = waitpid(-1, &status, WNOHANG);
    printf("removed proc id: %d \n", pid);
}