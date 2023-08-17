#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <getopt.h>

#define BUF_SIZE 1024
#define MAX_CLNT 256

void error_handling(char *message);
void sender(int max, char *file_name, int segSize, char *listenPort);
void file_send(char *file_name, int segmentSize, int max, int *clnt_socks);
void receiver(char *senderIp, char *senderPort, char *listen_port);
void *listening(void *arg);
void *connecting(void *arg);
void *read_from_sender(void *arg);
void *read_from_rPeer(void *arg);
void *write_file(void *arg);

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;

// Linked List
typedef struct list
{
    struct node *cur;
    struct node *head;
} linkedList;

typedef struct node
{
    int seq;
    char *fileContent;
    int file_size;
    struct node *next;
} node;

void insert(linkedList *L, int seq, char *fileContent, int file_size, int segmentSize)
{
    node *newNode = (node *)malloc(sizeof(node));
    newNode->fileContent = (char *)malloc((segmentSize * 1024) * sizeof(char));

    memcpy(newNode->fileContent, fileContent, file_size);
    newNode->seq = seq;
    newNode->file_size = file_size;
    newNode->next = NULL;
    node *p = L->head;
    node *pre = NULL;

    if (L->head == NULL || L->head->seq > newNode->seq)
    {
        newNode->next = L->head;
        L->head = newNode;
        return;
    }

    while (p != NULL && p->seq < newNode->seq)
    {
        pre = p;
        p = p->next;
    }

    if (pre != NULL)
        pre->next = newNode;

    newNode->next = p;

    L->cur = newNode;
}

void deleteNode(linkedList *L)
{
    if (L->head == NULL)
        return;

    node *p = L->head;
    L->head = L->head->next;

    free(p->fileContent); // Free the dynamically allocated file content
    free(p);
}

void printNodes(linkedList *L)
{
    node *p = L->head;
    putchar('[');
    while (p != NULL)
    {
        printf("%d, ", p->seq);
        p = p->next;
    }
    putchar(']');
    putchar('\n');
}

typedef struct
{ // listening thread에 전달할 parameter
    int *accept_socks;
    int id;
    int max;
    char *port;
} lParameter;

typedef struct
{ // connect thread에 전달할 parameter
    int *connect_socks;
    int id;
    int max;
    char **ip;
    char **port;
} cParameter;

typedef struct
{ // reading thread 함수에 전달할 parameter
    int sender_sock;
    int id;
    int max;
    int file_size;
    int segmentSize;
    int *accept_socks;
    int *connect_socks;
    linkedList *list;
} rParameter;

typedef struct
{ // reading thread 함수에 전달할 parameter
    int sender_sock;
    int id;
    int max;
    int file_size;
    int segmentSize;
    char file_name[BUF_SIZE];
    linkedList *list;
} wParameter;

typedef struct
{
    int file_seq;
    // char fileContent[1000];
    int fileSize;
} pkt_t;

int main(int argc, char *argv[])
{
    int opt;
    int max_clnt, segmentSize;
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    char file_name[BUF_SIZE];
    char role;
    char ip[BUF_SIZE];
    char port[BUF_SIZE];
    char listen_port[BUF_SIZE];

    // option parsing
    while ((opt = getopt(argc, argv, "srn:f:g:a:p:")) != -1)
    {
        switch (opt)
        {
        case 's':
            role = 's';
            break;

        case 'r':
            role = 'r';
            break;

        case 'n':
            max_clnt = atoi(optarg);
            break;

        case 'f':
            strcpy(file_name, optarg);
            break;

        case 'g':
            segmentSize = atoi(optarg);
            break;

        case 'a':
            strcpy(ip, optarg);
            if (argv[optind][0] == '-')
            {
                printf("inaccurate\n");
                exit(1);
            }
            strcpy(port, argv[optind]);
            optind++;
            break;

        case 'p':
            strcpy(listen_port, optarg);
            break;
        }
    }

    if (role == 's') // sending peer
    {
        sender(max_clnt, file_name, segmentSize, listen_port);
    }
    else if (role == 'r') // receiving peer
        receiver(ip, port, listen_port);

    printf("end\n");
    return 0;
}

