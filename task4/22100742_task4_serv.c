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

typedef struct { // handle_clnt 함수에 전달할 parameter
    int socket;
    char *file;
}parameter;

typedef struct {
    char findStr[BUF_SIZE];
    int cnt;
}found;

found findInfo[11]; 

// Trie
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

    // db파일에서 읽은 검색어와 횟수 구분하기 위해 공백으로 split
    while (token != NULL) {
        strcpy(tokens[j], token);
        j++;
        token = strtok(NULL, " ");
    }
    // 검색어 저장
    for(int k = 0; k < j-1; k++)
    {
        strcpy(ptr, tokens[k]);
        ptr = ptr + strlen(ptr);
        *ptr = ' ';
        ptr++;
    }
    ptr--;
    *ptr = 0;

    strcpy(lowString, string);

    // 소문자로 변경
    for(int k = 0; k < strlen(lowString); k++)
    {
        lowString[k] = tolower(lowString[k]);
    }
    int len = strlen(lowString);

    // Trie에 insert
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

// 검색할 단어 reverse
void search1(node* root, const char* str)
{
    int len = strlen(str);
    char reverseStr[LEN];

    for(int i = 0; i < len; i++)
    {
        reverseStr[len - i - 1] = str[i];
    }
    reverseStr[len] = 0;
    
    search2(root, reverseStr, reverseStr, 0);
}

int n = 0;
// Trie에서 검색할 단어 search
void search2(node* root, char* str1, char* str2, int find)
{
    int len = strlen(str2);
    node* now = root;
    char *ptr1, *ptr2;
    ptr1 = str1;
    ptr2 = str2;

    if(*ptr2 == NULL)
    {
        find = 1;
        ptr2 = ptr1;
    }
    if(find == 1 && now->end)
    {
        if(n >= 10)
        {
            Sort();
            if(findInfo[9].cnt < now->cnt)
            {
                strcpy(findInfo[9].findStr, now->name);
                findInfo[9].cnt = now->cnt;
            }
        }
        else
        {
            strcpy(findInfo[n].findStr, now->name);
            findInfo[n].cnt = now->cnt;
            n++;
        }
    }

    for(int i = 0; i < 26; i++)
    {
        if(now->child[i])
        {
            if(*ptr2 - 'a' == i)
                search2(now->child[i], ptr1, ptr2 + 1, find);
            else
            {
                ptr2 = ptr1;
                search2(now->child[i], ptr1, ptr2, find);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    pthread_t t_id;
    parameter pm;

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

        pm.socket = clnt_sock;
        pm.file = argv[2];

        pthread_create(&t_id, NULL, handle_clnt, (void*)&pm);
        pthread_detach(t_id);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
    }
    close(serv_sock);
    return 0;
}

void * handle_clnt(void *arg)
{
    parameter *data = (parameter*)arg;
    int clnt_sock = data->socket;
    int str_len = 0;
    FILE *file;
    char content[LEN];
    char word[LEN];
    
    node* root = newNode();
    file = fopen(data->file, "rb");

    for(int i = 0; i < 5; i++)
    {
        fgets(content, LEN, file);
        insert(root, content);
    }

    while(1)
    {
        int read_cnt = read(clnt_sock, word, LEN);
        if(read_cnt == 0)
            break;

        n = 0;
        printf("word : %s\n", word);
        search1(root, word);

        if(strlen(word) == 0) n = 0;
        printf("n: %d\n", n);

        Sort();
        for(int i = 0; i < n; i++)
        {
            printf("%s   %d\n", findInfo[i].findStr, findInfo[i].cnt);
        }
        printf("\n");

        write(clnt_sock, &n, sizeof(int));
        for(int i = 0; i < n; i++)
        {
            write(clnt_sock, &findInfo[i], sizeof(findInfo[i]));
        }
    }
}

// db에서 읽은 검색어 횟수 내림차순으로 정렬
void Sort()
{
    found temp;
    int i = 0, j = 0;
    for (i = n; i > 1; i--)
    {
        for (j = 1; j < i; j++)
        {
            if (findInfo[j - 1].cnt < findInfo[j].cnt)
            {
                temp = findInfo[j - 1];
                findInfo[j - 1] = findInfo[j];
                findInfo[j] = temp;
            }
        }
    }
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}