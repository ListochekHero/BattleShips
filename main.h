#ifndef MAIN_H
#define MAIN_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>

#include "application.h"
#include "config.h"
#include "logger.h"
#include "utility.h"

#define PORT 8080

#endif
