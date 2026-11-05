/*
 * ej06_udp_servidor.c — Práctica Tema 4, Ejercicio 6 (servidor UDP)
 *
 * Recibe datagramas con recvfrom(), informa la dirección de origen y el
 * tamaño de cada uno, y lo devuelve tal cual al remitente con sendto().
 *
 * Uso:  ./ej06_udp_servidor <puerto>
 * Compilación: gcc -Wall -Wextra -o ej06_udp_servidor ej06_udp_servidor.c
 *
 * Se usa getaddrinfo() con AF_UNSPEC + AI_PASSIVE para poder asociar tanto
 * IPv4 como IPv6. Si la primera dirección es IPv6 se desactiva
 * IPV6_V6ONLY para que el socket acepte también datagramas IPv4
 * (direcciones IPv4 mapeadas), de modo que sirva a clientes de las dos
 * familias con un solo socket.
 */
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_DATAGRAMA 2048

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *fin = NULL;
    long puerto = strtol(argv[1], &fin, 10);
    if (fin == argv[1] || *fin != '\0' || puerto < 1 || puerto > 65535) {
        fprintf(stderr, "Puerto inválido: \"%s\" (debe ser 1..65535)\n", argv[1]);
        return EXIT_FAILURE;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;        /* dirección comodín */

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(NULL, argv[1], &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return EXIT_FAILURE;
    }

    /* Se asocia la primera dirección que se pueda usar. */
    int sockfd = -1;
    for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) {
            perror("socket");
            continue;
        }
        if (rp->ai_family == AF_INET6) {
            /* Socket dual: acepta también IPv4 (IPv4-mapped) */
            int cero = 0;
            if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &cero, sizeof cero) < 0)
                perror("setsockopt(IPV6_V6ONLY)");
        }
        if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        perror("bind");
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);

    if (sockfd < 0) {
        fprintf(stderr, "No se pudo asociar el puerto %ld\n", puerto);
        return EXIT_FAILURE;
    }

    printf("Servidor UDP escuchando en el puerto %ld (Ctrl+C para salir)\n", puerto);
    fflush(stdout);

    for (;;) {
        struct sockaddr_storage origen;
        socklen_t len = sizeof origen;
        char buf[MAX_DATAGRAMA];

        ssize_t n = recvfrom(sockfd, buf, sizeof buf, 0,
                             (struct sockaddr *)&origen, &len);
        if (n < 0) {
            perror("recvfrom");
            continue;
        }

        /* Dirección de origen del datagrama */
        char ip[NI_MAXHOST], serv[NI_MAXSERV];
        if (getnameinfo((struct sockaddr *)&origen, len, ip, sizeof ip,
                        serv, sizeof serv, NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
            strcpy(ip, "?");
            strcpy(serv, "?");
        }

        printf("datagrama de %s:%s -> %zd bytes\n", ip, serv, n);
        fflush(stdout);

        if (sendto(sockfd, buf, (size_t)n, 0,
                   (struct sockaddr *)&origen, len) < 0) {
            perror("sendto");
        }
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
