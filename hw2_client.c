#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#define PORT "3490" 
#define MAXDATASIZE 1024 

void *get_in_addr(struct sockaddr *sa);

int main(int argc, char *argv[])
{
    int s, bytes_recieved;
    char send_data[MAXDATASIZE],recv_data[MAXDATASIZE],write_data[1], write_line[MAXDATASIZE];
    struct hostent *host;
    struct sockaddr_in server_addr;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char saddr_str[INET6_ADDRSTRLEN];

    host = gethostbyname("127.0.0.1");

    if (argc != 2) 
    {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    //set server address info
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // getaddrinfo can find server info in ipv4/ipv6
    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // create socket 
    // find all result that can use , and bind the frist find result
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }

        if (connect(s, p->ai_addr, p->ai_addrlen) == -1) {
            close(s);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), saddr_str, sizeof saddr_str);

    printf("client: connecting to %s\n", saddr_str);

    // free memory space ( after use getaddrinfo() )
    freeaddrinfo(servinfo); 

    while(1)
    {   
        // receive data from server
        printf("waiting ...\n");
        bytes_recieved=recv(s,recv_data,MAXDATASIZE,0);
        recv_data[bytes_recieved] = '\0';
        sleep(1);
        if(strcmp(recv_data,"write : Please write.")==0)
        {
            // write data in file
            printf("----from server----\n%s\n",recv_data);
            printf("(enter '/q' or '/Q' to exit editor.)\n");
            scanf("%s", write_line);

            // use "/q or /Q" to end writing file
            while (strcmp(write_line, "/q")!=0 && strcmp(write_line, "/Q")!=0)
            {
                send(s, write_line, MAXDATASIZE, 0);
                scanf("%c",&write_data[0]);
                send(s,write_data,1, 0);
                scanf("%s", write_line);
            }
            send(s, write_line, MAXDATASIZE, 0);
            
            scanf("%c",&write_data[0]);
            continue;
        }
        printf("----from server----\n%s\n",recv_data);
        printf("input:");
        gets(send_data);
        send(s,send_data,strlen(send_data), 0);
    }

	return 0;
}

void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
