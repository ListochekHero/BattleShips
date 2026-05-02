// IWYU pragma: no_include <vector>
#include "client.h"

#include "application/client.h"
#include "net/socket_routine.h"
#include "protocol/message_defs.h"
#include "protocol/message_types.h"
#include "utility/logger.h"

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define SERVER_PORT 8000

auto main(int /*unused*/, char* argv[]) -> int {
  char* program_name = strrchr(argv[0], '/');
  program_name++;
  if (auto result = bsm::Logger::instance().init(program_name); !result) {
    std::cout << "Unable to init Logger";
    return EXIT_FAILURE;
  }
  bsm::Client client{};
  client.v_init(bsm::end_point_e::TO_SERVER, 0);
  client.run();
  exit(EXIT_SUCCESS);

  int sockfd;
  struct sockaddr_in server_addr;
  char buffer[BUFF_SIZE];

  bsm::LOG("Creating socket");
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Socket creation error");
    exit(EXIT_FAILURE);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  bsm::LOG("Connecting socket");

  if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) <
      0) {
    perror("Connection to the server failed");
    exit(EXIT_FAILURE);
  }
  bsm::SocketHandler socket_server{sockfd};
  printf("Connected to the server on port %d\n", SERVER_PORT);

  while (1) {
    printf("Enter message to send to server: ");
    std::string message;
    std::getline(std::cin, message);

    socket_server.send_message({{message}});

    // send(sockfd, message.c_str(), message.size(), 0);

    memset(buffer, 0, BUFF_SIZE);

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout,
               sizeof(timeout));
    while (true) {
      auto result = socket_server.receive_message();
      if (result) {
        printf("Server: %s\n", (*result).payload.c_str());
      }

      // int bytes_received = recv(sockfd, buffer, BUFF_SIZE, 0);
      if ((*result).status == bsm::message_status_e::WOULDBLOCK) {
        perror("Error receiving data from server");
        break;
      } else if ((*result).status == bsm::message_status_e::DISCONNECTED) {
        printf("Server closed the connection");
        break;
      }

      printf("Server: %s\n", buffer);
      memset(buffer, 0, BUFF_SIZE);
    }
  }
  close(sockfd);
  exit(EXIT_SUCCESS);
}
