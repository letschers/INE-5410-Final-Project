#ifndef __CHIEFTAIN_H__
#define __CHIEFTAIN_H__

    #include <pthread.h>
    #include "config.h"
    #include "valhalla.h"

    /*============================================================================*
     * DESCRIÇÃO: O chieftain é o chefe da horda de vikings. É através dele que   *
     * os vikings pedem cadeiras e pratos (e os devolvem) e também solicitam      *
     * deuses para realizar as preces.                                            *
    *============================================================================*/

    /**
     * @brief Define os atributos do chieftain.
     */
    typedef struct chieftain
    {
        valhalla_t *valhalla;   /* Referência para valhalla.  */

        /* ----- Monitor da mesa (cadeiras + pratos) ----- */
        pthread_mutex_t table_mutex; /* Protege o estado da mesa.                 */
        pthread_cond_t  table_cond;  /* Acorda vikings que esperam por lugar.     */
        int *chair;        /* -1 = vazia; 0 = guerreiro normal; 1 = berserker.    */
        int *plate;        /* 0 = prato livre; 1 = prato em uso.                  */
        int *chair_plate0; /* 1o prato usado pelo ocupante da cadeira (ou -1).    */
        int *chair_plate1; /* 2o prato usado pelo ocupante da cadeira (ou -1).    */

        /* ----- Barreira do banquete (todos comem antes de rezar) ----- */
        pthread_mutex_t banquet_mutex;   /* Protege finished_eating.              */
        pthread_cond_t  banquet_cond;    /* Acorda quem espera o fim do banquete. */
        unsigned int finished_eating;    /* Qtde. de vikings que já comeram.      */

        /* ----- Atribuição de deuses ----- */
        pthread_mutex_t god_mutex;             /* Protege os contadores abaixo.   */
        unsigned int assigned[NUMBER_OF_GODS]; /* Preces já atribuídas por deus.  */
    } chieftain_t;

    /*============================================================================*
     * Funções utilizadas em arquivos que incluem esse .h                         *
     *============================================================================*/

    /**
     * @brief Inicializa o chieftain (o chefe da horda). Ele controlará
     * uma horda com inicialmente config.horde_size guerreiros e uma mesa com config.table_size
     * cadeiras.
     * 
     * @param self O chieftain.
     * @param valhalla Valhalla.
     */
    extern void chieftain_init(chieftain_t *self, valhalla_t *valhalla);
    
    /**
     * @brief Finaliza o chieftain (o chefe da horda).
     * 
     * @param self O chieftain.
    */
    extern void chieftain_finalize(chieftain_t *self);

    /**
     * @brief Adquire uma cadeira e dois pratos de comida. 
     * 
     * @param self O chieftain.
     * @param berserker Se igual a 1 indica que o viking que chamou este método é um
     * berserker. Por razões de segurança, guerreiros normais e berserkers jamais podem
     * ser atribuídos pelo chieftain a cadeiras adjacentes.
     *
     * @returns Índice da cadeira a ser ocupada pelo guerreiro, como um número no
     * intervalo [0, config.table_size)
     */
    extern int chieftain_acquire_seat_plates(chieftain_t *self, int berserker);

    /**
     * @brief Libera uma cadeira e dois pratos previamente adquiridos via
     * chieftain_acquire_seat_plates().
     *
     * @param self O chieftain.
     * @param pos Índice da cadeira ocupada pelo guerreiro, como um número no
     * intervalo [0, config.table_size)
     */
    extern void chieftain_release_seat_plates(chieftain_t *self, int pos);

    /**
     * @brief Indica ao guerreiro para qual deus viking ele deve fazer suas preces. O deus
     * deve ser escolhido aleatóriamente, mas de forma a respeitar as regras dispostas nas
     * escrituras (ver enunciado).
     *
     * @param self O chieftain.
     * 
     * @returns O Deus atribuído ao guerreiro.
     */
    extern god_t chieftain_get_god(chieftain_t *self);

    /*============================================================================*
    * ATENCÃO: Insira aqui funções que você quiser adicionar a interface para    *
    * serem usadas em arquivos que incluem esse header.                          *
    *============================================================================*/

#endif /*__CHIEFTAIN_H__*/