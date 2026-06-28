#ifndef __VIKING_H__
#define __VIKING_H__

    #include "config.h"
    #include "valhalla.h"
    #include "chieftain.h"

    /*============================================================================*
     * DESCRIÇÃO: Representa um viking.                                           *
     *============================================================================*/

    /**
     * @brief Define os atributos do viking.
     */
    typedef struct viking
    {
        int berserker;  /* É um berserker? 1 para sim, 0 para não. */
        int type;       /* NORMAL_VIKING ou LATE_VIKING?           */
        int id;         /* Identificador único do viking.          */ 

        chieftain_t *chieftain; /* Referência para chieftain. */
        valhalla_t *valhalla;   /* Referência para valhalla.  */

        pthread_t thread; /* A thread viking. */
        
        /* TODO: Adicione aqui os atributos que achar necessários para implementar o
        comportamento do viking. Esses atributos deverão ser usados pelas funções
        do viking. */
    } viking_t;

    /*============================================================================*
     * Funções utilizadas em arquivos que incluem esse .h                         *
     *============================================================================*/

    /**
     * @brief Inicializa o viking.
     * 
     * @param viking O viking.
     * @param chieftain O chieftain.
     * @param valhalla Valhalla.
     * @param berserker Se igual a 0 indica que é um viking normal, caso contrário,
     * indica que o viking é um berserker.
     * @param type Se NORMAL_VIKING participa do banquete e das preces. Se LATE_VIKING,
     * participa somente das preces.
     * @param id Identificador único do viking.
     */
    extern void viking_init(viking_t *viking, chieftain_t *chieftain, valhalla_t *valhalla, int berserker, int type, unsigned int id);
    
    /**
     * @brief Finaliza o viking.
     * 
     * @param self O viking.
     */
    extern void viking_finalize(viking_t *self);

    /**
     * @brief Come.
     * 
     * @param self O viking.
     */
    extern void viking_eat(viking_t *self);

    /**
     * @brief Realiza uma prece.
     * 
     * @param self O viking.
     */
    extern void viking_pray(viking_t *self);

    /**
     * @brief Função a ser executada pela thread viking. Implementa o comportamento
     * de um viking.
     *
     * @param arg O argumento de entrada da thread, que deverá ser um viking_t.
     */
    extern void * viking_run(void *arg);

    /*============================================================================*
     * ATENCÃO: Insira aqui funções que você quiser adicionar a interface para    *
     * serem usadas em arquivos que incluem esse header.                          *
     *============================================================================*/

#endif /*__VIKING_H__*/