#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string>
#include <arpa/inet.h>
#include <iostream>
#include <signal.h>
#include <sstream>

using namespace std;

#define PORT "33634" // the port client will be connecting to

#define RECVBUFFERSIZE 1500

// Beej’s Guide 6.2
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
   if (sa->sa_family == AF_INET)
   {
      return &(((struct sockaddr_in *)sa)->sin_addr);
   }

   return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main()
{
   int client_sockfd, numbytes;
   // char buf[MAXDATASIZE];
   struct addrinfo hints, *servinfo, *p;
   struct sockaddr_in clientAddr;
   int rv;
   char s[INET_ADDRSTRLEN];
   char recvBuffer[RECVBUFFERSIZE]; // buffer to hold data sent from server
   int recvbytes;                   // amount of bytes the recv command gets from server
   uint16_t clientPort;


   memset(&hints, 0, sizeof hints); // clear memory for addrinfo
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;

   //Check if getaddrinfo works
   if ((rv = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) != 0)
   {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
   }

   // Beej’s Guide 6.2 start
   //Code to see results and if it has the port name -- Give credits in readme
   for (p = servinfo; p != NULL; p = p->ai_next)
   {

      // Check if socket can be used by client
      if ((client_sockfd = socket(p->ai_family, p->ai_socktype,
                                  p->ai_protocol)) == -1)
      {
         perror("client: socket");
         continue;
      }

      // Initiate connection to server
      if (connect(client_sockfd, p->ai_addr, p->ai_addrlen) == -1)
      {
         close(client_sockfd);
         perror("client: connect");
         continue;
      }

      break;
   }

   // No connection could be established from client to server
   if (p == NULL)
   {
      fprintf(stderr, "client: failed to connect\n");
      return 2;
   }

   memset(&clientAddr, 0,sizeof(clientAddr));
	socklen_t len = sizeof(clientAddr);
	getsockname(client_sockfd, (struct sockaddr *) &clientAddr, &len);
	inet_ntop(AF_INET, &clientAddr.sin_addr, s, sizeof(s));
	clientPort = ntohs(clientAddr.sin_port);

   printf("Client is up and running.\n");

   freeaddrinfo(servinfo); // free memory for linked list
   // Beej’s Guide 6.2 end

   int count = 1;
   while (true)
   {
      char country[20];
      int userId;
      memset(&country, 0, 20);
      if (count == 1)
      {
         printf("Enter Country Name: ");
         scanf("%s", country);
         printf("Enter user ID: ");
         scanf("%d", &userId);
      }
      else
      {
         printf("--------Start a new query--------\n");
         printf("Enter Country Name: ");
         scanf("%s", country);
         printf("Enter user ID: ");
         scanf("%d", &userId);      
      }


   
      stringstream ss;
      ss << userId;
      string userIdS = ss.str();

      string query = string(country) + " " + string(userIdS);
      char* queryChar = new char[query.size()+1];
      strcpy(queryChar, query.c_str());
      int len = strlen(query.c_str());

      int sendBytes = 0;
      if ((sendBytes = send(client_sockfd, queryChar, len, 0)) == -1)
      {
         perror("send");
         exit(1);
      }

      printf("Client has sent %s and User %d to Main server using TCP over port %d\n", country, userId, clientPort);

      memset(&recvBuffer, 0, sizeof recvBuffer);
      // Block using recv for server reply
      if ((recvbytes = recv(client_sockfd, recvBuffer, RECVBUFFERSIZE - 1, 0)) == -1)
      {
         perror("recv");
         exit(1);
      }
      if (recvbytes == 0)
      {
         perror("server closed connection");
         exit(1);
      }
      recvBuffer[recvbytes] = '\0';



      if (strstr(recvBuffer,"Not found") == NULL)
      {
         printf("%s is/are possible friend(s) of User %d in %s\n", recvBuffer, userId, country);
      }
      else // Country name or user if not found
      {
         printf("%s\n",recvBuffer);
      }
      memset(&country, 0, 20);
      count = 2 ;
   }
   return 0;
}