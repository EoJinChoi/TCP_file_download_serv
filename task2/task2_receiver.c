#define _XOPEN_SOURCE 200  

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024
void error_handling(char *message);

typedef struct {
    int seq;
    int ack;
}pkt;
pkt *send_pkt;

typedef struct {
    int seq;
    char fileName[BUF_SIZE];
    char fileSize[BUF_SIZE];
    char fileContent[BUF_SIZE];
    int size;
} pkt_t;
pkt_t *data_pkt;


int main(int argc, char *argv[])
{
    int serv_sock;
    socklen_t clnt_adr_sz;
    FILE *file;
    char file_name[BUF_SIZE] = "photo.jpeg";
    int seq = -1;

    data_pkt = (pkt_t *) malloc(sizeof(pkt_t));
    memset(data_pkt, 0, sizeof(pkt_t));
    send_pkt = (pkt *) malloc(sizeof(pkt));
    memset(send_pkt, 0, sizeof(pkt));

    struct sockaddr_in serv_adr, clnt_adr;
    if(argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(serv_sock == -1)
        error_handling("UDP socket creation error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    file = fopen(file_name, "wb");
    if(file == NULL) {
        printf("Failed to create file.\n");
        exit(1);
    }
    while(1)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        int recv = recvfrom(serv_sock, data_pkt, sizeof(pkt_t), 0, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
        if(recv > 0)
            send_pkt->ack = 1;
        else
            send_pkt->ack = 0;
        if(seq == data_pkt->seq)
            continue;
    
        send_pkt->seq = data_pkt->seq;
        sendto(serv_sock, send_pkt, sizeof(pkt_t), 0, (struct sockaddr*)&clnt_adr, clnt_adr_sz);

        printf("\nsize: %d\n", atoi(data_pkt->fileSize));
        fwrite(data_pkt->fileContent, 1, atoi(data_pkt->fileSize), file);

        if(data_pkt->size < BUF_SIZE)
            break;
    }
    fclose(file);

    close(serv_sock);
    return 0;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}