#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "horde.h"
#include "viking.h"

void horde_init(horde_t *self, chieftain_t *chieftain, valhalla_t *valhalla)
{
    /* O número de vikings é no máximo 2x o tamanho da horda para acomodar
    vikings normais + atrasados. */
    self->vikings = (viking_t *) malloc(sizeof(viking_t) * config.horde_size * 2);

    self->normal_vikings = 0;
    self->late_vikings = 0;
    self->berserkers = 0;

    self->chieftain = chieftain;
    self->valhalla = valhalla;

    plog("[horde] Initialized\n");
    
    /* TODO: Adicionar código aqui se necessário! */
}

void horde_spawn_viking(horde_t *self, int berserker, int type)
{
    /* Calcula o id do viking. */
    int viking_id = self->normal_vikings + self->late_vikings;

    /* Inicializa o viking. */    
    viking_init(&self->vikings[viking_id], self->chieftain, self->valhalla, berserker, type, viking_id);

    /* Cria a thread viking. */
    pthread_create(&self->vikings[viking_id].thread, NULL, viking_run, (void *) &self->vikings[viking_id]);
    
    /* Atualiza o número de vikings e berserkers. */
    (type == NORMAL_VIKING) ? self->normal_vikings++ : self->late_vikings++;
    if (berserker)
        self->berserkers++;    
    
    plog("[horde] Viking %d created (berserker=%d, type=%d)!\n", viking_id, berserker, type);
}

void horde_join(horde_t *self)
{
    /* TODO: Adicionar código aqui se necessário! */

    for (int i = 0; i < self->normal_vikings + self->late_vikings; i++)
        pthread_join(self->vikings[i].thread, NULL);
}

void horde_finalize(horde_t *self)
{   
    /* TODO: Adicionar código aqui se necessário! */

    free(self->vikings);
    plog("[horde] Finalized\n");
}

/* --------------------------------------------------------- */
/* ATENÇÃO: Não será necessário modificar as funções abaixo! */
/* --------------------------------------------------------- */

void horde_print(horde_t *self)
{
    plog("\n");
    plog(BAR);
    printf("Horde statistics:\n  Normal vikings: %d (%d berserkers)\n  Late vikings: %d\n\n", self->normal_vikings, self->berserkers, self->late_vikings);
}