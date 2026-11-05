/*
 * ej01_sockets.c — Práctica Tema 4, Ejercicio 1
 *
 * Objetivo: ver qué devuelve la función socket() y cómo se detectan e
 * informan sus errores.
 *
 * Uso:  ./ej01_sockets [reutilizar]
 *   - Sin argumentos: crea los tres sockets, los muestra, los cierra y
 *     prueba las combinaciones inválidas.
 *   - Con "reutilizar": además cierra el primer socket y crea uno nuevo
 *     para observar cómo se reutiliza el número de descriptor (pregunta 2).
 *
 * Compilación: gcc -Wall -Wextra -o ej01_sockets ej01_sockets.c
 */
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * Crea un socket y muestra el descriptor obtenido.
 * Se pasa 0 como protocolo: el sistema elige el protocolo por defecto que
 * corresponde al tipo de socket (TCP para SOCK_STREAM, UDP para SOCK_DGRAM).
 * Devuelve el descriptor o -1 si falla (el error ya fue informado).
 */
static int crear_socket(int dominio, int tipo, const char *descripcion)
{
    int fd = socket(dominio, tipo, 0);

    if (fd < 0) {
        /* No abortamos: el enunciado pide informar el error y continuar */
        fprintf(stderr, "ERROR socket(%s): %s\n", descripcion, strerror(errno));
        return -1;
    }
    printf("%-46s -> descriptor %d\n", descripcion, fd);
    return fd;
}

/* Cierra un descriptor informando si la operación tuvo éxito. */
static void cerrar_socket(int fd, const char *descripcion)
{
    if (fd < 0)
        return;
    if (close(fd) < 0)
        fprintf(stderr, "ERROR close(%d %s): %s\n", fd, descripcion, strerror(errno));
    else
        printf("close(%d) %-10s OK\n", fd, descripcion);
}

int main(int argc, char *argv[])
{
    int reutilizar = 0;

    /* Validación de argumentos de línea de comandos */
    if (argc == 2 && strcmp(argv[1], "reutilizar") == 0) {
        reutilizar = 1;
    } else if (argc != 1) {
        fprintf(stderr, "Uso: %s [reutilizar]\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("== Creación de sockets (protocolo 0) ==\n");
    int tcp4 = crear_socket(AF_INET, SOCK_STREAM, "socket(AF_INET, SOCK_STREAM, 0)");
    int udp4 = crear_socket(AF_INET, SOCK_DGRAM, "socket(AF_INET, SOCK_DGRAM, 0)");
    int tcp6 = crear_socket(AF_INET6, SOCK_STREAM, "socket(AF_INET6, SOCK_STREAM, 0)");

    /*
     * Pregunta 2: Linux asigna siempre el descriptor libre más bajo, así que
     * al cerrar el primer socket y crear otro, el nuevo vuelve a ser el 3.
     */
    if (reutilizar) {
        printf("\n== Pregunta 2: cierro el primer socket y creo otro ==\n");
        if (tcp4 >= 0) {
            printf("Cierro el descriptor %d (TCP/IPv4)\n", tcp4);
            close(tcp4);
            tcp4 = -1;
        }
        int nuevo = crear_socket(AF_INET, SOCK_STREAM, "socket(AF_INET, SOCK_STREAM, 0)");
        if (nuevo >= 0) {
            printf("El socket nuevo reutiliza el descriptor liberado (%d)\n", nuevo);
            close(nuevo);
        }
    }

    /* Cierre de los tres sockets */
    printf("\n== Cierre de los tres sockets ==\n");
    cerrar_socket(tcp4, "TCP/IPv4");
    cerrar_socket(udp4, "UDP/IPv4");
    cerrar_socket(tcp6, "TCP/IPv6");

    /*
     * Punto 4: SOCK_STREAM (tipo de socket orientado a conexión) con el
     * protocolo IPPROTO_UDP es una combinación incoherente: el tipo ya
     * determina el protocolo por defecto (TCP), por lo que el sistema la
     * rechaza.
     */
    printf("\n== Punto 4: combinación inválida ==\n");
    errno = 0;
    int mala = socket(AF_INET, SOCK_STREAM, IPPROTO_UDP);
    if (mala < 0) {
        printf("socket(AF_INET, SOCK_STREAM, IPPROTO_UDP) falló: %s (errno=%d)\n",
               strerror(errno), errno);
    } else {
        printf("socket(AF_INET, SOCK_STREAM, IPPROTO_UDP) inesperadamente OK: %d\n", mala);
        close(mala);
    }

    /* Pregunta 4: caso simétrico, tipo de datagrama con protocolo TCP. */
    printf("\n== Pregunta 4: otra combinación inválida ==\n");
    errno = 0;
    int mala2 = socket(AF_INET, SOCK_DGRAM, IPPROTO_TCP);
    if (mala2 < 0) {
        printf("socket(AF_INET, SOCK_DGRAM, IPPROTO_TCP) falló: %s (errno=%d)\n",
               strerror(errno), errno);
    } else {
        printf("socket(AF_INET, SOCK_DGRAM, IPPROTO_TCP) inesperadamente OK: %d\n", mala2);
        close(mala2);
    }

    return EXIT_SUCCESS;
}
