#ifndef __GOD_H__
#define __GOD_H__

    #include <pthread.h>
    #include "config.h"    

    /*============================================================================*
     * DESCRIÇÃO: Valhalla é o local onde ficam os deuses. É através de valhalla  *
     * que os vikings enviam suas preces.                                         *
     *============================================================================*/

    /**
     * @brief Define os deuses.
     */
    typedef enum god {
        BALDR, LOKI, /* primeiro e segundo deseus são rivais. */
        VALI, HODER, /* terceiro e quarto deuses são rivais.  */
        FRIGG, JORD, /* quinto e sexto deuses são rivais.     */
        ODIN,        /* sétimo deus não possui rivais.        */
        THOR         /* oitavo deus não possui rivais.        */
    } god_t;

    /**
     * @brief Define os atributos de valhalla.
     */
    typedef struct valhalla
    {
        unsigned int prayers[NUMBER_OF_GODS]; /* Controla o número de preces por deus. */

        /* TODO: Adicione aqui os atributos que achar necessários para implementar o
        comportamento de valhalla. Esses atributos deverão ser usados pelas funções
        de valhalla. */
    } valhalla_t;

    /*============================================================================*
    * Funções utilizadas em arquivos que incluem esse .h                         *
    *============================================================================*/

    /**
     * @brief Inicializa valhalla.
     */
    extern void valhalla_init(valhalla_t *self);

    /**
     * @brief Finaliza valhalla.
     */
    extern void valhalla_finalize(valhalla_t *self);

    /**
     * @brief Realiza uma prece para um deus.
     * 
     * @param god O deus que receberá a prece.
     */
    extern void valhalla_pray(valhalla_t *self, god_t god);

    /**
     * @brief Verifica se god é um super deus (ODIN ou THOR).
     * 
     * @param god Um deus.
     * 
     * @returns 1 se god é igual à ODIN ou THOR.
     */
    extern int valhalla_is_super(god_t god);

    /**
     * @brief Verifica se há desavença entre dois deuses.
     * 
     * @param god1 Um deus.
     * @param god2 Um deus.
     * 
     * @returns 1 se há desavenças entre god1 e god2.
     */
    extern int valhalla_is_rival(god_t god1, god_t god2);

    /**
     * @brief Retorna o deus rival de um deus.
     * 
     * @param god Um deus.
     * 
     * @returns O deus rival de god.
     */
    extern god_t valhalla_get_rival(god_t god);

    /**
     * @brief Retorna o nome do deus.
     * 
     * @param god Um deus.
     * 
     * @returns O nome do deus god.
     */
    extern char * valhalla_get_name(god_t god);

    /**
     * @brief Imprime as estatísticas de preces.
     */
    extern void valhalla_print(valhalla_t *self);

    /*============================================================================*
     * ATENCÃO: Insira aqui funções que você quiser adicionar a interface para    *
     * serem usadas em arquivos que incluem esse header.                          *
     *============================================================================*/

#endif /*__GOD_H__*/