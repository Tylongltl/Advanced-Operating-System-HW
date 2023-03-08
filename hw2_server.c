#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#define Port "3490"
#define Backlog 10  // at most 10 client can connect to server
#define User_amout 30
#define FlistSize 10
#define BuffSize 1024
#define MAXLINESIZE 1024

struct file{

	char fName[20];//file name
	char gName[20];//group name
	char right[10];//access right
	int lockbit;//lock
};

struct user{

	char uName[20];//user name
	struct file fList[FlistSize];//file list

}*uList;

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);
int findID(char *uname);
int findFile(char *fname , int *ulistNum, int *flistNum);
int createFile(int idnum,char *fname,char *right,char *group);
int checkRight(int idnum, char *usr,char *group,char *fname,char rw, int *ulistNum, int *flistNum);
int changeFile(char *fname,char *right,char *usr, int *ulistNum, int *flistNum);

int main()  {
    // mmap let father & children processes matain same file
    uList = mmap(NULL, User_amout*sizeof(struct user), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    struct addrinfo hints, *servinfo, *p;
    int sock, sock_client, yes=1, status;
    struct sockaddr_storage their_addr;
	socklen_t addr_size;
	struct sigaction sa;
	char send_data[BuffSize], recv_data[BuffSize], temp[BuffSize], buffer[BuffSize], c[1], write_line[MAXLINESIZE] ;
	char *usrName, *grpName, *cmd, *fName, *rightStr;
    int idNum, uListNum, fListNum;
    char caddr_str[INET6_ADDRSTRLEN];

	//set server address info
	memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // getaddrinfo can find server info in ipv4/ipv6
    if ((status = getaddrinfo(NULL, Port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // create socket 
    // find all result that can use , and bind the frist find result
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
          perror("Server: Socket error");
          continue;
        }

        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
          perror("Server: Setsockopt error");
          exit(1);
        }

        if (bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
          close(sock);
          perror("server: Bind error");
          continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    // free memory space ( after use getaddrinfo() )
    freeaddrinfo(servinfo); 

    // set state to wait client in TCP connect
    if (listen(sock, Backlog) == -1)
    {
        perror("Listen error");
		exit(1);
    }

    // use signal function let father & children process can communicate
    // kill all failure process
    sa.sa_handler = sigchld_handler; 
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("Waiting for connections ... \n");
	fflush(stdout);

	while (1)
    {
        int bytes_recieved, fSize;
        char *msg;
        // accept connection form TCP connect
        addr_size = sizeof(their_addr);
        FILE *pfile;
        sock_client = accept(sock, (struct sockaddr *)&their_addr, &addr_size);
        if (sock_client == -1) {
            error("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), caddr_str, sizeof(caddr_str));
        printf("\t[Info] Receive connection from %s...\n", caddr_str);

        switch(fork())
        {
            case -1:
                perror("Fork error");
                exit(1);

            case 0:
                msg = "Enter your ID and Group.";
                send(sock_client, msg, strlen(msg), 0);
                printf("Waiting for client ...\n");
                sleep(1);
                bytes_recieved = recv(sock_client, recv_data, BuffSize, 0);
                printf("recv()'d %d bytes of data in buf\n", bytes_recieved);
                recv_data[bytes_recieved] = '\0';
                sleep(1);

                strcpy(temp,recv_data);
                usrName = strtok(temp, " ");
                idNum = findID(usrName);
                grpName = strtok(NULL," ");
                //int send(int sockfd, const void *msg, int len, int flags);
                send(sock_client, "Enter your command.", 30,0);

                while(1)
                {
                    uListNum = -1;
                    fListNum = -1;
                    printf("Waiting for client ...\n");
                    bytes_recieved = recv(sock_client, recv_data, BuffSize, 0);
                    printf("recv()'d %d bytes of data in buf\n", bytes_recieved);
                    recv_data[bytes_recieved] = '\0';
                    sleep(1);
                    cmd = strtok(recv_data," ");

                    // command : create new file 
                    if(strcmp(cmd,"create") == 0)
                    {
                        fName = strtok(NULL," ");
                        rightStr = strtok(NULL," ");

                        if(fName == NULL || rightStr == NULL)
                        {
                            printf("create : input error \n");
                            send(sock_client,"create : wrong input!",30,0);
                            continue;
                        }

                        // access right string eg: rwrw--
                        if (strlen(rightStr) != 6)
                        {
                            printf("create : input error \n");
                            send(sock_client,"create : Access right's string length is not enough, please check again!", 70,0);
                            continue;
                        }

                        printf("create : %s right : %s\n", fName, rightStr);
                        // find the file is exist or not 
                        if(!findFile(fName, &uListNum, &fListNum))
                        {
                            pfile = fopen(fName,"w");
                            fclose(pfile);
                            if(createFile(idNum, fName, rightStr, grpName) == -1)
                            {
                                printf("create : too much files \n");
                                send(sock_client,"create : too much files.", 30, 0);
                                continue;
                            }
                            else
                                send(sock_client,"create : finished.", 30, 0);
                        }
                        else
                            send(sock_client,"create : The file already existed.", 50, 0);

                    }//end create a file
                    // command : read a file
                    else if(strcmp(cmd,"read") == 0)
                    {
                        fName = strtok(NULL," ");

                        if(fName == NULL)
                        {
                            printf("read : input error \n");
                            send(sock_client,"read : wrong input!",30,0);
                            continue;
                        }
                        printf("read : %s\n",fName);
                        // find the file is exist or not 
                        if(findFile(fName, &uListNum, &fListNum))
                        {   
                            // check user's access right that can read file
                            if(checkRight(idNum, usrName, grpName, fName, 'r', &uListNum, &fListNum) == 1)
                            {
                                if(uList[uListNum].fList[fListNum].lockbit != -1)
                                {
                                    printf("---readFile---\n");
                                    uList[uListNum].fList[fListNum].lockbit++;
                                    pfile = fopen(fName, "r");
                                    fseek(pfile, 0, SEEK_END);
                                    fSize = ftell(pfile);
                                    rewind(pfile);  
                                    //set pointer to the beginning of file
                                    char* cBuff = (char *)malloc((fSize+100) * sizeof(char));
                                    fread(cBuff, sizeof(char), fSize, pfile);
                                    cBuff[fSize] = '\0';
                                    fclose(pfile);

                                    if(fSize == 0)
                                    {
                                        cBuff[0] = '\0';
                                    }

                                    strcat(cBuff,"----end file----\nPlease enter 'end' to end read\n");

                                    send(sock_client, cBuff, strlen(cBuff), 0);

                                    bytes_recieved = recv(sock_client,recv_data,1024,0);
                                    recv_data[bytes_recieved] = '\0';

                                    if(strcmp(recv_data,"end") == 0)
                                        send(sock_client,"read : finished.\nPlease enter next command.",50,0);
                                    else
                                        send(sock_client,"read : This is not 'end'\nPlease enter next command.",60,0);

                                    uList[uListNum].fList[fListNum].lockbit--;
                                    free(cBuff);
                                }
                                else
                                    send(sock_client,"read : Someone is writting this file.",60,0);
                            }//end checkright
                            else
                                send(sock_client,"read : You have no right to read.",50,0);
                        }//end findfile
                        else
                            send(sock_client,"read : The file does not exist.",50,0);
                    } //end read file
                    // command : write a file o/a
                    else if(strcmp(cmd,"write") == 0)
                    {
                        fName=strtok(NULL," ");
                        rightStr=strtok(NULL," ");

                        if(fName==NULL || rightStr==NULL)
                        {
                            printf("create : input error \n");
                            send(sock_client,"create : wrong input!", 30, 0);
                            continue;
                        }

                        printf("write : %s\n",fName);
                        // find the file is exist or not 
                        if(findFile(fName, &uListNum, &fListNum))
                        {   
                            // check user's access right that can read file
                            if(checkRight(idNum, usrName, grpName, fName, 'w', &uListNum, &fListNum) == 2)
                            {
                                if(uList[uListNum].fList[fListNum].lockbit == 0)
                                {
                                    // set lockbit = -1 to lock file avoiding other user write the file
                                    uList[uListNum].fList[fListNum].lockbit = -1;
                                    send(sock_client,"write : Please write.", 30, 0);

                                    recv_data[bytes_recieved] = '\0';
                                    if(strcmp(rightStr,"o") == 0)//overwrite
                                    {
                                        pfile = fopen(fName,"w");
                                        bytes_recieved = recv(sock_client, write_line, MAXLINESIZE, 0);
                                        
                                        // use "/q or /Q" to end writing file
                                        while (strcmp(write_line, "/q")!=0 && strcmp(write_line, "/Q")!=0)
                                        {
                                            fprintf(pfile, "%s", write_line);
                                            bytes_recieved = recv(sock_client, c, 1, 0);
                                            fprintf(pfile,"%c",c[0]);
                                            bytes_recieved = recv(sock_client, write_line, MAXLINESIZE, 0);

                                        }
                                    }
                                    else if(strcmp(rightStr,"a") == 0)//append
                                    {
                                        pfile = fopen(fName,"a");
                                        bytes_recieved = recv(sock_client, write_line, MAXLINESIZE, 0);

                                        // use "/q or /Q" to end writing file
                                        while (strcmp(write_line, "/q")!=0 && strcmp(write_line, "/Q")!=0)
                                        {
                                            fprintf(pfile, "%s", write_line);
                                            bytes_recieved = recv(sock_client, c, 1, 0);
                                            fprintf(pfile,"%c",c[0]);
                                            bytes_recieved = recv(sock_client, write_line, MAXLINESIZE, 0);
                                        }
                                    }
                                    fclose(pfile);
                                    uList[uListNum].fList[fListNum].lockbit=0;
                                    send(sock_client,"write : Done.",30,0);

                                }//end check lockbit
                                else
                                    send(sock_client,"write : Someone is reading or writing this file, please try again later.", 100, 0);

                            }//end checkright
                            else
                                send(sock_client,"write : You have no right to write.",50,0);
                        }//end findFile
                        else
                            send(sock_client,"The file does not exist.",30,0);
                    }//end write file
                    // command : changemode : change file access fight
                    else if(strcmp(cmd,"changemode") == 0)   
                    {
                        fName=strtok(NULL," ");
                        rightStr=strtok(NULL," ");

                        if(fName==NULL || rightStr==NULL)
                        {
                            printf("change : input error \n");
                            send(sock_client,"change : wrong input!",30,0);
                            continue;
                        }

                        // find the file is exist or not 
                        if(findFile(fName, &uListNum, &fListNum))
                        {
                            if(changeFile(fName, rightStr, usrName, &uListNum, &fListNum))
                            {
                                send(sock_client,"change : The file's right is changed.", 40,0);
                            }
                            else
                                send(sock_client,"change : You have no right to change this file.",50,0);

                        }
                        else
                            send(sock_client,"change : The file does not exist.",50,0);
                    } //end change file mode
                    else
                    {
                        char msg[60];
                        sprintf(msg, "%s: command not found", cmd);
                        send(sock_client, msg, 60, 0);
                    }


                    fflush(stdout);
                    sleep(1);
                }//end command
                break;  //break switch

            default:
                close(sock_client);
                break;
        }//end switch
    }//end while
    close(sock);
  	return 0;
}

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// 取得sockaddr，IPv4或IPv6：
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int findID(char *uname)
{
	int i;

	for(i=0;i<User_amout;i++)
	{
		if(strcmp(uList[i].uName, uname)==0)
			return i;
	}
	for(i=0;i<User_amout;i++)
	{
		if(strlen(uList[i].uName)==0)
		{
			strcpy(uList[i].uName, uname);
			return i;
		}
	}

	return -1;

}

int findFile(char *fname, int *ulistNum, int *flistNum)
{
	int i,j;

	for(i = 0; i < User_amout; i++)
		for(j = 0; j < FlistSize; j++)
			if(strcmp(uList[i].fList[j].fName, fname) == 0)
			{
				*ulistNum = i;
				*flistNum = j;
				return 1;
			}

	return 0;

}

int createFile(int idnum,char *fname,char *right,char *group)
{

	int i;

	for(i=0; i<FlistSize; i++)
	{
		if(strlen(uList[idnum].fList[i].fName)==0)
		{
			strcpy(uList[idnum].fList[i].fName, fname);
			strcpy(uList[idnum].fList[i].right, right);
			strcpy(uList[idnum].fList[i].gName, group);
			uList[idnum].fList[i].lockbit = 0;
			return i;
		}
	}

	return -1;
}

int checkRight(int idnum, char *usr,char *group,char *fname,char rw, int *ulistNum, int *flistNum)
{
	int i,j,r=0;

	if(rw=='w')
		r=1;

	if(*ulistNum==-1||*flistNum==-1)
		if(findFile(group, ulistNum, flistNum)==0)
			return -1;


	if(idnum==*ulistNum && uList[*ulistNum].fList[*flistNum].right[r]==rw)
	{
		if(rw=='r')
			return 1;
		else if(rw=='w')
			return 2;
	}
	else if(strcmp(uList[*ulistNum].fList[*flistNum].gName, group)==0)
	{
		printf("checking group member\n");
		if(uList[*ulistNum].fList[*flistNum].right[r+2]==rw)
		{
			if(rw=='r')
				return 1;
			else if(rw=='w')
				return 2;
		}
	}
	else
	{
		if(uList[*ulistNum].fList[*flistNum].right[r+4]==rw)
		{
			if(rw=='r')
                return 1;
            else if(rw=='w')
                return 2;
		}

	}

	return -1;

}

int changeFile(char *fname,char *right,char *usr, int *ulistNum, int *flistNum)
{
	if(*ulistNum==-1||*flistNum==-1)
		if(findFile(fname, ulistNum, flistNum)==0)
			return 0;

	if(strcmp(uList[*ulistNum].uName, usr) == 0)
	{
		strcpy(uList[*ulistNum].fList[*flistNum].right, right);
		return 1;
	}
	return 0;

}
