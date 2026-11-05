/*
 * ej04_saludo_servidor.c — Práctica Tema 4, Ejercicio 4
 *
 * Objetivo: implementar la secuencia básica de un servidor TCP:
 * socket(), bind(), listen() y accept().
 *
 * Uso:  ./ej04_saludo_servidor <puerto>
 * Compilación: gcc -Wall -Wextra -o ej04_saludo_servidor ej04_saludo_servidor.c
 *
 * El servidor queda en un bucle infinito atendiendo clientes de a uno:
 * por cada conexión informa la dirección del cliente, le manda un saludo
 * y cierra esa conexión sin terminar (queda a la espera de la siguiente).
 */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_BUF 256

/* Envía todo el búfer, reintentando si send() envía menos de lo pedido. */
static int enviar_todo(int fd, const void *buf, size_t len)
{
    const char *p = (const char *)buf;
    size_t enviados = 0;

    while (enviados < len) {
        ssize_t n = send(fd, p + enviados, len - enviados, 0);
        if (n < 0) {
            if (errno == EINTR)     /* interrumpido por señal: reintentar */
                continue;
            return -1;
        }
        if (n == 0)                 /* sin progreso: evitamos bucle infinito */
            return -1;
        enviados += (size_t)n;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Validación del puerto */
    char *fin = NULL;
    long puerto = strtol(argv[1], &fin, 10);
    if (fin == argv[1] || *fin != '\0' || puerto < 1 || puerto > 65535) {
        fprintf(stderr, "Puerto inválido: \"%s\" (debe ser 1..65535)\n", argv[1]);
        return EXIT_FAILURE;
    }

    /* 1. socket TCP (IPv4) */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    /* SO_REUSEADDR: permite reiniciar el servidor aunque quede algo por
     * cerrar en el puerto (por ejemplo una conexión en TIME_WAIT). */
    int uno = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof uno) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        close(sockfd);
        return EXIT_FAILURE;
    }

    /* 2. bind a la dirección comodín (INADDR_ANY) y al puerto pedido */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);              /* estructura en cero */
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   /* comodín: todas las interfaces */
    addr.sin_port = htons((unsigned short)puerto);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind");
        close(sockfd);
        return EXIT_FAILURE;
    }

    /* 3. listen: cola de conexiones pendientes */
    if (listen(sockfd, 10) < 0) {
        perror("listen");
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("Servidor de saludos escuchando en el puerto %ld (Ctrl+C para salir)\n",
           puerto);
    fflush(stdout);

    /* 4. accept: bloquea hasta que un cliente se conecte */
    for (;;) {
        struct sockaddr_in peer;
        socklen_t len = sizeof peer;
        int connfd = accept(sockfd, (struct sockaddr *)&peer, &len);

        if (connfd < 0) {
            if (errno == EINTR)         /* señal recibida: no es un error */
                continue;
            perror("accept");
            continue;
        }

        /* Dirección IP y puerto efímero del cliente */
        char ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof ip) == NULL)
            strcpy(ip, "?");
        unsigned short pto_cliente = ntohs(peer.sin_port);

        printf("Cliente conectado desde %s:%u\n", ip, pto_cliente);
        fflush(stdout);

        /* 3. Saludo personalizado */
        char msg[MAX_BUF];
        int n = snprintf(msg, sizeof msg, "Hola, te conectaste desde %s:%u\n",
                         ip, pto_cliente);
        if (n < 0 || (size_t)n >= sizeof msg) {
            fprintf(stderr, "Error armando el saludo\n");
        } else if (enviar_todo(connfd, msg, (size_t)n) < 0) {
            perror("send");
        }

        /* 4. Cierre de esta conexión: el servidor sigue a la espera */
        close(connfd);
        printf("Conexión con %s:%u cerrada\n", ip, pto_cliente);
        fflush(stdout);
    }

    /* Nunca se llega aquí (bucle infinito), pero por completitud: */
    close(sockfd);
    return EXIT_SUCCESS;
}
