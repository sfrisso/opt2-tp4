/*
 * ej03_resolver.c — Práctica Tema 4, Ejercicio 3
 *
 * Objetivo: usar getaddrinfo() y getnameinfo() para traducir nombres de
 * host y servicios a direcciones y viceversa.
 *
 * Uso:  ./ej03_resolver <host> <servicio>
 * Compilación: gcc -Wall -Wextra -o ej03_resolver ej03_resolver.c
 */
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <host> <servicio>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *host = argv[1];
    const char *servicio = argv[2];

    /* AF_UNSPEC: acepta IPv4 e IPv6; SOCK_STREAM: sockets TCP (http, ssh...) */
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);     /* estructura inicializada en cero */
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, servicio, &hints, &res);
    if (rc != 0) {
        /* getaddrinfo() no usa errno: el error viene en el valor devuelto */
        fprintf(stderr, "getaddrinfo(%s, %s): %s\n", host, servicio, gai_strerror(rc));
        return EXIT_FAILURE;
    }

    printf("Resolviendo host=\"%s\" servicio=\"%s\"\n\n", host, servicio);

    int num = 0;
    for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
        char ip[NI_MAXHOST];
        char puerto[NI_MAXSERV];
        char nombre[NI_MAXHOST];
        const char *familia;

        /* Familia y dirección en formato numérico */
        if (rp->ai_family == AF_INET)
            familia = "IPv4";
        else if (rp->ai_family == AF_INET6)
            familia = "IPv6";
        else
            familia = "otra";

        rc = getnameinfo(rp->ai_addr, rp->ai_addrlen,
                         ip, sizeof ip, puerto, sizeof puerto,
                         NI_NUMERICHOST | NI_NUMERICSERV);
        if (rc != 0) {
            printf("  [%d] getnameinfo (numérico): %s\n", num, gai_strerror(rc));
            num++;
            continue;
        }

        printf("  [%d] %-4s %-39s puerto %s\n", num, familia, ip, puerto);

        /* Nombre inverso (reverse DNS). Con NI_NAMEREQD no se acepta la
         * dirección numérica como "nombre": si no existe, lo informamos. */
        rc = getnameinfo(rp->ai_addr, rp->ai_addrlen,
                         nombre, sizeof nombre, NULL, 0, NI_NAMEREQD);
        if (rc == 0)
            printf("       nombre inverso: %s\n", nombre);
        else
            printf("       nombre inverso: no existe (%s)\n", gai_strerror(rc));

        num++;
    }

    if (num == 0)
        printf("  (sin direcciones)\n");

    freeaddrinfo(res);      /* se libera la lista devuelta por getaddrinfo() */
    return EXIT_SUCCESS;
}
