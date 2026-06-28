#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "valhalla.h"
#include "config.h"

#define MAX(a,b) (((a)>(b))?(a):(b))

void valhalla_init(valhalla_t *self)
{
    /* Inicializa os contadores de preces. */
    for (int i = 0; i < NUMBER_OF_GODS; i++)
        self->prayers[i] = 0;

    /* TODO: Adicionar código aqui se necessário! */

    plog("[valhalla] Initialized\n");
}

void valhalla_finalize(valhalla_t *self)
{
    /* TODO: Adicionar código aqui se necessário! */
    
    plog("[valhalla] Finalized\n");
}

void valhalla_pray(valhalla_t *self, god_t god)
{
    /* TODO: Adicionar código se necessário! */

    /* Atualiza o número de preces do deus god. */
    self->prayers[god]++;

    /* Realiza a prece por um tempo determinado (NÃO ALTERE!). */
    msleep(rand() % config.max_pray_time);
}

/* --------------------------------------------------------- */
/* ATENÇÃO: Não será necessário modificar as funções abaixo! */
/* --------------------------------------------------------- */

int valhalla_is_super(god_t god)
{
    return (god == ODIN || god == THOR);
}

int valhalla_is_rival(god_t god1, god_t god2)
{
    if ((god1 < ODIN && god2 < ODIN) && (god1 - god2 == 1 || god1 - god2 == -1)) return 1;
    return 0;
}

god_t valhalla_get_rival(god_t god)
{
    if (god == ODIN || god == THOR)
        return -1;
    
    if (god % 2 == 0)
        return god + 1;
    return god - 1;
}

char * valhalla_get_name(god_t god)
{
    switch (god) {
        case BALDR:
            return "BALDR";
            break;
        case LOKI:
            return "LOKI ";
            break;
        case VALI:
            return "VALI ";
            break;
        case HODER:
            return "HODER";
            break;
        case FRIGG:
            return "FRIGG";
            break;
        case JORD:
            return "JORD ";
            break;
        case ODIN:
            return "ODIN ";
            break;
        case THOR:
            return "THOR ";
            break;
        default:
            return "?";
    }
}

void valhalla_print(valhalla_t *self)
{
    int max = 0, total = 0, total_normal = 0;
    int max_allowed, min_allowed, error;

    printf("Prayers statistics:\n");

    for (int god = 0; god < NUMBER_OF_GODS; god++) {
        int count = self->prayers[god];
        total += count;
        if (!valhalla_is_super(god)) total_normal += count;
        if (count > max) max = count;
    }
    
    for (int god = 0; god < NUMBER_OF_GODS; god++) {
        int count = self->prayers[god];
        error = 0;
        if (!valhalla_is_super(god)) {
            int rival_count = self->prayers[valhalla_get_rival(god)];
            max_allowed = MAX(1, (int) ceil(rival_count * (1.0 + RIVAL_TOLERANCE_RATE)));
            min_allowed = (int) floor(rival_count * (1.0 - RIVAL_TOLERANCE_RATE));
            if (count < min_allowed)
                error = -1;
            else if (count > max_allowed)
                error = 1;
        }
        else 
            if (count > ceil(total_normal * (1.0 + SUPER_GOD_TOLERANCE_RATE)))
                error = 1;

        printf("%s |", valhalla_get_name(god));
        
        for (int i = 0; i < 50 * count/(double)max; i++)
            printf("#");
        
        printf(" %d ", count);

        if (error == 1)
            printf("[TOO MUCH]");
        else if (error == -1)
            printf("[TOO FEW]");
        
        printf("\n");
    }
    printf("Total: %d prayers!\n", total);
    plog(BAR);
    plog("\n");
}
