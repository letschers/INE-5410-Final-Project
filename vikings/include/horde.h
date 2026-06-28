#ifndef __HORDE_H__
#define __HORDE_H__

    #include "config.h"
    #include "viking.h"

    /*============================================================================*
     * DESCRIÇÃO: Representa uma horda de vikings.                                *
     *============================================================================*/

    /**
     * @brief Define os atributos da horda vikings.
     */
    typedef struct horde
    {
        viking_t *vikings;      /* Array que armazena os vikings. */
        int normal_vikings;     /* Qtde. de vikings normais.      */
        int late_vikings;       /* Qtde. de vikings atrasados.    */
        int berserkers;         /* Qtde. de berserkers.           */

        chieftain_t *chieftain; /* Referência para chieftain.     */
        valhalla_t *valhalla;   /* Referência para valhalla.      */

        /* TODO: Adicione aqui os atributos que achar necessários para implementar o
        comportamento da horda de vikings. Esses atributos deverão ser usados pelas
        funções da horda de vikings. */
    } horde_t;

    /*============================================================================*
     * Funções utilizadas em arquivos que incluem esse .h                         *
     *============================================================================*/

    /**
     * @brief Inicializa a horda de vikings.
     * 
     * @param self A horda.
     * @param chieftain O chieftain.
     * @param valhalla Valhalla
     */
    extern void horde_init(horde_t *self, chieftain_t *chieftain, valhalla_t *valhalla);
    
    /**
     * @brief Finaliza a horda de vikings.
     * 
     * @param self A horda.
     */
    extern void horde_finalize(horde_t *self);

    /**
     * @brief Cria uma thread viking e a executa.
     *
     * @param self A horda.
     * @param berserker Se igual a 0 indica que é um viking normal, caso contrário,
     * indica que o viking é um berserker.
     * @param type Se NORMAL_VIKING participa do banquete e das preces. Se LATE_VIKING,
     * participa somente das preces.
     */
    extern void horde_spawn_viking(horde_t *self, int berserker, int type);

    /**
     * @brief Espera até que todos os vikings criados por viking_spawn() tenham
     * terminado sua execução.
     *
     * @param self A horda.
     */
    extern void horde_join(horde_t *self);

    /**
     * @brief Imprime informações da horda.
     * 
     * @param self A horda.
     */
    extern void horde_print(horde_t *self);

    /*============================================================================*
     * ATENCÃO: Insira aqui funções que você quiser adicionar a interface para    *
     * serem usadas em arquivos que incluem esse header.                          *
     *============================================================================*/

#endif /*__HORDE_H__*/