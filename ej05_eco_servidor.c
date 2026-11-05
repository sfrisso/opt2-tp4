/*
 * ej05_eco_servidor.c — Práctica Tema 4, Ejercicio 5 (servidor)
 *
 * Objetivo: servidor TCP completo que devuelve todo lo que recibe y permite
 * observar el estado TIME_WAIT del cierre de conexión.
 *
 * Uso:  ./ej05_eco_servidor <puerto> [noreuse]
 *   noreuse  -> desactiva la opción SO_REUSEADDR (para el experimento del
 *               enunciado: sin ella, reiniciar el servidor falla mientras
 *               quede una conexión sin cerrar del todo en ese puerto).
 *
 * Compilación: gcc -Wall -Wextra -o ej05_eco_servidor ej05_eco_servidor.c
 */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_BUF 4096

/* Hora actual en formato hh:mm:ss para los mensajes del servidor. */
static void hora(char *buf, size_t n)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, n, "%H:%M:%S", &tm);
}

/*
 * Envía todos los bytes pedidos. send() puede devolver menos de lo pedido
 * (o -1 con EINTR), así que hay que reintentar hasta completar.
 */
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

int main(int argc, char *argv[])
{
    int reuse = 1;

    /* Validación de argumentos */
    if (argc == 3 && strcmp(argv[2], "noreuse") == 0) {
        reuse = 0;
    } else if (argc != 2) {
        fprintf(stderr, "Uso: %s <puerto> [noreuse]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *fin = NULL;
    long puerto = strtol(argv[1], &fin, 10);
    if (fin == argv[1] || *fin != '\0' || puerto < 1 || puerto > 65535) {
        fprintf(stderr, "Puerto inválido: \"%s\" (debe ser 1..65535)\n", argv[1]);
        return EXIT_FAILURE;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (reuse) {
        int uno = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof uno) < 0) {
            perror("setsockopt(SO_REUSEADDR)");
            close(sockfd);
            return EXIT_FAILURE;
        }
        printf("SO_REUSEADDR activado\n");
    } else {
        printf("SO_REUSEADDR desactivado (noreuse)\n");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)puerto);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind");
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (listen(sockfd, 5) < 0) {
        perror("listen");
        close(sockfd);
        return EXIT_FAILURE;
    }

    char h[32];
    hora(h, sizeof h);
    printf("[%s] Servidor eco escuchando en el puerto %ld\n", h, puerto);
    fflush(stdout);

    /* Atiende a los clientes de a uno por vez (aceptar -> eco -> cerrar). */
    for (;;) {
        struct sockaddr_in peer;
        socklen_t len = sizeof peer;
        int connfd = accept(sockfd, (struct sockaddr *)&peer, &len);
        if (connfd < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof ip) == NULL)
            strcpy(ip, "?");
        unsigned short pto = ntohs(peer.sin_port);

        hora(h, sizeof h);
        printf("[%s] >> cliente conectado desde %s:%u\n", h, ip, pto);
        fflush(stdout);

        /* Eco: devuelve todo lo que recibe hasta que el cliente cierra. */
        for (;;) {
            char buf[MAX_BUF];
            ssize_t n = recv(connfd, buf, sizeof buf, 0);

            if (n < 0) {
                if (errno == EINTR)
                    continue;
                perror("recv");
                break;
            }
            if (n == 0) {                       /* el cliente cerró la conexión */
                hora(h, sizeof h);
                printf("[%s] << cliente %s:%u se desconectó (recv devolvió 0)\n",
                       h, ip, pto);
                fflush(stdout);
                break;
            }

            /* Se envían exactamente los n bytes recibidos, aunque send()
             * devuelva menos de lo pedido. */
            if (enviar_todo(connfd, buf, (size_t)n) < 0) {
                perror("send");
                break;
            }
        }

        /* Se cierra la conexión con este cliente y se vuelve a aceptar. */
        close(connfd);
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
