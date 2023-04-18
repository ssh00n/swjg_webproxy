#include <stdio.h>
#include "csapp.h"


/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define TINY_HOST "localhost"


void doit(int fd);
// void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *hostname, char *path, char *port);
void read_requesthdrs(rio_t *rp, char* HTTPheader, char *host, char *port, char *path);
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
    char request_buf[MAXLINE], method[10], host[MAXLINE], uri[MAXLINE], port[10], path[MAXLINE], version[10];
    rio_t rio, rio2;
    int targetfd;
    char HTTPheader[MAXLINE];
    char buf[MAXLINE];

    strcpy(HTTPheader, "");

    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, request_buf, MAXLINE);

    sscanf(request_buf, "%s %s %s", method, uri, version);
    // printf("%s", request_buf);

    parse_uri(uri, host, path, port);


    read_requesthdrs(&rio, HTTPheader, host, port, path);

    // printf("HTTPheader:\n%s", HTTPheader);
    
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Invalid request", "");
        return;
    }
    

     // Establish a connection to the target server

    targetfd = Open_clientfd(TINY_HOST, port);

    Rio_writen(targetfd, HTTPheader, strlen(HTTPheader));

    char forward_buf[MAXLINE];
    char response_buf[MAXLINE];
    char response_header[MAXLINE];
    char response_content[MAXLINE];
    
    int content_length;
    // parse response_header
    sprintf(response_header, "");
    char *payload;

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

    // while(Rio_readlineb(&rio2, response_buf, MAXLINE)){
    //     if (strcmp(response_buf, "\r\n") == 0){
    //         break;
    //     } 
    //     strcat(response_content, response_buf);
    // }

    printf("%s", response_content);

    Rio_writen(fd, response_header, strlen(response_header));
    Rio_writen(fd, payload, content_length);
    close(targetfd);
        // Getaddrinfo(hostname, NULL, )

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
            strcpy(path, "");
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
            if (strcmp(buf, "\r\n"))
                break;
            strcat(other_header, buf);
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