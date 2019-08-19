#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifdef WIN32
#include <process.h>
#include <io.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#endif

#ifdef WIN32
#define SHUT_WR SD_SEND
#endif

struct sockaddr_in c_addr;
char fname[100];

void* SendFileToClient(int *arg)
{
      int connfd=(int)*arg;
      printf("Connection accepted and id: %d\n",connfd);
      printf("Connected to Clent: %s:%d\n",inet_ntoa(c_addr.sin_addr),ntohs(c_addr.sin_port));
       //write(connfd, fname,256);

        FILE *fp = fopen(fname,"rb");
        if(fp==NULL)
        {
            printf("File opern error");
            return 1;   
        }   

        /* Read data from file and send it */
        while(1)
        {
            /* First read file in chunks of 256 bytes */
            unsigned char buff[1024]={0};
            int nread = fread(buff,1,1024,fp);
            //printf("Bytes read %d \n", nread);        

            /* If read was success, send data. */
            if(nread > 0)
            {
                //printf("Sending \n");
#ifdef WIN32
                send(connfd, buff, nread, 0);
#else
                write(connfd, buff, nread);
#endif
            }
            if (nread < 1024)
            {
                if (feof(fp))
		{
                    printf("End of file\n");
		    printf("File transfer completed for id: %d\n",connfd);
		}
                if (ferror(fp))
                    printf("Error reading\n");
                break;
            }
        }
printf("Closing Connection for id: %d\n",connfd);
close(connfd);
shutdown(connfd,SHUT_WR);
sleep(2);
}

int main(int argc, char *argv[])
{
    int connfd = 0,err;
#ifdef WIN32
    unsigned long	tid;
#else
    pthread_t tid; 
#endif
    struct sockaddr_in serv_addr;
    int listenfd = 0,ret;
    char sendBuff[1025];
    int numrv;
    size_t clen=0;
    int PORT = 5000;

#ifdef WIN32
	WSADATA            			wsaData;
	short              			wVersionRequested = 0x101;

	// Initialize Winsock version 2.2
	if (WSAStartup( wVersionRequested, &wsaData ) == -1) 
	{
	        printf("Error in socket creation\n");
		return -1;
	}
#endif

    printf("Enter file name to send: ");
    gets(fname);

    PORT = atoi(argv[1]);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd<0)
	{
	  printf("Error in socket creation\n");
	  exit(2);
	}

    printf("Socket retrieve success @ %d\n", PORT);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    ret=bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr));
    if(ret<0)
    {
      perror("bind :");
      printf("Error in bind\n");
      exit(2);
    }

    if(listen(listenfd, 10) == -1)
    {
        printf("Failed to listen\n");
        return -1;
    }

/*
if (argc < 2) 
{
	printf("Enter file name to send: ");
        gets(fname);
}
else
   strcpy(fname,argv[1]);
*/

    while(1)
    {
        clen=sizeof(c_addr);
        printf("Waiting...\n");
        connfd = accept(listenfd, (struct sockaddr*)&c_addr,&clen);
        if(connfd<0)
        {
	  printf("Error in accept\n");
	  continue;	
	}
#ifdef WIN32		
	err = createThread(SendFileToClient, 
				&connfd,
				14,
				(32*1024*1024), // stack size
				&tid);
#else		
        err = pthread_create(&tid, NULL, &SendFileToClient, &connfd);
#endif
        if (err != 0)
            printf("\ncan't create thread :[%s]", strerror(err));
   }
    close(connfd);
    return 0;
}
