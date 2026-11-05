/*
 * ej02_endianness.c — Práctica Tema 4, Ejercicio 2
 *
 * Objetivo: observar en memoria la diferencia entre el orden de bytes del
 * host y el de la red, y el efecto de las funciones de conversión.
 *
 * Uso:  ./ej02_endianness
 * Compilación: gcc -Wall -Wextra -o ej02_endianness ej02_endianness.c
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Recorre la variable byte a byte con un puntero a unsigned char (como
 * sugiere el enunciado) y los imprime en hexadecimal.
 */
static void imprimir_bytes(const char *etiqueta, const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;

    printf("%-44s", etiqueta);
    for (size_t i = 0; i < n; i++)
        printf("%s%02X", i == 0 ? "" : " ", b[i]);
    printf("\n");
}

int main(void)
{
    /* --- 1. ¿La máquina es little-endian o big-endian? --- */
    uint16_t muestra = 0x0102;
    const unsigned char *p = (const unsigned char *)&muestra;

    printf("== 1. Orden de bytes del host ==\n");
    imprimir_bytes("uint16_t con valor 0x0102 en memoria:", &muestra, sizeof muestra);

    if (p[0] == 0x02)
        printf("-> LITTLE-ENDIAN: el byte menos significativo (0x02) está primero.\n");
    else
        printf("-> BIG-ENDIAN: el byte más significativo (0x01) está primero.\n");

    /* --- 2. 8080 antes y después de htons() --- */
    printf("\n== 2. Puerto 8080 (0x1F90) ==\n");
    uint16_t antes = 8080;
    uint16_t despues = htons(8080);
    imprimir_bytes("8080 en el host (uint16_t):", &antes, sizeof antes);
    imprimir_bytes("htons(8080) en memoria:", &despues, sizeof despues);
    printf("(en hexadecimal: 1F 90 => 90 1F)\n");

    /* --- 3. 0xC0A8010A antes y después de htonl() --- */
    printf("\n== 3. Dirección 0xC0A8010A (192.168.1.10) ==\n");
    uint32_t dir_host = 0xC0A8010A;
    uint32_t dir_red = htonl(0xC0A8010A);
    imprimir_bytes("0xC0A8010A en el host (uint32_t):", &dir_host, sizeof dir_host);
    imprimir_bytes("htonl(0xC0A8010A) en memoria:", &dir_red, sizeof dir_red);
    printf("(0A 01 A8 C0 => C0 A8 01 0A)\n");

    /* --- 4. sin_port sin y con la conversión de orden de bytes --- */
    printf("\n== 4. campo sin_port de struct sockaddr_in ==\n");
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);      /* estructura inicializada en cero */

    sa.sin_port = 8080;             /* asignación INCORRECTA: sin htons() */
    printf("sin_port = 8080 (sin htons)  -> ntohs(sin_port) = %u\n",
           (unsigned)ntohs(sa.sin_port));

    sa.sin_port = htons(8080);      /* asignación correcta */
    printf("sin_port = htons(8080)       -> ntohs(sin_port) = %u\n",
           (unsigned)ntohs(sa.sin_port));

    return 0;
}
