#include "client.h"

#define SERVER_PORT 8000

int main(int argc, char* argv[]) {
  int sockfd;
  struct sockaddr_in server_addr;
  char buffer[BUFF_SIZE];
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Socket creation error");
    exit(EXIT_FAILURE);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) <
      0) {
    perror("Connection to the server failed");
    exit(EXIT_FAILURE);
  }

  printf("Connected to the server on port %d\n", SERVER_PORT);

  while (1) {
    printf("Enter message to send to server: ");
    std::string message;
    std::getline(std::cin, message);

    send(sockfd, message.c_str(), message.size(), 0);

    memset(buffer, 0, BUFF_SIZE);
    int bytes_received = recv(sockfd, buffer, BUFF_SIZE, 0);
    if (bytes_received < 0) {
      perror("Error receiving data from server");
      break;
    } else if (bytes_received == 0) {
      printf("Server closed the connection");
      break;
    }

    printf("Server: %s\n", buffer);
  }
  close(sockfd);
  exit(EXIT_SUCCESS);
}