void sender(int max, char *file_name, int segmentSize, char *listenPort)
{
    // printf("send\n");
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    pthread_t t_id;
    char port[BUF_SIZE];
    char receiver_ip[max][BUF_SIZE];
    char receiver_port[max][BUF_SIZE];
    char msg[BUF_SIZE];

    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(listenPort));

    int reuse = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // receiver들과 연결
    while (1)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

        clnt_socks[clnt_cnt++] = clnt_sock;

        write(clnt_sock, &max, sizeof(int)); // 최대 client 수 write
        read(clnt_sock, port, BUF_SIZE);     // 각 receiving peer의 listen port read

        strcpy(receiver_ip[clnt_cnt - 1], inet_ntoa(clnt_adr.sin_addr));
        strcpy(receiver_port[clnt_cnt - 1], port);
        printf("IP: %s, Port: %s\n", receiver_ip[clnt_cnt - 1], receiver_port[clnt_cnt - 1]);

        int id = clnt_cnt - 1;
        write(clnt_sock, &id, sizeof(int)); // receiver id write

        if (clnt_cnt == max)
            break;
    }

    // receiving peer 정보 write
    for (int i = 0; i < max; i++)
    {
        for (int j = 0; j < max; j++)
        {
            write(clnt_socks[i], receiver_ip[j], sizeof(receiver_ip[j]));
            write(clnt_socks[i], receiver_port[j], sizeof(receiver_port[j]));
        }
    }

    for (int i = 0; i < max; i++)
        read(clnt_socks[i], msg, BUF_SIZE);

    printf("%s\n", msg);

    file_send(file_name, segmentSize, max, clnt_socks);

    // while (1)
    // {
    // }

    close(serv_sock);
    printf("complete\n");
}

void file_send(char *file_name, int segmentSize, int max, int *clnt_socks)
{
    // printf("file send\n"); //
    FILE *file;
    int file_size;
    char fname[BUF_SIZE];
    strcpy(fname, file_name);
    char *buf;
    buf = (char *)malloc((segmentSize * 1024) * sizeof(char));

    pkt_t *file_pkt;

    file_pkt = (pkt_t *)malloc(sizeof(pkt_t));
    memset(file_pkt, 0, sizeof(pkt_t));

    // 파일 크기 구하기
    file = fopen(file_name, "rb");
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    printf("fname: %s, fsize: %d, ssize: %d\n", fname, file_size, segmentSize);
    // receiving peer들에게 file_name, file_size, segmentSize write
    for (int i = 0; i < max; i++)
    {
        write(clnt_socks[i], fname, BUF_SIZE);
        write(clnt_socks[i], &file_size, sizeof(int));
        write(clnt_socks[i], &segmentSize, sizeof(int));
    }

    int readSize = 0;
    int rPeer_id = 0;
    int segment_num = 0;
    int sending[max];
    double pct;

    for (int i = 0; i < max; i++)
        sending[i] = 0;

    while (readSize != file_size)
    {
        int fsize = fread(buf, 1, segmentSize * 1024, file);
        readSize += fsize;

        sending[rPeer_id] += fsize;
        pct = ((readSize * 1.0) / file_size) * 100;

        // system("clear");
        // printf("\x1b[%dA\r", max + 1);
        printf("Sending Peer [");
        for (int i = 0; i < 20; i++)
        {
            if (i < (int)pct / 5)
                printf("#");
            else
                printf(" ");
        }

        printf("] %.0f%% (%d/%d Bytes)\n", pct, readSize, file_size);

        for (int i = 0; i < max; i++)
            printf("To Receiving Peer #%d: %d\n", i + 1, sending[i]);

        if (fsize < segmentSize * 1024)
        {
            file_pkt->fileSize = fsize;
            file_pkt->file_seq = segment_num;

            write(clnt_socks[rPeer_id], buf, segmentSize * 1024); //
            write(clnt_socks[rPeer_id], file_pkt, sizeof(pkt_t));
            printf("fsize: %d\n", fsize);
            break;
        }

        file_pkt->fileSize = fsize;
        file_pkt->file_seq = segment_num;
        write(clnt_socks[rPeer_id], buf, segmentSize * 1024); //
        write(clnt_socks[rPeer_id], file_pkt, sizeof(pkt_t));

        rPeer_id++;
        if (rPeer_id == max)
            rPeer_id = 0;

        segment_num++;
        printf("fsize: %d\n", fsize);
    }
    fclose(file);

    strcpy(buf, "end");
    for (int i = 0; i < max; i++)
        write(clnt_socks[i], buf, segmentSize * 1024);
    printf("send complete\n");

    free(buf);
    free(file_pkt);

    for (int i = 0; i < max; i++)
        close(clnt_socks[i]);
}

