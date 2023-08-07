#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <ctype.h>

#define BUF_SIZE 1024
#define MAX_CLNT 256
#define LEN 100

void error_handling(char *message);
void * handle_clnt(void *arg);

int clnt_cnt;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;

typedef struct node{
    struct node *child[26];
    int end;
    int cnt;
    char name[LEN];
}node;

node *newNode(){
    node * new = (node *)malloc(sizeof(node));
    new->end = 0;
    new->cnt = 0;
    for(int i = 0; i < 26; i++)
        new->child[i] = 0;
    return new;
}

void insert(node *root, char* str)
{
    char tokens[LEN][20];
    char* token = strtok(str, " ");
    int j = 0;
    char string[LEN] = "";
    char lowString[LEN];
    node* now = root;
    char *ptr = string;

    while (token != NULL) {
        strcpy(tokens[j], token);
        j++;
        token = strtok(NULL, " ");
    }
    for(int k = 0; k < j-1; k++)
    {
        strcpy(ptr, tokens[k]);
        ptr = ptr + strlen(ptr);
        *ptr = ' ';
        ptr++;
    }
    ptr--;
    *ptr = 0;

    // printf("str1: %s\n", string);
    strcpy(lowString, string);

    
    // 소문자로 변경
    for(int k = 0; k < strlen(lowString); k++)
    {
        lowString[k] = tolower(lowString[k]);
    }
    int len = strlen(lowString);

    for(int i = len - 1; i >= 0; i--){
        if(lowString[i] == ' ')
        {
            while(lowString[i] == ' ')
                i--;
        }
        if(!now->child[lowString[i] - 'a'])
            now->child[lowString[i] - 'a'] = newNode();
        now = now->child[lowString[i] - 'a'];
    }

    now->end = 1;
    strcpy(now->name, string);
    now->cnt = atoi(tokens[j-1]);
}

void showtree(node* root)
{
    node *now = root;

    if(now->end)
        printf("%s  %d\n", now->name, now->cnt);


    for(int i = 0; i < 26; i++)
    {
        if(now->child[i]){
            showtree(now->child[i]);
       } 
    }
}

int search(node* root, const char* str)
{
    char reverseStr[LEN];
    for(int i = 0; i < strlen(str); i++)
    {
        reverseStr[strlen(str) - i-1] = str[i];
    }
    printf("re: %s\n", reverseStr);
    int len = strlen(str);
    node* now = root;

    for(int i = 0; i < len; i++)
    {
        if(!now->child[reverseStr[i] - 'a'])
            return 0;
        if(i == 0) return 1;
        now = now->child[reverseStr[i] - 'a'];
    }
    return now->end;
}

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    pthread_t t_id;
    if(argc != 3) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    while(1)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);

        pthread_mutex_lock(&mutx);
        clnt_socks[clnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
        pthread_detach(t_id);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
    }
    close(serv_sock);
    return 0;
}

void * handle_clnt(void *arg)
{
    int clnt_sock = *((int*)arg);
    int str_len = 0;
    FILE *file;
    char content[LEN];
    // char tokens[LEN][20];
    
    node* root = newNode();
    file = fopen("data.txt", "rb");

    for(int i = 0; i < 5; i++)
    {
        fgets(content, LEN, file);
        
        insert(root, content);
    }

    printf("-------------show all word--------------\n");
    showtree(root);

    printf("\ndo I have?\n");
    printf("%s : %s\n", "Pohang", search(root, "pohang")? "Yes":"No");
    printf("%s : %s\n", "ang", search(root, "taurant")? "Yes":"No");
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}