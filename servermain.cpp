
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <map>
#include <utility>
#include <set>
#include <fstream>
#include <sstream>
#include <ctype.h>
#include <vector>
#include <string>
#include <algorithm>

#define UDP_PORT "32634"	// The port of the main server
#define BACKENDPORTA "30634"
#define BACKENDPORTB "31634"
#define TCP_PORT "33634" // the port users will be connecting to

#define BACKLOG 10 // how many pending connections queue will hold

#define RECVBUFFERSIZE 1500

using namespace std;


// get sockaddr, IPv4 or IPv6:
// Beej’s Guide 6.1
void *get_in_addr(struct sockaddr *sa)
{
   if (sa->sa_family == AF_INET)
   {
      return &(((struct sockaddr_in *)sa)->sin_addr);
   }

   return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// Beej’s Guide 6.1
void sigchld_handler(int s)
{
   // waitpid() might overwrite errno, so we save and restore it:
   int saved_errno = errno;

   while (waitpid(-1, NULL, WNOHANG) > 0)
      ;

   errno = saved_errno;
}

int main()
{
   int sockfd, dummysockfd, sockfdTcp, new_fd; // sockfd - UDP, sockfdTcp - TCP
   struct addrinfo hints, hintsTcp, *servinfo, *backendserverAinfo, *backendserverBinfo, *p;
   struct sockaddr_storage client_addr; // clients's address information
   int yes = 1;
   int rv; // variable to check if getaddrinfo works
   struct sigaction sa;
   struct sockaddr_storage their_addr;
   socklen_t addr_len;
   socklen_t sin_size;
   int numbytes;
   char recvBuffer[RECVBUFFERSIZE]; // for UDP recv
   char buf[RECVBUFFERSIZE]; // for TCP recv
   int recvbytes;
   int querycount = 0; // To check if we have received country lists from both server A and B
   vector<string> countryListA;
   vector<string> countryListB;
   int sent = 0;
   int tcp_port;
   int clientId;

   // Set up UDP server
   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_INET; 
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_flags = AI_NUMERICHOST;

   if ((rv = getaddrinfo("127.0.0.1", UDP_PORT, &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
   }

   // loop through all the results and make a socket
   // Beejs 6.3
   for(p = servinfo; p != NULL; p = p->ai_next) {
      if ((sockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
         perror("Main Server: socket");
         continue;
      }

      if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
      {
         close(sockfd);
         perror("Main Server: bind");
         continue;
      }


      break;
   }

   if (p == NULL) {
      fprintf(stderr, "Main Server: failed to create socket\n");
      return 2;
   }

   freeaddrinfo(servinfo);
   
   // Start TCP server
   memset(&hintsTcp, 0, sizeof hintsTcp);
   hintsTcp.ai_family = AF_INET;
   hintsTcp.ai_socktype = SOCK_STREAM;
   hintsTcp.ai_flags = AI_NUMERICHOST;

   //Check if getaddrinfo works, populate hints and servinfo structs
   // Beej’s Guide 6.1 start
   if ((rv = getaddrinfo("127.0.0.1", TCP_PORT, &hintsTcp, &servinfo)) != 0)
   {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
   }


   //Code to loop over list of available addresses and select the working one
   for (p = servinfo; p != NULL; p = p->ai_next)
   {

      // Check if socket can be used
      if ((sockfdTcp = socket(p->ai_family, p->ai_socktype,
                           p->ai_protocol)) == -1)
      {
         perror("server: socket");
         continue;
      }

      //Allow socket to reuse port
      if (setsockopt(sockfdTcp, SOL_SOCKET, SO_REUSEADDR, &yes,
                     sizeof(int)) == -1)
      {
         perror("setsockopt");
         exit(1);
      }

      //Bind socket to port
      if (bind(sockfdTcp, p->ai_addr, p->ai_addrlen) == -1)
      {
         close(sockfdTcp);
         perror("server: bind");
         continue;
      }

      break;
   }

   freeaddrinfo(servinfo); // free memory for linked list

   if (p == NULL)
   {
      fprintf(stderr, "server: failed to bind\n");
      exit(1);
   }
   else
   {
      struct sockaddr_in *saddr = (struct sockaddr_in *)p->ai_addr;
      // printf("hostname: %s\n", inet_ntoa(saddr->sin_addr));
      tcp_port = ntohs(saddr->sin_port);
   }

   // Listening to connections with error check
   if (listen(sockfdTcp, BACKLOG) == -1)
   {
      perror("listen");
      exit(1);
   }

   sa.sa_handler = sigchld_handler; // reap all dead child server processes
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = SA_RESTART;
   if (sigaction(SIGCHLD, &sa, NULL) == -1)
   {
      perror("sigaction");
      exit(1);
   }
   // Beej’s Guide 6.1 end
   // TCP server UP and listening

   printf("Main server is up and running.\n");

   // Get backend server A details
   if ((rv = getaddrinfo("127.0.0.1", BACKENDPORTA, &hints, &backendserverAinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
   }

   // Check if backendserver A is up
   for(p = backendserverAinfo; p != NULL; p = p->ai_next) {
      // Create a dummy socket
      if ((dummysockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
         continue;
      }

      // bind will fail because backendserver is already running
      if (bind(dummysockfd, p->ai_addr, p->ai_addrlen) == -1)
      {
         close(dummysockfd);
         break;
      }

      printf("Start Backend server A first\n");
      return 3;
   }

   if (backendserverAinfo == NULL) {
      fprintf(stderr, "Invalid backend server details\n");
      return 2;
   }

   // Get backend server B details
   if ((rv = getaddrinfo("127.0.0.1", BACKENDPORTB, &hints, &backendserverBinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
   }
   //Check if backend server B is up
   for(p = backendserverBinfo; p != NULL; p = p->ai_next) {
      // Create a dummy socket
      if ((dummysockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
         continue;
      }

      // bind will fail because backendserver is already running
      if (bind(dummysockfd, p->ai_addr, p->ai_addrlen) == -1)
      {
         close(dummysockfd);
         break;
      }

      printf("Start Backend server B first\n");
      return 3;
   }

   if (backendserverBinfo == NULL) {
      fprintf(stderr, "Invalid backend server details\n");
      return 2;
   }


   // Ask backend servers to send their country lists
   string backendquery = "send country names";
   if ((numbytes = sendto(sockfd, backendquery.c_str(), strlen(backendquery.c_str()), 0,
                          backendserverAinfo->ai_addr, backendserverAinfo->ai_addrlen)) == -1)
   {
      perror("Main Server: sendto serverA failed: ");
      exit(1);
   }
   

   if ((numbytes = sendto(sockfd, backendquery.c_str(), strlen(backendquery.c_str()), 0,
                          backendserverBinfo->ai_addr, backendserverBinfo->ai_addrlen)) == -1)
   {
      perror("Main Server: sendto serverB failed: ");
      exit(1);
   }

   // Ready to receive
   memset(&recvBuffer, 0, sizeof recvBuffer);
   
   /* Block using recv for server reply
   *  Receives country names from both serverA and serverB, then unblock and get ready to send user input
   */
   addr_len = sizeof their_addr;

   while ((recvbytes = recvfrom(sockfd, recvBuffer, RECVBUFFERSIZE - 1, 0,  (struct sockaddr *)&their_addr, &addr_len)) > -1) // loop to receive all data from socket
   {
      if(their_addr.ss_family == AF_INET)
      {
         int udp_port = ntohs(((struct sockaddr_in *)&their_addr)->sin_port);
         if(udp_port == 30634) {
         
            querycount++;
            recvBuffer[recvbytes] = '\0';
            stringstream countryNames(recvBuffer);
            string countryName = "";
         
            while(countryNames >> countryName) {
               countryListA.push_back(countryName);
            }
         
            // Print statement to indicate that country list has been received
            printf("Main server has received the country list from server A using UDP over port %s\n",UDP_PORT);
         
         }
         else if(udp_port == 31634) {
         
            querycount++;
            recvBuffer[recvbytes] = '\0';
            stringstream countryNames(recvBuffer);
            string countryName = "";
         
            while(countryNames >> countryName) {
               countryListB.push_back(countryName);
            }

            // Print statement to indicate that country list has been received
            printf("Main server has received the country list from server B using UDP over port %s\n",UDP_PORT);            
            
         }
         if(querycount == 2) { // exit out of a blocking recv call
            break;
         }
      }
      else {
         printf("Use IPV4 ordering\n");
         return 4;
      }
   }

   // Print out country names received
   cout << endl << "Server A" << endl;
   for(int i = 0; i < countryListA.size(); i++) {
      cout << countryListA[i] << endl;
   }
   cout << endl;
   
   cout << "Server B" << endl;
   for(int j = 0; j < countryListB.size(); j++) {
      cout << countryListB[j] << endl;
   }
   cout << endl;

   // Start Accepting TCP connections
   while(true) {
      sin_size = sizeof client_addr;
      new_fd = accept(sockfdTcp, (struct sockaddr *)&client_addr, &sin_size);
      if (new_fd == -1)
      {
         perror("accept error");
         continue;
      }
      clientId++; // indicating client number

      if(fork() == 0) {
         close(sockfdTcp);

         memset(&buf, 0, sizeof buf);

         int client_port = ntohs(((sockaddr_in *)&client_addr)->sin_port);

         while ((recvbytes = recv(new_fd, buf, RECVBUFFERSIZE - 1, 0)) > 0)
         {
            buf[recvbytes] = '\0';

            char *token;
            char *country;
            char *userId;

            // Parse country and userId from query
            char *query = new char[strlen(buf)+1];
            strcpy(query, buf);

            token = strtok(query, " ");
            country = new char[strlen(token)+1];
            strcpy(country, token);
            token = strtok(NULL, " ");
            userId = new char[strlen(token)+1];
            strcpy(userId, token);

            printf("\nMain server has received request on User %s in %s from client %d using TCP over port %d\n", userId, country, clientId, tcp_port);
            if(count(countryListA.begin(), countryListA.end(), country)) { // Check if country present in Server A
               
               printf("%s shows up in server A\n", country);
               printf("Main Server has sent request of User %s to server A using UDP over port %s\n", userId, UDP_PORT);
               
               if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
                     backendserverAinfo->ai_addr, backendserverAinfo->ai_addrlen)) == -1) {
                  perror("Main Server: sendto");
                  exit(1);
               }

               sent = 1;         

            }
            else if(count(countryListB.begin(), countryListB.end(), country)) { // Check if country present in Server B

               printf("%s shows up in server B\n", country);
               printf("Main Server has sent request of User %s to server B using UDP over port %s\n", userId, UDP_PORT);

               if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
                     backendserverBinfo->ai_addr, backendserverBinfo->ai_addrlen)) == -1) {
                  perror("Main Server: sendto");
                  exit(1);
               }

               sent = 1;        
      
            }
            else {
               char countrynotfound[40];
               printf("%s does not show up in server A&B\n", country);
               // send to client that country does not exist
               strcat(countrynotfound,country);
               strcat(countrynotfound,": Not found");
               if(send(new_fd,countrynotfound,strlen(countrynotfound),0) == -1) {
                  perror("send");
               }

               printf("Main Server has sent \"%s: Not found\" to client %d using TCP over port %d\n", country, clientId, tcp_port);
               sent = 0;
            }
            if(sent) {               
               // Reset recvbuffer and number of received bytes
               memset(&recvBuffer, 0, sizeof recvBuffer);
               recvbytes = 0;      
               
               // Receive reply from backend server A or B
               if ((recvbytes = recvfrom(sockfd, recvBuffer, RECVBUFFERSIZE - 1, 0,  (struct sockaddr *)&their_addr, &addr_len)) == -1) // loop to receive all data from socket
               {
                  perror("recv");
                  exit(1);
               }
               if(their_addr.ss_family == AF_INET)
               {
                  int udp_port = ntohs(((struct sockaddr_in *)&their_addr)->sin_port);
                  recvBuffer[recvbytes] = '\0';
                  if(udp_port == 30634) { // If recv from backendserverA
                     if(strstr(recvBuffer, "Not found") != NULL) { // UserId not found 
                        printf("Main server has received \"%s\" from server A\n", recvBuffer);
                        // send to client that user id does not exist
                        if(send(new_fd,recvBuffer,strlen(recvBuffer),0) == -1) {
                          perror("send");
                        }
                        printf("Main server has sent message to client %d using TCP over %d\n", clientId, tcp_port);
                     }
                     else {
                        printf("Main server has received searching result of User %s from server A\n", userId);
                        // send to client that user id does not exist
                        if(send(new_fd,recvBuffer,strlen(recvBuffer),0) == -1) {
                          perror("send");
                        }
                        printf("Main server has sent searching result(s) to client %d using TCP over %d\n", clientId, tcp_port);
                     }
                  }
                  else if(udp_port == 31634) { // If recv from backendserverB
                     if(strstr(recvBuffer, "Not found") != NULL) { // UserId not found 
                        printf("Main server has received \"%s\" from server B\n", recvBuffer);
                        // send to client that user id does not exist
                        if(send(new_fd,recvBuffer,strlen(recvBuffer),0) == -1) {
                          perror("send");
                        }                        
                        printf("Main server has sent message to client %d using TCP over %d\n", clientId, tcp_port);
                     }
                     else {
                        printf("Main server has received searching result of User %s from server B\n", userId);
                        // send to client possible friends
                        if(send(new_fd,recvBuffer,strlen(recvBuffer),0) == -1) {
                          perror("send");
                        }                        
                        printf("Main server has sent searching result(s) to client %d using TCP over %d\n", clientId, tcp_port);
                     }                            
                  }
                  else {
                     printf("Unkown sender\n");
                     return 5;
                  }
               }
               else {
                  printf("Use IPV4 ordering\n");
                  return 4;
               }
            }
         }
         if ((recvbytes = recv(new_fd, buf, RECVBUFFERSIZE - 1, 0)) == -1)
         {
            perror("recv");
            exit(1);
         }
         if ((recvbytes = recv(new_fd, buf, RECVBUFFERSIZE - 1, 0)) == 0) // When killed sends 0 indicating remote connection is closed
         {
            close(new_fd); // Close child TCP
            close(sockfd); // Close child UDP
            return 0;
         }
      }
      else {
         close(new_fd);
      }
   }
   close(sockfdTcp);
   close(sockfd);

   freeaddrinfo(backendserverAinfo);
   freeaddrinfo(backendserverBinfo);
   return 0;
}