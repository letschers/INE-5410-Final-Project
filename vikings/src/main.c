#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "config.h"
#include "chieftain.h"
#include "valhalla.h"
#include "horde.h"

config_t parse (int argc, char **argv)
{
    int c;
    config_t config = { 20, 10, 1000, 1000 };

    while ((c = getopt(argc, argv, "v:c:p:e:h")) != -1) {
        switch (c) {
            case 'v':
                config.horde_size = atoi(optarg);
                break;
            case 'c':
                config.table_size = atoi(optarg);
                break;
            case 'p':
                config.max_pray_time = atoi(optarg);
                break;
            case 'e':
                config.max_eat_time = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [OPTIONS]\n\nOptions:\n", argv[0]);
                printf("  -v  Number of vikings (default 20).\n");
                printf("  -c  Number of chairs (default 10).\n");
                printf("  -e  Maximum eat time (default 1000 ms).\n");
                printf("  -p  Maximum pray time (default 1000 ms).\n");
                printf("  -h  Prints this help.\n");
            default:
                exit(1);
        }
    }

    return config;
}

/* --------------------------------------------------------- *
 *                                                           *
 *     ATENÇÃO: A FUNÇÃO MAIN NÃO PODERÁ SER ALTERADA!       *
 *                                                           *
 * --------------------------------------------------------- */

int main (int argc, char **argv)
{
    chieftain_t chieftain;
    valhalla_t valhalla;
    horde_t horde;

    /* Faz o parsing dos parâmetros de entrada. */
    config = parse(argc, argv);
    
    /* Imprime as informações da execução. */
    printf(BAR);
    printf("Number of vikings       : %u\n", config.horde_size);
    printf("Number of chairs        : %u\n", config.table_size);
    printf("Maximum eat time        : %u ms\n", config.max_eat_time);
    printf("Maximum pray time       : %u ms\n", config.max_pray_time);
    printf("Rival tolerance rate    : %f\n", RIVAL_TOLERANCE_RATE);
    printf("Super god tolerance rate: %f\n", SUPER_GOD_TOLERANCE_RATE);
    printf(BAR);
    printf("\n");

    /* Inicializa o gerador de números aleatórios. */
    srand(time(NULL));
    
    /* Inicializa valhalla. */
    valhalla_init(&valhalla);

    /* Inicializa o chieftain. */    
    chieftain_init(&chieftain, &valhalla);

    /* Inicializa a hora de vikings. */
    horde_init(&horde, &chieftain, &valhalla);
    
    /* Cria os vikings normais (ou seja, os que participam do banquete e das preces). */
    for (int i = 0; i < config.horde_size; i++)
        horde_spawn_viking(&horde, (rand() % 10) < 5, NORMAL_VIKING);

    /* Aguarda um tempo antes de criar os vikings que chegam atrasados.
       Estes vikings participam somente das preces. */
    msleep(WAIT_LATE_VIKING);

    /* Define a quantidade de vikings que chegarão atrasados. */
    unsigned int late_vikings = rand() % config.horde_size;

    /* Cria os vikings atrasados. */
    for (int i = 0; i < late_vikings; i++)
        horde_spawn_viking(&horde, 0, LATE_VIKING);

    /* Aguarda que todos os vikings tenham terminado com sucesso. */
    horde_join(&horde);

    /* Imprime as estatísticas da horda e das preces. */
    horde_print(&horde);
    valhalla_print(&valhalla);

    /* Finaliza tudo. */
    chieftain_finalize(&chieftain);
    valhalla_finalize(&valhalla);
    horde_finalize(&horde);

    return 0;
}