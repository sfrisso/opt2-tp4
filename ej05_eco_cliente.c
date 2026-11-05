/*
 * ej05_eco_cliente.c — Práctica Tema 4, Ejercicio 5 (cliente)
 *
 * Objetivo: cliente TCP que resuelve el destino con getaddrinfo(),
 * probando TODAS las direcciones de la lista hasta que una conexión tenga
 * éxito, y permite hablar por líneas con el servidor eco.
 *
 * Uso:  ./ej05_eco_cliente <host> <servicio>
 * Termina con fin de entrada (Ctrl+D).
 *
 * Compilación: gcc -Wall -Wextra -o ej05_eco_cliente ej05_eco_cliente.c
 */
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_LIN 4096

/* Envía todos los bytes (send() puede enviar menos de lo pedido). */
static int enviar_todo(int fd, const void *buf, size_t len)
{
    const char *p = (const char *)buf;
    size_t enviados = 0;

    while (enviados < len) {
        ssize_t n = send(fd, p + enviados, len - enviados, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;
        enviados += (size_t)n;
    }
    return 0;
}

/* Recibe exactamente 'len' bytes (o hasta que el servidor cierre). */
static ssize_t recibir_todo(int fd, char *buf, size_t len)
{
    size_t recibidos = 0;

    while (recibidos < len) {
        ssize_t n = recv(fd, buf + recibidos, len - recibidos, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)                 /* el servidor cerró la conexión */
            break;
        recibidos += (size_t)n;
    }
    return (ssize_t)recibidos;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <host> <servicio>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;        /* IPv4 o IPv6 */
    hints.ai_socktype = SOCK_STREAM;    /* TCP */

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(argv[1], argv[2], &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo(%s, %s): %s\n", argv[1], argv[2], gai_strerror(rc));
        return EXIT_FAILURE;
    }

    /* Se prueba cada dirección de la lista hasta que alguna conecte. */
    int sockfd = -1;
    int intento = 0;
    for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
        char ip[NI_MAXHOST], serv[NI_MAXSERV];

        intento++;
        if (getnameinfo(rp->ai_addr, rp->ai_addrlen, ip, sizeof ip,
                        serv, sizeof serv, NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
            strcpy(ip, "?");
            strcpy(serv, "?");
        }

        printf("Intento %d: %s:%s ... ", intento, ip, serv);
        fflush(stdout);

        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) {
            printf("socket() falló: %s\n", strerror(errno));
            continue;
        }
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            printf("conectado\n");
            break;
        }
        printf("conexión rechazada: %s\n", strerror(errno));
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);

    if (sockfd < 0) {
        fprintf(stderr, "No se pudo conectar con %s:%s\n", argv[1], argv[2]);
        return EXIT_FAILURE;
    }
    printf("Conectado. Escriba líneas; Ctrl+D para salir.\n\n");

    char linea[MAX_LIN];
    while (fgets(linea, sizeof linea, stdin) != NULL) {
        size_t len = strlen(linea);

        if (enviar_todo(sockfd, linea, len) < 0) {
            perror("send");
            break;
        }

        /* El servidor eco devuelve exactamente lo que enviamos. */
        char eco[MAX_LIN];
        ssize_t n = recibir_todo(sockfd, eco, len);
        if (n < 0) {
            perror("recv");
            break;
        }
        if (n == 0) {
            printf("(el servidor cerró la conexión)\n");
            break;
        }
        printf("eco: %.*s", (int)n, eco);
        fflush(stdout);
    }

    printf("\nCerrando conexión.\n");
    close(sockfd);
    return EXIT_SUCCESS;
}
