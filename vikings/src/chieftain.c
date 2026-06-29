#include "chieftain.h"

#include <pthread.h>
#include <stdlib.h>

#include "config.h"
#include "valhalla.h"

void chieftain_init(chieftain_t* self, valhalla_t* valhalla) {
    int n = config.table_size;

    self->valhalla = valhalla;

    /* ----- Monitor da mesa ----- */
    pthread_mutex_init(&self->table_mutex, NULL);
    pthread_cond_init(&self->table_cond, NULL);

    self->chair = (int*)malloc(sizeof(int) * n);
    self->plate = (int*)malloc(sizeof(int) * n);
    self->chair_plate0 = (int*)malloc(sizeof(int) * n);
    self->chair_plate1 = (int*)malloc(sizeof(int) * n);

    for (int i = 0; i < n; i++) {
        self->chair[i] = -1;        /* cadeira vazia          */
        self->plate[i] = 0;         /* prato livre            */
        self->chair_plate0[i] = -1; /* nenhum prato associado */
        self->chair_plate1[i] = -1;
    }

    /* ----- Barreira do banquete ----- */
    pthread_mutex_init(&self->banquet_mutex, NULL);
    pthread_cond_init(&self->banquet_cond, NULL);
    self->finished_eating = 0;

    /* ----- Atribuição de deuses ----- */
    pthread_mutex_init(&self->god_mutex, NULL);
    for (int g = 0; g < NUMBER_OF_GODS; g++) self->assigned[g] = 0;

    plog("[chieftain] Initialized\n");
}

/**
 * @brief Dentro de um par de rivais, retorna qual deus deve receber a próxima
 * prece para manter o par balanceado (|diferença| <= 1). Sempre incrementa o
 * menor dos dois (empate -> o próprio g). Manter a diferença em no máximo 1
 * garante que o par jamais ultrapasse a tolerância de 5% verificada em
 * valhalla_print().
 *
 * @param self O chieftain.
 * @param g Um deus rival (BALDR..JORD).
 *
 * @returns O deus do par que deve ser incrementado.
 */
static god_t chieftain_balance_pair(chieftain_t* self, god_t g) {
    god_t rival = valhalla_get_rival(g);
    return (self->assigned[g] <= self->assigned[rival]) ? g : rival;
}

int chieftain_acquire_seat_plates(chieftain_t* self, int berserker) {
    int n = config.table_size;
    int chosen = -1;

    pthread_mutex_lock(&self->table_mutex);

    /* Procura atomicamente uma cadeira livre + dois pratos. Como a aquisição é
       "tudo ou nada" (o viking nunca segura recursos parciais enquanto espera),
       não há hold-and-wait e, portanto, não há deadlock. */
    while (chosen == -1) {
        for (int i = 0; i < n; i++) {
            /* Cadeira precisa estar livre. */
            if (self->chair[i] != -1) continue;

            /* Regra do berserker: adjacência é LINEAR (o vão entre a cadeira 0
               e a cadeira n-1 é berserker-safe). Um vizinho ocupado por um tipo
               diferente do nosso impede o uso da cadeira. */
            if (i > 0 && self->chair[i - 1] != -1 &&
                self->chair[i - 1] != berserker)
                continue;

            if (i < n - 1 && self->chair[i + 1] != -1 &&
                self->chair[i + 1] != berserker)
                continue;

            /* Pratos alcançáveis: o da própria cadeira, o da esquerda e o da
               direita. O alcance é CIRCULAR (pode atravessar o vão). */
            int ps[3] = {i, (i - 1 + n) % n, (i + 1) % n};
            int cand[3];
            int nc = 0;
            for (int k = 0; k < 3; k++) {
                int dup = 0;
                for (int j = 0; j < nc; j++)
                    if (cand[j] == ps[k]) {
                        dup = 1;
                        break;
                    }
                if (!dup) cand[nc++] = ps[k];
            }

            /* Precisa de dois pratos livres dentre os alcançáveis. */
            int p0 = -1, p1 = -1;
            for (int k = 0; k < nc; k++) {
                if (!self->plate[cand[k]]) {
                    if (p0 < 0)
                        p0 = cand[k];
                    else {
                        p1 = cand[k];
                        break;
                    }
                }
            }

            if (p0 >= 0 && p1 >= 0) {
                /* Senta e reserva os dois pratos. */
                self->chair[i] = berserker;
                self->plate[p0] = 1;
                self->plate[p1] = 1;
                self->chair_plate0[i] = p0;
                self->chair_plate1[i] = p1;
                chosen = i;
                break;
            }
        }

        /* Nenhuma combinação possível agora: espera alguém liberar recursos. */
        if (chosen == -1)
            pthread_cond_wait(&self->table_cond, &self->table_mutex);
    }

    pthread_mutex_unlock(&self->table_mutex);

    /* Debug: registra a cadeira e os dois pratos efetivamente escolhidos. Em
       modo release (NDEBUG) o plog não gera código algum. A leitura aqui é
       segura: a cadeira só será modificada por este viking ao liberar o lugar.
     */
    plog("[chieftain] grant seat=%d plates=%d,%d berserker=%d\n", chosen,
         self->chair_plate0[chosen], self->chair_plate1[chosen], berserker);

    return chosen;
}

