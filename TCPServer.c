#include <stdio.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <limits.h>

#define FAILURE    -1
#define MAX_EVENTS 3000

int OpenListener(int port)
{
  int sd;
  struct sockaddr_in addr;
  int reuse = 1;

  sd = socket(PF_INET, SOCK_STREAM, 0);

  if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,(char *)&reuse, sizeof(reuse)) < 0)
  {
    printf("\nSO_REUSEADDR failed..");
    close(sd);
    exit(1);
  }

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if ( bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 )
  {
      perror("can't bind port");
      abort();
  }
  if ( listen(sd, SOMAXCONN) != 0 )
  {
      perror("Can't configure listening port");
      abort();
  }
  printf("Server Listening on port Number :%d\n",port);
  return sd;
}

int process_client_data(int *client)
{
  int bytes,s,device_id = 0,rv;
  int client_index = -1, exit_thread = 0;
  struct timeval tv;
  fd_set readset;

  struct epoll_event event;
  char **dev_msg, buf[1024], *cuid;
  int tokens, i;

  while(!exit_thread)
  {
    FD_ZERO(&readset);
    FD_SET(*client, &readset);

    rv = select(*client + 1, &readset, NULL, NULL, NULL);

    if(rv < 0)
    {
      printf("Select Error occured %d\n",errno);
    }
    else
    {
      if(FD_ISSET(*client, &readset))
      {
        bytes = read(*client, buf, sizeof(buf));
        if(bytes <= 0)
        {
          /* Read error/client closed connection. Close socket */
          printf("Read error. Closing socket connection\n");
          exit_thread = 1;
        }
        else
        {
          char *tmp;
          printf("Data Received:%s\n",buf);
        }
      }
    }
  }
  return 0;
}


int main(int count, char *strings[])
{
  int serverSock;
  int epollWaitRet;
  int loopVar;
  int epollCtlRet;
  char *portnum;
  struct epoll_event *epoll_events;
  struct epoll_event event;
  //sigset_t saved_mask, sigpipe_mask;
  int efd;
  int *client;
  struct sockaddr_in addr;
  int len;
  int rc;
  pthread_attr_t attr;
  pthread_t thread;

  if ( count != 2 )
  {
    printf("Usage: %s <portnum>\n", strings[0]);
    exit(0);
  }

  portnum = strings[1];

  /* Create Server Socket */
  serverSock = OpenListener(atoi(portnum));
  if(FAILURE == serverSock)
  {
    printf("FATAL:Error creating server socket.\n");
    exit(1);
  }

  efd = epoll_create1 (0);
  if (FAILURE == efd)
  {
    printf ("epoll_create error");
    abort ();
  }

  /* Add server socket to events */
  event.data.fd = serverSock;
  event.events = EPOLLIN;
  epollCtlRet = epoll_ctl (efd, EPOLL_CTL_ADD, serverSock, &event);
  if (FAILURE == epollCtlRet)
  {
    perror ("epoll_ctl1");
    printf("epoll_ctl error: %d\n",errno);
    exit (1);
  }
  
  epoll_events = calloc(MAX_EVENTS,sizeof event);
  printf("Going to while loop\n");
  while(1)
  {
    epollWaitRet = epoll_wait(efd, epoll_events, MAX_EVENTS, -1);
    if(epollWaitRet < 0)
    {
      printf("EPOLL ERROR HERE\n");
      exit(0);
    }

    for(loopVar = 0; loopVar < epollWaitRet; loopVar++)
    {
      if ((epoll_events[loopVar].events & EPOLLERR) ||
          (epoll_events[loopVar].events & EPOLLHUP) ||
          (!(epoll_events[loopVar].events & EPOLLIN)))
      {
        printf("Epoll Error\n");
      }
      else if(serverSock == epoll_events[loopVar].data.fd)
      {
        client = (int *)malloc(sizeof(int));
        *client = accept(serverSock, (struct sockaddr*)&addr, &len);  /* accept connection as usual */
        if(client < 0)
        {
          printf("\nSocket accept failed %d",errno);
        }
        else
        {
          printf("Connection: %s:%d\n",(char *)inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        }

        rc = pthread_attr_init(&attr);
        if(rc == -1)
        {
          printf("\nFatal: Pthread attr create failed!! %d\n",errno);
          close(*client);
          free(client);
        }
        else
        {
          rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
          if(rc == -1)
          {
            printf("\nFatal: Pthread attr set detached failed!! %d\n",errno);
            close(*client);
            free(client);
          }
          else
          {
            rc = pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
            if(rc == -1)
            {
              printf("\nFatal: Pthread attr set stacksize failed!! %d\n",errno);
            }
            if(pthread_create(&thread, &attr, process_client_data, client))
            {
              printf("\nThread create failed. Cannot accept client connection");
            }
          }
        }
      }
    }
  }
}

