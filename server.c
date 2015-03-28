// simple_proxy
// Jonathan Yang - the.jonathan.yang@gmail.com

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/wait.h>

#define MAX_LEN 2048
#define BACKLOG 10

// name of this program
char *basename;

// no zombies allowed
void handler(int signo)
{
  pid_t signal;
  if (signo == SIGCHLD) signal = wait(NULL);
}

// looks up hostname with gethostbyname() and connects accordingly
int connect_to_host(char *address)
{
  int sock_fd;
  int check;
  struct sockaddr_in server_addr;
  struct hostent *host_entry;
  
  host_entry = gethostbyname(address);
  
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(80);
  memcpy(&server_addr.sin_addr, host_entry->h_addr_list[0], sizeof(struct in_addr)); 

  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    perror("socket");
    exit(1);
  }

  check = connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (check < 0) return check;
            else return sock_fd;  
}

// parses http requests for GET and HEAD commands. i probably could have
// done this better
void parse_message(int sock_fd, char *buffer, char *ip)
{
  int host_fd;
  int recvd;
  int check;
  char command[MAX_LEN];
  char url[MAX_LEN];
  char http[MAX_LEN];
  char host[MAX_LEN];
  char temp[MAX_LEN];
  char *thing_to_get;

  sscanf(buffer, "%s %s %s", command, url, http);

  if (strcmp(command,"GET") == 0 || strcmp(command, "HEAD") == 0) {
    
    strcpy(temp, url);
    
    thing_to_get = strtok(buffer, "//");
    thing_to_get = strtok(NULL, "/");
    
    strcpy(host, thing_to_get);
    
    thing_to_get = strtok(temp, "//");
    thing_to_get = strtok(NULL, "/");
    thing_to_get = strtok(NULL, "\0");
    
    memset(buffer, 0, MAX_LEN);
        
    if (thing_to_get == NULL)
      sprintf(buffer, "%s / %s\r\nHost: %s\r\nConnection: close\r\n\r\n",
              command, http, host);
    else
      sprintf(buffer, "%s /%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",
              command, thing_to_get, http, host);

    printf("%s is requesting %s\n", ip, url);

    host_fd = connect_to_host(host);
    if (host_fd < 0) {
      perror("connect");
      return;
    }

    check = send(host_fd, buffer, strlen(buffer), 0);
    if (check < 0) perror("send");

    memset(buffer, 0, MAX_LEN);
  
    while ((recvd = recv(host_fd, buffer, MAX_LEN, 0)) > 0) {
      if (send(sock_fd, buffer, recvd, 0) <= 0) {
        perror("send");
        memset(buffer, 0, MAX_LEN);
        return;
      }
      memset(buffer, 0, MAX_LEN);
    }

    if (recvd < 0) perror("recv");

    memset(buffer, 0, MAX_LEN);
    close(host_fd);
  } else {
    memset(buffer, 0, MAX_LEN);
    sprintf(buffer, 
            "<!DOCTYPE html><html lang=en><meta charset=utf-8><title>501:\
            not supported</title><ins>501: not supported</ins></html>");
    if (send(sock_fd, buffer, strlen(buffer), 0) <= 0) perror("send");
    _exit(0);
  }
}

int main(int argc, char **argv)
{ 
  int listen_fd;
  int new_fd;
  int nbytes;
  pid_t pid;

  char buffer[MAX_LEN];
  char ip[MAX_LEN];

  struct sockaddr_in server_addr;
  struct sockaddr_in inc_addr;
  socklen_t inc_addr_size;

  basename = argv[0];

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port number>\n", basename);
    fflush(NULL);
    exit(EXIT_FAILURE);
  }

  signal(SIGCHLD, handler);

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(argv[1]));
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  
  if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }
  
  if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    close(listen_fd);
    perror("bind");
    exit(1);
  }
  
  if (listen(listen_fd, BACKLOG) < 0) {
    perror("listen");
    exit(1);
  }

  for (;;) {
    new_fd = accept(listen_fd, (struct sockaddr *)&inc_addr, &inc_addr_size);
    if (new_fd < 0) {
      perror("accept");
      continue;
    }

    if ((pid = fork()) == 0) {
      close(listen_fd);

      inet_ntop(AF_INET, &(inc_addr.sin_addr), ip, INET_ADDRSTRLEN);

      while ((nbytes = recv(new_fd, buffer, MAX_LEN, 0)) > 0) 
        parse_message(new_fd, buffer, ip);

      if (nbytes < 0) {
        perror("recv");
        _exit(1);
      }

      _exit(0);
    }

    close(new_fd);
  }
}