void chieftain_release_seat_plates(chieftain_t* self, int pos) {
    /* Libera a cadeira e os dois pratos, acordando quem espera por lugar. */
    pthread_mutex_lock(&self->table_mutex);
    self->plate[self->chair_plate0[pos]] = 0;
    self->plate[self->chair_plate1[pos]] = 0;
    self->chair_plate0[pos] = -1;
    self->chair_plate1[pos] = -1;
    self->chair[pos] = -1;
    pthread_cond_broadcast(&self->table_cond);
    pthread_mutex_unlock(&self->table_mutex);

    /* Este viking (necessariamente NORMAL, pois apenas eles comem) acabou de
       comer. Contabiliza na barreira do banquete e, se foi o último, libera
       todos que aguardam para começar as preces. */
    pthread_mutex_lock(&self->banquet_mutex);
    self->finished_eating++;
    if (self->finished_eating >= config.horde_size)
        pthread_cond_broadcast(&self->banquet_cond);
    pthread_mutex_unlock(&self->banquet_mutex);
}

god_t chieftain_get_god(chieftain_t* self) {
    god_t chosen;

    /* É falta de educação começar a rezar antes de o banquete terminar: espera
       que todos os config.horde_size participantes do banquete tenham comido.
     */
    pthread_mutex_lock(&self->banquet_mutex);
    while (self->finished_eating < config.horde_size)
        pthread_cond_wait(&self->banquet_cond, &self->banquet_mutex);
    pthread_mutex_unlock(&self->banquet_mutex);

    /* Escolhe um deus aleatoriamente, mas mantendo as escrituras satisfeitas. O
       chieftain mantém seus próprios contadores (assigned[]) pois valhalla só é
       incrementado depois, ao fim da prece. */
    pthread_mutex_lock(&self->god_mutex);

    int r = rand() % NUMBER_OF_GODS;

    if (r == ODIN || r == THOR) {
        /* Super deus: pode receber no máximo 10% além da soma dos 6 deuses
           normais. ceil(1.1 * total_normal) calculado em inteiros como
           (11*total + 9)/10 (igual ao ceil exato e <= ao ceil em ponto
           flutuante usado em valhalla_print, logo sempre seguro). */
        unsigned int total_normal = 0;
        for (int g = BALDR; g <= JORD; g++) total_normal += self->assigned[g];

        unsigned int max_super = (11u * total_normal + 9u) / 10u;

        if (self->assigned[r] + 1 <= max_super)
            chosen = (god_t)r;
        else
            /* Sem orçamento para o super deus: equilibra um par de rivais. */
            chosen = chieftain_balance_pair(self, (god_t)(rand() % 6));
    } else {
        /* Deus normal: incrementa o menor do seu par para manter |dif| <= 1. */
        chosen = chieftain_balance_pair(self, (god_t)r);
    }

    self->assigned[chosen]++;

    pthread_mutex_unlock(&self->god_mutex);

    return chosen;
}

void chieftain_finalize(chieftain_t* self) {
    free(self->chair);
    free(self->plate);
    free(self->chair_plate0);
    free(self->chair_plate1);

    pthread_mutex_destroy(&self->table_mutex);
    pthread_cond_destroy(&self->table_cond);
    pthread_mutex_destroy(&self->banquet_mutex);
    pthread_cond_destroy(&self->banquet_cond);
    pthread_mutex_destroy(&self->god_mutex);

    plog("[chieftain] Finalized\n");
}
