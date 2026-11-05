/*
 * ej06_udp_cliente.c — Práctica Tema 4, Ejercicio 6 (cliente UDP)
 *
 * Envía tres datagramas de tamaños distintos y comprueba que cada respuesta
 * tenga exactamente el mismo tamaño. Cada respuesta espera como máximo 2
 * segundos (SO_RCVTIMEO) y se reintenta hasta 3 veces antes de abandonar.
 *
 * Uso:  ./ej06_udp_cliente <host> <puerto> [conectado]
 *   sin "conectado" -> sendto()/recvfrom() (cada llamada lleva la dirección)
 *   con "conectado" -> connect() + send()/recv()
 *
 * Compilación: gcc -Wall -Wextra -o ej06_udp_cliente ej06_udp_cliente.c
 */
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_DATAGRAMA 1024
#define MAX_INTENTOS  3
#define TIMEOUT_S     2      /* segundos de espera por cada respuesta */

int main(int argc, char *argv[])
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Uso: %s <host> <puerto> [conectado]\n", argv[0]);
        return EXIT_FAILURE;
    }
    int modo_conectado = (argc == 4 && strcmp(argv[3], "conectado") == 0);

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(argv[1], argv[2], &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo(%s, %s): %s\n", argv[1], argv[2], gai_strerror(rc));
        return EXIT_FAILURE;
    }

    /* Se usa la primera dirección de la lista (el servidor es dual). */
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return EXIT_FAILURE;
    }

    /* Timeout de recepción: 2 segundos, como pide el enunciado. */
    struct timeval tv = { .tv_sec = TIMEOUT_S, .tv_usec = 0 };
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) < 0) {
        perror("setsockopt(SO_RCVTIMEO)");
        close(sockfd);
        freeaddrinfo(res);
        return EXIT_FAILURE;
    }

    char destino[NI_MAXHOST], puerto_dst[NI_MAXSERV];
    getnameinfo(res->ai_addr, res->ai_addrlen, destino, sizeof destino,
                puerto_dst, sizeof puerto_dst, NI_NUMERICHOST | NI_NUMERICSERV);

    if (modo_conectado) {
        /* connect() en UDP NO envía ningún paquete: sólo fija el destino
         * por defecto del socket y filtra los paquetes entrantes. */
        if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
            perror("connect");
            close(sockfd);
            freeaddrinfo(res);
            return EXIT_FAILURE;
        }
        printf("Modo CONECTADO (connect + send/recv) con %s:%s\n", destino, puerto_dst);
    } else {
        printf("Modo NO conectado (sendto/recvfrom) con %s:%s\n", destino, puerto_dst);
    }

    /* Tres datagramas de tamaños distintos. */
    const size_t tamanios[3] = { 1, 18, 1000 };
    unsigned char enviado[MAX_DATAGRAMA];
    unsigned char recibido[MAX_DATAGRAMA];

    for (int m = 0; m < 3; m++) {
        size_t len = tamanios[m];
        memset(enviado, 'A' + m, len);

        int respondio = 0;
        for (int intento = 1; intento <= MAX_INTENTOS && !respondio; intento++) {
            printf("  mensaje %d (%zu bytes) intento %d/%d: ",
                   m + 1, len, intento, MAX_INTENTOS);
            fflush(stdout);

            ssize_t enviado_n;
            if (modo_conectado)
                enviado_n = send(sockfd, enviado, len, 0);
            else
                enviado_n = sendto(sockfd, enviado, len, 0,
                                   res->ai_addr, res->ai_addrlen);

            if (enviado_n < 0) {
                printf("send falló: %s\n", strerror(errno));
                continue;
            }

            ssize_t n;
            if (modo_conectado)
                n = recv(sockfd, recibido, sizeof recibido, 0);
            else
                n = recvfrom(sockfd, recibido, sizeof recibido, 0, NULL, NULL);

            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    printf("sin respuesta (timeout de %d s): %s\n",
                           TIMEOUT_S, strerror(errno));
                else
                    printf("recv falló: %s\n", strerror(errno));
                continue;
            }

            if ((size_t)n != len) {
                printf("respuesta de %zd bytes (se esperaban %zu): ERROR\n", n, len);
                continue;
            }

            printf("respuesta OK, %zd bytes\n", n);
            respondio = 1;
        }

        if (!respondio)
            printf("  -> mensaje %d abandonado tras %d intentos\n",
                   m + 1, MAX_INTENTOS);
    }

    close(sockfd);
    freeaddrinfo(res);
    return EXIT_SUCCESS;
}
