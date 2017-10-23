/*

 Simple echo UDP server: with a fixed port number

 Usage: ./echo-udp-server

*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

#define PORT 5140        // server port number
#define BUFFER	(1024)   // length of the receiving buffer

int main(int argc, char *argv[])
{
  int fd;                           // an incoming socket descriptor
  struct sockaddr_in server;        // server's address structure
  int n, r;
  char buffer[BUFFER];              // receiving buffer
  struct sockaddr_in client;        // client's address structure
  socklen_t length;

  server.sin_family = AF_INET;                     // set IPv4 addressing
  server.sin_addr.s_addr = htonl(INADDR_ANY);      // the server listens to any interface
  server.sin_port = htons(PORT);                   // the server listens on this port

  printf("opening UDP socket(...)\n");
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) // create the server UDP socket
    err(1, "socket() failed");

  printf("binding to the port %d (%d)\n",PORT,server.sin_port);
  if (bind(fd, (struct sockaddr *)&server, sizeof(server)) == -1) // binding with the port
    err(1, "bind() failed");
  length = sizeof(client);

  while ((n= recvfrom(fd, buffer, BUFFER, 0, (struct sockaddr *)&client, &length)) >= 0)
    {
      buffer[n] = '\0';
      printf("Request received from %s, port %d, data %s\n",inet_ntoa(client.sin_addr),ntohs(client.sin_port), buffer);
      memset(buffer, 0, BUFFER);

    /*  for (r = 0; r < n; r++)
	if (islower(buffer[r]))
	  buffer[r] = toupper(buffer[r]);
	else if (isupper(buffer[r]))
	  buffer[r] = tolower(buffer[r]);*/

      //r = sendto(fd, buffer, n, 0, (struct sockaddr *)&client, length); // send the answer
      //if (r == -1)
	//err(1, "sendto()");
      //else if (r != n)
	//errx(1, "sendto(): Buffer written just partially");
      //else
	//printf("data: \"%s\"", buffer);
    }
  printf("closing socket\n");
  close(fd);

  return 0;
}
