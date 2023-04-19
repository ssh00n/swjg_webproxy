#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define TINY_HOST "localhost"

typedef struct cache_node{
    char* file_path;
    char* response_header;
    char* content;
    int content_length;
    struct cache_node *prev;
    struct cache_node *next;
} cache_node;

struct cache {
    int total_size;
    struct cache_node* head;
    struct cache_node* tail;
};

struct cache *c;


cache_node *find_cache(struct cache* c, char* file_path);
void insert_cache(struct cache* c, char* file_path, char *response_header, char* content, int content_length);
void delete_cache(struct cache* c);
void hit(struct cache* c, struct cache_node* node);
void doit(int fd);
void *thread(void *vargp);
int parse_uri(char *uri, char *hostname, char *path, char *port);
void read_requesthdrs(rio_t *rp, char* HTTPheader, char *host, char *port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
// void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv) {

    pthread_t tid;
    int listenfd, *connfdp;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    
    c = (struct cache*)malloc(sizeof(struct cache));
    c->total_size = 0;
    c->head = NULL;
    c->tail = NULL;

    if (argc != 2) { // if arguments count != 2 ('./proxy {port}'), exit program
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // listen for proxy requests
    listenfd = Open_listenfd(argv[1]);
    

    while (1) {
          clientlen = sizeof(clientaddr);
          connfdp = Malloc(sizeof(int));
          *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
          Pthread_create(&tid, NULL, thread, connfdp);

          // handle request
    }
  return 0;
}

void *thread(void *vargp){
        int connfd = *((int *)vargp);
        Pthread_detach(pthread_self());
        Free(vargp);
        doit(connfd);
        Close(connfd);
        return NULL;    
}


void doit(int fd){
    struct stat sbuf;
    char request_buf[MAXLINE], method[10], host[MAXLINE], uri[MAXLINE], port[10], path[MAXLINE], version[10];

    char *client_path = (char *)malloc(MAXLINE);
    
    rio_t rio, rio2;
    int targetfd;
    char HTTPheader[MAXLINE];
    char buf[MAXLINE];

    strcpy(HTTPheader, "");

    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, request_buf, MAXLINE);

    sscanf(request_buf, "%s %s %s", method, uri, version);

    parse_uri(uri, host, path, port);
    read_requesthdrs(&rio, HTTPheader, host, port, path);

    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Invalid request", "");
        return;
    }
    strcpy(client_path, path);

    cache_node* node = find_cache(c, client_path);
    
    if (node != NULL){
        hit(c, node);

        Rio_writen(fd, node->response_header, strlen(node->response_header));
        Rio_writen(fd, node->content, node->content_length);
        return;
    }
    
     // Establish a connection to the target server
    targetfd = Open_clientfd(TINY_HOST, port);
    Rio_writen(targetfd, HTTPheader, strlen(HTTPheader));

    char response_buf[MAXLINE];
    char response_header[MAXLINE];
    char response_content[MAXLINE];
    char *payload;
    int content_length;
    // parse response_header

    sprintf(response_header, "");

    Rio_readinitb(&rio2, targetfd);
    Rio_readlineb(&rio2, response_buf, MAXLINE);
    strcat(response_header, response_buf);
    while(strcmp(response_buf, "\r\n")){
        if (strstr(response_buf, "Content-length:"))
            content_length = atoi(strchr(response_buf, ':') + 1);
        Rio_readlineb(&rio2, response_buf, MAXLINE);
        strcat(response_header, response_buf);
    }

    printf("-------- response header ------------\n");
    printf("%s", response_header);
    payload = malloc(content_length);

    printf("-------- response content -----------\n");
    
    Rio_readnb(&rio2, payload, content_length);
    printf("%s", response_content);

    Rio_writen(fd, response_header, strlen(response_header));
    Rio_writen(fd, payload, content_length);

    insert_cache(c, client_path, response_header, payload, content_length);
    
    close(targetfd);

}