int receiving = 0;
void receiver(char *senderIp, char *senderPort, char *listen_port)
{
    int sender_sock, serv_sock;
    struct sockaddr_in sender_adr, serv_adr, clnt_adr;
    int clnt_adr_sz;
    pthread_t listen_thread, connect_thread;
    pthread_t readS_thread, readR_thread, write_thread;
    void *thread_return;
    lParameter lPm;
    cParameter cPm;
    rParameter *rPm;
    wParameter wPm;
    char **rPeer_ip;
    char **rPeer_port;
    int id;
    int max;

    pthread_mutex_init(&mutx, NULL);

    // sender와 연결하기 위한 socket
    sender_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sender_sock == -1)
        error_handling("socket() error");

    memset(&sender_adr, 0, sizeof(sender_adr));
    sender_adr.sin_family = AF_INET;
    sender_adr.sin_addr.s_addr = inet_addr(senderIp);
    sender_adr.sin_port = htons(atoi(senderPort));

    if (connect(sender_sock, (struct sockaddr *)&sender_adr, sizeof(sender_adr)) == -1)
        error_handling("connect() error");

    printf("connect\n");

    read(sender_sock, &max, sizeof(int));                 // 최대 client 수 read
    write(sender_sock, listen_port, sizeof(listen_port)); // listen port write
    read(sender_sock, &id, sizeof(int));                  // id read

    rPeer_ip = malloc(sizeof(char *) * max);
    rPeer_port = malloc(sizeof(char *) * max);
    for (int i = 0; i < max; i++)
    {
        rPeer_ip[i] = malloc(sizeof(char) * BUF_SIZE);
        rPeer_port[i] = malloc(sizeof(char) * BUF_SIZE);
    }

    // receiving peer 정보 read
    for (int i = 0; i < max; i++)
    {
        read(sender_sock, rPeer_ip[i], BUF_SIZE);
        read(sender_sock, rPeer_port[i], BUF_SIZE);
        if (i != id)
            printf("id: %d, ip: %s, port: %s\n", i, rPeer_ip[i], rPeer_port[i]);
    }

    // listen, connect thread
    int accept_socks[id];
    int connect_socks[max - id - 1];
    lPm.id = id;
    lPm.port = listen_port;
    lPm.max = max;
    lPm.accept_socks = accept_socks;

    cPm.connect_socks = connect_socks;
    cPm.id = id;
    cPm.max = max;
    cPm.ip = rPeer_ip;
    cPm.port = rPeer_port;

    pthread_create(&listen_thread, NULL, listening, &lPm);
    sleep(1);
    pthread_create(&connect_thread, NULL, connecting, &cPm);
    pthread_join(listen_thread, &thread_return);
    pthread_join(connect_thread, &thread_return);

    for (int i = 0; i < id; i++)
        printf("accept_sock: %d\n", accept_socks[i]);

    for (int i = 0; i < (max - id - 1); i++)
        printf("connect_sock: %d\n", connect_socks[i]);

    // 연결 완료 메세지 보내기
    char msg[BUF_SIZE] = "Init Complete";
    write(sender_sock, msg, sizeof(msg));

    int file_size, segmentSize;
    char file_name[BUF_SIZE];

    read(sender_sock, file_name, BUF_SIZE);
    read(sender_sock, &file_size, sizeof(int));
    read(sender_sock, &segmentSize, sizeof(int));
    printf("fname: %s, fsize: %d, ssize: %d\n", file_name, file_size, segmentSize);

    // read thread
    int seq = 0;
    linkedList *L = (linkedList *)malloc(sizeof(linkedList));
    L->cur = NULL;
    L->head = NULL;

    rPm = malloc(max * sizeof(rParameter));
    rPm[0].sender_sock = sender_sock;
    for (int i = 0; i < max; i++)
    {
        rPm[i].id = id;
        rPm[i].max = max;
        rPm[i].file_size = file_size;
        rPm[i].segmentSize = segmentSize;
        rPm[i].accept_socks = accept_socks;
        rPm[i].connect_socks = connect_socks;
        rPm[i].list = L;
    }

    wPm.sender_sock = sender_sock;
    wPm.id = id;
    wPm.max = max;
    wPm.file_size = file_size;
    wPm.segmentSize = segmentSize;
    wPm.list = L;
    strcpy(wPm.file_name, file_name);

    printf("thread\n");

    int j = 1;
    int pre = receiving;
    // printf("Receiving Peer %d [                    ] 0%% (0/%d Bytes)", id, file_size);

    pthread_create(&readS_thread, NULL, read_from_sender, &rPm[0]);
    for (int i = 0; i < id; i++)
    {
        rPm[j].sender_sock = accept_socks[i];
        pthread_create(&readR_thread, NULL, read_from_rPeer, &rPm[j]);
        pthread_detach(readR_thread);
        j++;
    }
    for (int i = 0; i < max - id - 1; i++)
    {
        rPm[j].sender_sock = connect_socks[i];
        pthread_create(&readR_thread, NULL, read_from_rPeer, &rPm[j]);
        pthread_detach(readR_thread);
        j++;
    }
    // pthread_create(&readR_thread, NULL, read_from_rPeer, &rPm);
    pthread_create(&write_thread, NULL, write_file, &wPm);
    while (1)
    {
        pthread_mutex_lock(&mutx);
        if (receiving != pre)
        {
            printf("Receiving Peer %d [", id);
            double pct = (receiving * 1.0) / file_size * 100;
            for (int i = 0; i < 20; i++)
            {
                if (i < (int)pct / 5)
                    printf("#");
                else
                    printf(" ");
            }
            printf("] %.0f%% (%d/%d Bytes)\n", pct, receiving, file_size);
            pre = receiving;
        }
        if (receiving == file_size)
        {
            pthread_mutex_unlock(&mutx);
            break;
        }
        pthread_mutex_unlock(&mutx);
    }
    pthread_join(readS_thread, &thread_return);
    // pthread_join(readR_thread, &thread_return);
    pthread_join(write_thread, &thread_return);

    // while (1)
    // {
    // }
    for (int i = 0; i < max; i++)
    {
        free(rPeer_ip[i]);
        free(rPeer_port[i]);
    }
    free(rPeer_ip);
    free(rPeer_port);
    free(rPm);
}

