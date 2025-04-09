# Definir el compilador
CC = gcc

# Definir las banderas del compilador
CFLAGS = -Wall -Wextra -I./srv_lib

# Definir los archivos fuente
SRCS = chat_srv.c srv_utils/clients.c

# Definir los archivos objeto
OBJS = $(SRCS:.c=.o)

# Nombre del ejecutable
EXEC = srv

# Regla para compilar el ejecutable
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJS)

# Regla para compilar los archivos .c en archivos .o
%.o: %.c	
	$(CC) $(CFLAGS) -c $< -o $@

# Regla para limpiar los archivos objeto y el ejecutable
clean:
	rm -f $(OBJS) $(EXEC)