/* http://www.cmu.edu:8000/hub/index.html */
int parse_uri(char *uri, char *host, char *path, char *port){
    char *port_start, *path_start;
    
     // Check if URI starts with "http://"
    if (strstr(uri, "http://") == uri) {
        // Skip over "http://"
        uri += 7;
    }
    path_start = strchr(uri, '/');
    port_start = strchr(uri, ':');

    if (port_start == NULL) {
        strcpy(port, "80");
        if (path_start == NULL){
            strcpy(host, uri);
            strcpy(path, "/");
        }
        else {
            strncpy(host, uri, strlen(uri) - strlen(path_start));
            strcpy(path, path_start);
        }
    }
    else {
        if (path_start == NULL) {
            strncpy(host, uri, strlen(uri) - strlen(port_start));
            strcpy(port, port_start+1);
            strcpy(path, "/");
        }
        else{ // port 있고, path 있음
            strncpy(host, uri, strlen(uri) - strlen(port_start));
            strncpy(port, port_start+1, strlen(port_start) - strlen(path_start)-1);
            strcpy(path, path_start);
        }
    }
    return 0;
}

void read_requesthdrs(rio_t *rp, char* HTTPheader, char *host, char *port, char *path)  // HTTP header
{

    char buf[MAXLINE], request_hdr[MAXLINE], other_header[MAXLINE], host_header[MAXLINE];
    sprintf(request_hdr, "GET %s HTTP/1.0\r\n", path);
    
    while(strcmp(buf, "\r\n")) {          
        Rio_readlineb(rp, buf, MAXLINE);
        if (strstr(buf, "Host:")){
            strcpy(host_header, buf);
        }
        else if (strstr(buf, "User-Agent:")) {
            continue;   
        }
        else if (strstr(buf, "Connection:")){
            continue;
        }
        else if (strstr(buf, "Proxy-Connection")){
            continue;
        }
        else {
            if (strcmp(buf, "\r\n"))
                break;
            strcat(other_header, buf);
        }
    }
    if (!strstr(host_header, "Host:")){
        sprintf(host_header, "Host: %s\r\n", host);
    }
    sprintf(HTTPheader, "%s%s%s%s%s%s\r\n", request_hdr, user_agent_hdr, host_header, other_header, "Connection: close\r\n", "Proxy-Connection: close\r\n");
    
}

cache_node *find_cache(struct cache *c, char* file_path){
    cache_node *curr;
    curr = c->head;
    while(curr != NULL) {
        if (strcmp(curr->file_path, file_path) == 0){
            printf("Found cache!\n");
            return curr;
        }
        curr = curr->next;
    }
    printf("Not Found Cache!\n");
    return NULL;
}


void insert_cache(struct cache* c, char* file_path, char *response_header, char* content, int content_length) {
    // cache_node* node = find_cache(c, file_path);
    // if (node != NULL) { // the URI already exists in the cache, just update the cache
    //     hit(c, node);
    //     return;
    // }

    if (content_length >= MAX_OBJECT_SIZE){
        return;
    }

    if (c->total_size + content_length >= MAX_CACHE_SIZE){
        while(c->total_size + content_length >= MAX_CACHE_SIZE)
            delete_cache(c);
    }

    // Insert the new node into the cache linked list
    cache_node* new_node = (cache_node*)malloc(sizeof(cache_node));
    new_node->file_path = file_path;
    new_node->response_header = response_header;
    new_node->content = content;
    new_node->content_length = content_length;
    new_node->prev = NULL;
    new_node->next = NULL;

    if (c->head == NULL) {
        c->head = new_node;
        c->tail = new_node;
    } else {
        c->head->prev = new_node;
        new_node->next = c->head;
        c->head = new_node;
    }
    c->total_size += content_length;
}

void delete_cache(struct cache* c){ // delete the tail node(the least recently used node from the cache linked list)
    cache_node *temp;
    temp = c->tail;
    c->tail = c->tail->prev;
    c->tail->next = NULL;

    free(temp);
    
}

void hit(struct cache* c, struct cache_node* node){
    if (node == c->head) {
        return;
    } 
    else {
        node->prev->next = node->next;     // Remove the cache node from the cache linked list
        node->next->prev = node->prev;

        node->next = c->head;   // Re-insert the cache node to the head of the cache linked list
        c->head = node;
    }
}




void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{

    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));

}
