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
#include <map>
#include <utility>
#include <set>
#include <vector>
#include <fstream>
#include <sstream>
#include <ctype.h>
#include <string>
#include <algorithm>

using namespace std;

#define PORT "30634" // The port users will be connecting to

#define MAXBUFLEN 1500 // Max number of bytes we can get at once

// Load the data file
void loadfile (map<string, vector< vector<int> > > &dataBase) {
   

   string dataFileLocation = "./dataA.txt";
   ifstream dataFile(dataFileLocation.c_str());
   string data;
   string countryName;

   // Read data to load serverid and countries
   while (!dataFile.eof())
   {
      char ch;
      ch = dataFile.peek();
      if (!isdigit(ch))
      {
         getline(dataFile, data);
         stringstream countryNameString(data);
         countryNameString >> countryName;
      }
      while(isdigit(dataFile.peek()))
      {
         vector<int> interestGroup;
         getline(dataFile, data);
         stringstream tokenizer(data);
         int userId;
         while(tokenizer >> userId) {
            interestGroup.push_back(userId);
         }
         dataBase[countryName].push_back(interestGroup);
      }
   }

}

// Check if userId is present or not in the country
int findId(const vector<vector<int> > &interestGroups, int userId) {
   for(int i = 0 ; i < interestGroups.size(); ++i) {
      for(int j = 0; j < interestGroups[i].size(); ++j) {
         if(interestGroups[i][j]== userId) {
            return 1;
         }
      }
   }
   return 0;
}

// Populate recommended userIds of friends
void recommend(const vector< vector<int> > &interestGroups,int userId, set<int> &recommendedUsers) {
   for(size_t i = 0; i < interestGroups.size(); ++i) {
      if (find(interestGroups[i].begin(), interestGroups[i].end(), userId) != interestGroups[i].end()) {
         recommendedUsers.insert(interestGroups[i].begin(), interestGroups[i].end());
      }
   }
   recommendedUsers.erase(userId);
}

int main()
{
   int sockfd;
   struct addrinfo hints, *servinfo, *p;
   int rv;
   int numbytes;
   int recvbytes;
   struct sockaddr_storage their_addr;
   char buf[MAXBUFLEN];
   socklen_t addr_len;
   char s[INET_ADDRSTRLEN];
   int server_port;
   map<string, vector< vector<int> > > dataBase;
   map<string, vector< vector<int> > >::iterator it;

   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_flags = AI_NUMERICHOST;

   if ((rv = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) != 0)
   {
      fprintf(stderr, "Server A: getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
   }

   // loop through all the results and bind to our port
   // Beejs 6.3
   for (p = servinfo; p != NULL; p = p->ai_next)
   {
      if ((sockfd = socket(p->ai_family, p->ai_socktype,
                           p->ai_protocol)) == -1)
      {
         perror("Server A: socket");
         continue;
      }

      if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
      {
         close(sockfd);
         perror("Server A: bind");
         continue;
      }

      break;
   }

   if (p == NULL)
   {
      fprintf(stderr, "Server A: failed to bind socket\n");
      return 2;
   }
   else
   {
      struct sockaddr_in *saddr = (struct sockaddr_in *)p->ai_addr;
      server_port = ntohs(saddr->sin_port);

   }

   freeaddrinfo(servinfo); // free memory for linked list
   
   // Server is up and running
   printf("Server A is up and running using UDP on port %d\n", server_port);

   loadfile(dataBase);

   string countryNames = "";

   // Loop over all country names and store as a single string
   for (it = dataBase.begin(); it != dataBase.end(); it++)
   {
      countryNames = countryNames + " " + it->first;
   }

   addr_len = sizeof their_addr;

   while ((recvbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
                               (struct sockaddr *)&their_addr, &addr_len)) > 0)
   {
      buf[recvbytes] = '\0';
      char* countrynames = new char[countryNames.size()+1];
      strcpy(countrynames, countryNames.c_str());

      // If the main server is asking to send country names send them
      if(strcmp(buf,"send country names") == 0) {
         int sendLen = strlen(countryNames.c_str());
         int currentSent = 0;


         // See that all of the data is sent
         while(currentSent < sendLen) {
            if ((numbytes = sendto(sockfd, countrynames, sendLen - currentSent, 0,
                                 (struct sockaddr *)&their_addr, addr_len)) == -1)
            {
               perror("Server A: send to main server failed ");
               exit(1);
            }
            currentSent = currentSent + numbytes;
            countrynames += currentSent;
         }

         printf("Server A has sent a country list to Main Server\n\n");
      }
      else {
         // Normal query not initial setup
         // buf = <country name> <userId>
         char *token;
         char *country;
         char *userIds;

         // Parse country and userId from query
         char *query = new char[strlen(buf)+1];
         strcpy(query, buf);

         token = strtok(query, " ");
         country = new char[strlen(token)+1];
         strcpy(country, token);
         token = strtok(NULL, " ");
         userIds = new char[strlen(token)+1];
         strcpy(userIds, token);
         int userId;
         stringstream(userIds) >> userId;

         printf("\nServer A has received a request for finding possible friends of User %s in %s\n", userIds, country);
         vector< vector<int> > interestGroups = dataBase.at(country);
         int found = findId(interestGroups, userId);
         set<int> recommendedUsers;
         string response;

         if(!found) { // User not found
            printf("User %s does not show up in %s\n", userIds, country);
            // send reply to mainserver
            response = "User " + string(userIds) + ": Not found";  
         }
         else {
            recommend(interestGroups, userId, recommendedUsers);
            string userIdString = "";
            string wouserString = "";

            if(recommendedUsers.empty()) {
               response = "None";
            }
            else {
               set<int>::iterator it = recommendedUsers.begin();

               // Stringify userIDs  - wouseString = <userId1>, <userId2>..., userIdString = User <userId1>, User <userId2>....
               while (it != recommendedUsers.end())
               {
                  stringstream ss;
                  ss << (*it);
                  string userId = ss.str();
                  userIdString += "User " + userId + ", ";
                  wouserString += userId + ", ";
                  it++;
               }
               userIdString = userIdString.substr(0,userIdString.size()-2);
               wouserString = wouserString.substr(0,wouserString.size()-2);
               response = userIdString;
            }
            printf("Server A found the following possible friends for User %s in %s: %s\n", userIds, country, wouserString.c_str());
         }
         
         char* responsechar = new char[response.size()+1];
         strcpy(responsechar, response.c_str());

         int sendLen = strlen(response.c_str());
         int currentSent = 0;


         // See that all of the data is sent
         while(currentSent < sendLen) {
            if ((numbytes = sendto(sockfd, responsechar, sendLen - currentSent, 0,
                                 (struct sockaddr *)&their_addr, addr_len)) == -1)
            {
               perror("Server A: send to main server failed ");
               exit(1);
            }
            currentSent = currentSent + numbytes;
            responsechar += currentSent;
         }

         if(!found) {
            printf("Server A has sent \"User %s: Not found\" to Main server\n", userIds);
         }
         else {
            printf("Server A has sent the result(s) to Main Server\n");
         }
      }

      // Clear buf for the next recv
      memset(&buf, 0, sizeof buf);
      recvbytes = 0;  
      
   }

   close(sockfd);

   return 0;
}