#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdio.h>

/*============================================================================*
 * Configuração                                                               *
 *============================================================================*/

#define NUMBER_OF_GODS           8
#define RIVAL_TOLERANCE_RATE     0.05
#define SUPER_GOD_TOLERANCE_RATE 0.10

#define NORMAL_VIKING            0
#define LATE_VIKING              1
#define WAIT_LATE_VIKING         100

typedef struct config {
    unsigned int horde_size;
    unsigned int table_size;
    unsigned int max_pray_time;
    unsigned int max_eat_time;
} config_t;

extern config_t config;

/*============================================================================*
 * Macros                                                                     *
 *============================================================================*/

/**
* @brief Imprime um log em modo debug.
*/
#ifndef NDEBUG
    #define plog(...) printf(__VA_ARGS__); fflush(stdout)
#else
    #define plog(...)
#endif

/**
* @brief Serapador.
*/
#define BAR "=====================================================================\n"


/*============================================================================*
 * Funções                                                                    *
 *============================================================================*/

/**
* @brief Puts the current thread to sleep.
* 
* @param msecs Time in milliseconds.
* 
* @returns 0 em caso de sucesso e -1 em caso de erro.
*/
extern int msleep(long msec);

#endif /*__CONFIG_H__*/