// 다른 receiving peer의 connect를 기다리는 thread
void *listening(void *arg)
{
    lParameter *data = (lParameter *)arg;
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(data->port));

    int reuse = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    for (int i = 0; i < data->id; i++)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
        data->accept_socks[i] = clnt_sock;
    }

    return NULL;
}

// 다른 receiving peer에게 connect하는 thread
void *connecting(void *arg)
{
    cParameter *data = (cParameter *)arg;
    int sock;
    struct sockaddr_in sock_adr;
    int j = 0;

    for (int i = data->id + 1; i < data->max; i++)
    {
        data->connect_socks[j] = socket(PF_INET, SOCK_STREAM, 0);

        memset(&sock_adr, 0, sizeof(sock_adr));
        sock_adr.sin_family = AF_INET;
        sock_adr.sin_addr.s_addr = inet_addr(data->ip[i]);
        sock_adr.sin_port = htons(atoi(data->port[i]));

        if (connect(data->connect_socks[j], (struct sockaddr *)&sock_adr, sizeof(sock_adr)) == -1)
            error_handling("connect() error");

        j++;
    }

    return NULL;
}

void *read_from_sender(void *arg)
{
    pkt_t *file_pkt;

    file_pkt = (pkt_t *)malloc(sizeof(pkt_t));
    memset(file_pkt, 0, sizeof(pkt_t)); ///

    // printf("\nread from sender\n");
    rParameter *data = (rParameter *)arg;
    char *buf;
    buf = (char *)malloc((data->segmentSize * 1024) * sizeof(char));

    while (1)
    {
        while (1)
        {
            int read_cnt = recv(data->sender_sock, buf, data->segmentSize * 1024, MSG_PEEK);
            if (read_cnt == data->segmentSize * 1024)
                break;
        }
        recv(data->sender_sock, buf, data->segmentSize * 1024, 0);
        if (!strcmp(buf, "end"))
            break;

        while (1)
        {
            int read_cnt = recv(data->sender_sock, file_pkt, sizeof(pkt_t), MSG_PEEK);
            if (read_cnt == sizeof(pkt_t))
                break;

            // printf("\nsize: %d\n", read_cnt);
        }
        recv(data->sender_sock, file_pkt, sizeof(pkt_t), 0);

        pthread_mutex_lock(&mutx);
        receiving += file_pkt->fileSize;
        pthread_mutex_unlock(&mutx);

        // printf("[recvS]:%d, seq: %d, fsize: %d\n", data->sender_sock, file_pkt->file_seq, file_pkt->fileSize);

        pthread_mutex_lock(&mutx);
        insert(data->list, file_pkt->file_seq, buf, file_pkt->fileSize, data->segmentSize);
        // printNodes(data->list);
        pthread_mutex_unlock(&mutx);

        for (int i = 0; i < data->id; i++)
        {
            write(data->accept_socks[i], buf, data->segmentSize * 1024);
            write(data->accept_socks[i], file_pkt, sizeof(pkt_t));
        }
        for (int i = 0; i < (data->max - data->id - 1); i++)
        {
            write(data->connect_socks[i], buf, data->segmentSize * 1024);
            write(data->connect_socks[i], file_pkt, sizeof(pkt_t));
        }
    }

    // file_pkt->fileSize = 0;
    strcpy(buf, "end");
    for (int i = 0; i < data->id; i++)
        write(data->accept_socks[i], buf, data->segmentSize * 1024);
    for (int i = 0; i < (data->max - data->id - 1); i++)
        write(data->connect_socks[i], buf, data->segmentSize * 1024);

    printf("read_from_sender_complete\n");
    free(buf);
    free(file_pkt);

    return NULL;
}

