TARGET = battleships

CC = gcc
CFFLAGS = -Wall -Wextra -Werror

SRC = = main.c

HEADERS = main.HEADERS

OBJ = $(SRC:.c=.o)

$(TARGET): $(OBJ)
   $(CC) $(CFFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c $(HEADERS)
   $(CC) $(CFFLAGS) -c $< -o $@


.PHONY: clean
clean:
   rm -f $(TARGET) $(OBJ)