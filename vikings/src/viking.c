#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "viking.h"
#include "valhalla.h"

void viking_init(viking_t *self, chieftain_t *chieftain, valhalla_t *valhalla, int berserker, int type, unsigned int id)
{
    self->chieftain = chieftain;
    self->valhalla = valhalla;
    
    self->berserker = berserker;
    self->type = type;
    self->id = id;

    plog("[viking] Initialized (id=%d, berserker=%d, type=%d)\n", id, berserker, type);
}

void viking_finalize(viking_t *self)
{
    /* TODO: Adicionar código aqui se necessário! */
    plog("[viking] Finalized");
}

/* --------------------------------------------------------- */
/* ATENÇÃO: Não será necessário modificar as funções abaixo! */
/* --------------------------------------------------------- */

void viking_eat(viking_t *self)
{
    /* Pede ao chieftain uma cadeira e dois pratos. */
    int chair = chieftain_acquire_seat_plates(self->chieftain, self->berserker);
    plog("[viking] Viking=%d is now eating (chair=%d)\n", self->id, chair);
    
    /* Se alimenta. */
    msleep(rand() % config.max_eat_time);
    
    /* Informa ao chieftain que terminou de comer, liberando a cadeira e os pratos. */
    chieftain_release_seat_plates(self->chieftain, chair);
    plog("[viking] Viking=%d has finished eating (chair=%d)\n", self->id, chair);
}

void viking_pray(viking_t *self)
{
    /* Pede ao chieftain um deus para rezar. */
    god_t god = chieftain_get_god(self->chieftain);
 
    plog("[viking] Viking=%d is now praying to %s\n", self->id, valhalla_get_name(god));
 
    /* Realiza a prece. */
    valhalla_pray(self->valhalla, god);

    plog("[viking] Viking=%d has finished praying to %s\n", self->id, valhalla_get_name(god));
}

void * viking_run(void *arg)
{
    /* Recupera o argumento de entrada (viking_t). */
    viking_t *viking = (viking_t *) arg;

    plog("[viking] Viking %d is now running!\n", viking->id);
    
    /* Se for NORMAL_VIKING come e depois reza. 
       Caso contrário (LATE_VIKING), somente reza. */
    if (viking->type == NORMAL_VIKING)
        viking_eat(viking);
    viking_pray(viking);

    pthread_exit(NULL);
}