void *read_from_rPeer(void *arg)
{
    pkt_t *file_pkt;

    file_pkt = (pkt_t *)malloc(sizeof(pkt_t));
    memset(file_pkt, 0, sizeof(pkt_t)); ///

    // printf("read from rPeer\n");
    rParameter *data = (rParameter *)arg;
    char *buf;
    buf = (char *)malloc((data->segmentSize * 1024) * sizeof(char));

    while (1)
    {
        int read_cnt;
        // printf("[recvR %d] rPeer in\n", data->sender_sock);
        while (1)
        {
            read_cnt = recv(data->sender_sock, buf, data->segmentSize * 1024, MSG_PEEK);
            // printf("[recvR %d] read_cnt: %d\n", data->sender_sock, read_cnt);
            if (read_cnt == data->segmentSize * 1024)
                break;
        }
        recv(data->sender_sock, buf, data->segmentSize * 1024, 0);
        // printf("[recvR %d] 1\n", data->sender_sock);

        if (!strcmp(buf, "end"))
            break;

        while (1)
        {
            read_cnt = recv(data->sender_sock, file_pkt, sizeof(pkt_t), MSG_PEEK);
            if (read_cnt == sizeof(pkt_t))
                break;
        }
        recv(data->sender_sock, file_pkt, sizeof(pkt_t), 0);

        pthread_mutex_lock(&mutx);
        receiving += file_pkt->fileSize;
        pthread_mutex_unlock(&mutx);
        // printf("[recvR %d] 2\n", data->sender_sock);

        // printf("[%d recvR %d], seq: %d, fsize: %d\n", data->id, data->sender_sock, file_pkt->file_seq, file_pkt->fileSize);

        pthread_mutex_lock(&mutx);
        insert(data->list, file_pkt->file_seq, buf, file_pkt->fileSize, data->segmentSize);
        // printNodes(data->list);
        pthread_mutex_unlock(&mutx);
        // printf("[recvR %d] 3\n", data->sender_sock);
    }
    printf("read_from_rPeer_complete\n");
    free(buf);
    free(file_pkt);

    return NULL;
}

void *write_file(void *arg)
{
    printf("write file\n");
    wParameter *data = (wParameter *)arg;
    int write_seq = 0;
    FILE *file;
    file = fopen(data->file_name, "wb");

    while (1)
    {
        pthread_mutex_lock(&mutx);
        if (data->list->head != NULL && data->list->head->seq == write_seq)
        {
            // if (data->list->head->seq == write_seq)
            // {
            // printf("write_seq: %d\n", write_seq);
            fwrite(data->list->head->fileContent, 1, data->list->head->file_size, file);
            // printf("write_seq: %d, filesize: %d, segmentSize: %d\n", write_seq, data->list->head->file_size, data->segmentSize * 1024);
            if (data->list->head->file_size < data->segmentSize * 1024)
            {
                pthread_mutex_unlock(&mutx);
                break;
            }
            deleteNode(data->list);
            write_seq++;
            // }
        }
        pthread_mutex_unlock(&mutx);
    }
    fclose(file);
    printf("write complete\n");

    return NULL;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}