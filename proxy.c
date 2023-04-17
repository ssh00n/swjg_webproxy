#include <stdio.h>
#include "csapp.h"


/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define TINY_HOST "localhost"
#define TINY_PORT "8000"


void doit(int fd);
// void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *hostname, char *path, int *port);
void read_requesthdrs(rio_t *rp, char* HTTPheader, char *host, int *port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

// printf("%s", user_agent_hdr);

// logic : main
// 1. listen for proxy requests
// 2. handle that request (doit)

// logic : doit
// 1. connect to the target server
// 2. copy(parse) the src output to the destination connection
// 3. copy the response from target server conn to src conn


int main(int argc, char **argv) {

    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc != 2) { // if arguments count != 2 ('./proxy {port}'), exit program
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // listen for proxy requests
    listenfd = Open_listenfd(argv[1]);
    while (1) {
          clientlen = sizeof(clientaddr);
          connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
          Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
          printf("Accepted proxy connection from (%s, %s)\n", hostname, port);
            
          // handle request
          doit(connfd);
          Close(connfd);

    }
  return 0;
}

void doit(int fd){
    struct stat sbuf;
    char request_buf[MAXLINE], method[10], host[MAXLINE], uri[MAXLINE], path[MAXLINE], version[10];
    rio_t rio, rio2;
    int port, targetfd;
    char HTTPheader[MAXLINE];
    

    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, request_buf, MAXLINE);
    sscanf(request_buf, "%s %s %s", method, uri, version);
    printf("%s", request_buf);

    parse_uri(uri, host, path, &port);
    read_requesthdrs(&rio, HTTPheader, host, port, path);

    // printf("Port : %d \n", port);
    // printf("HTTPheader:\n%s", HTTPheader);
    
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Invalid request", "");
        return;
    }
    

     // Establish a connection to the target server

    targetfd = Open_clientfd(TINY_HOST, TINY_PORT);
    Rio_writen(targetfd, HTTPheader, strlen(HTTPheader));

    char forward_buf[MAXLINE];
    char response_buf[MAXLINE];
    char response_result[MAXLINE];

    Rio_readinitb(&rio2, targetfd);
    Rio_readlineb(&rio2, response_buf, MAXLINE);
    
    while(strcmp(response_buf, "\r\n")){
        Rio_readlineb(&rio2, response_buf, MAXLINE);
        strcat(response_buf);
    }
    
    //printf("%d", targetfd);

    close(targetfd);

        // Getaddrinfo(hostname, NULL, )

}


int parse_uri(char *uri, char *host, char *path, int *port){
    *port = 8000;
    char *port_start, *path_start;
    
     // Check if URI starts with "http://"
    if (strstr(uri, "http://") == uri) {
        // Skip over "http://"
        uri += 7;
    }
    path_start = strchr(uri, '/');
    port_start = strchr(uri, ':');
    if (port_start == NULL) {
        if (path_start == NULL){
            strcpy(host, uri);
            strcpy(path, "");
        }
        else {
            strncpy(host, uri, path_start - uri);
            strcpy(path, path_start);
        }
    }
    else {
        strncpy(host, uri, port_start - uri);
        strcpy(path, path_start);
    }
    return 0;
}

void read_requesthdrs(rio_t *rp, char* HTTPheader, char *host, int *port, char *path)  // HTTP header
{

    char buf[MAXLINE], request_hdr[MAXLINE], other_header[MAXLINE], host_header[MAXLINE];
    sprintf(request_hdr, "GET %s HTTP/1.0\r\n", path);
    
    while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
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
            if (strcmp(buf, "\r\n")){
            strcat(other_header, buf);
            }
        }
    }
    // other_header[strlen(other_header)-1]='';
    if (!strstr(host_header, "Host:")){
        sprintf(host_header, "Host: %s\r\n", host);
    }
    // if (!strstr(other_header, "Proxy-Connection")){
        
    //     strcat(other_header, "Proxy-Connection: close\r\n");
    // }
    // if (!strstr(other_header, "Connection")){
    //     strcat(other_header, "Connection: close\r\n");
    // }

    

    sprintf(HTTPheader, "%s%s%s%s%s%s\r\n", request_hdr, user_agent_hdr, host_header, other_header, "Connection: close\r\n", "Proxy-Connection: close\r\n");

    // Host: www.cmu.edu
    // User-Agent: Mozilla/5.0 ...
    // Connection: close
    // Proxy-Connection: close
    

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