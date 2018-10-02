/**
 * @file ain.h Аналоговые входа.
 */

#ifndef AIN_H_
#define AIN_H_

#include "errors/errors.h"
#include "FreeRTOS.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "q15.h"


//! Число аналоговых входов.
#define AIN_CHANNELS_COUNT 5

//! Частота оверсемплинга.
#define AIN_OVERSAMPLE_FREQ 12800

//! Частота семплирования.
#define AIN_SAMPLE_FREQ 1600

//! Кратность оверсемплинга.
#define AIN_OVERSAMPLE_RATE ((AIN_OVERSAMPLE_FREQ) / (AIN_SAMPLE_FREQ))

//! Частота сети.
#define AIN_POWER_FREQ 50

//! Число точек за период.
#define AIN_PERIOD_SAMPLES ((AIN_SAMPLE_FREQ) / (AIN_POWER_FREQ))

//! Число точек вычисления действующих значений.
#define AIN_EFF_SAMPLES AIN_PERIOD_SAMPLES

//! Максимальная длина имени канала.
#define AIN_NAME_LEN 16

//! Максимальная длина единицы измерения канала.
#define AIN_UNIT_LEN 8

// Каналы входов.
//! Ua.
#define AIN_Ua 0
//! Ub.
#define AIN_Ub 1
//! Uc.
#define AIN_Uc 2
//! Udc.
#define AIN_Udc 3
//! In0.
#define AIN_0 4

//! Тип канала.
typedef enum _Ain_Channel_Type {
    AIN_DC = 0, //!< DC.
    AIN_AC = 1 //!< AC.
} ain_channel_type_t;

//! Тип действующего значения.
typedef enum _Ain_Channel_Eff_Type {
    AIN_AVG = 0, //!< AVG.
    AIN_RMS = 1 //!< RMS.
} ain_channel_eff_type_t;


/**
 * Инициализирует аналоговые входа.
 * @return Код ошибки.
 */
extern err_t ain_init(void);

/**
 * Устанавливает разрешение измерения аналоговых входов.
 * @param enabled Разрешение.
 */
extern void ain_set_enabled(bool enabled);

/**
 * Получает разрешение измерения аналоговых входов.
 * @return Разрешение.
 */
extern bool ain_enabled(void);

/**
 * Сбрасывает аналоговые входа.
 */
extern void ain_reset(void);

/**
 * Инициализирует канал АЦП.
 * @param n Номер канала.
 * @param type Тип канала.
 * @param eff_type Тип вычисления действующего значения.
 * @param offset Смещение АЦП.
 * @param inst_gain Коэффициент АЦП.
 * @param eff_gain Коэффициент действующего значения.
 * @param real_k Коэффициент преобразования в реальную величину.
 * @return Код ошибки.
 */
extern err_t ain_init_channel(size_t n, ain_channel_type_t type, ain_channel_eff_type_t eff_type,
                              uint16_t offset, q15_t inst_gain, iq15_t eff_gain);

/**
 * Устанавливает коэффициент для преобразования в реальную величину.
 * @param n Номер канала.
 * @param real_k Коэффициент преобразования в реальную величину.
 * @return Код ошибки.
 */
extern err_t ain_channel_set_real_k(size_t n, iq15_t real_k);

/**
 * Получает коэффициент для преобразования в реальную величину.
 * @param n Номер канала.
 * @return Коэффициент преобразования в реальную величину.
 */
extern iq15_t ain_channel_real_k(size_t n);

/**
 * Устанавливает имя канала.
 * @param n Номер канала.
 * @param name Имя канала.
 * @return Код ошибки.
 */
extern err_t ain_channel_set_name(size_t n, const char* name);

/**
 * Получает имя канала.
 * @param n Номер канала.
 * @return Имя канала.
 */
extern const char* ain_channel_name(size_t n);

/**
 * Устанавливает единицу измерения канала.
 * @param n Номер канала.
 * @param unit Единица измерения канала.
 * @return Код ошибки.
 */
extern err_t ain_channel_set_unit(size_t n, const char* unit);

/**
 * Получает единицу измерения канала.
 * @param n Номер канала.
 * @return Единица измерения канала.
 */
extern const char* ain_channel_unit(size_t n);

/**
 * Устанавливает разрешение измерения канала.
 * @param n Номер канала.
 * @param enabled Разрешение канала.
 * @return Код ошибки.
 */
extern err_t ain_channel_set_enabled(size_t n, bool enabled);

/**
 * Получает разрешение измерения канала.
 * @param n Номер канала.
 * @return Разрешение канала.
 */
extern bool ain_channel_enabled(size_t n);

/**
 * Сбрасывает канал.
 * @param n Номер канала.
 * @return Код ошибки.
 */
extern err_t ain_channel_reset(size_t n);

/**
 * Помещает в очередь для обработки
 * данные от АЦП в количестве
 * AIN_CHANNELS_COUNT каналов.
 * @param adc_data Указатель на данные.
 * @param pxHigherPriorityTaskWoken Флаг необходимости переключения задачи.
 * @return Код ошибки.
 */
extern err_t ain_process_adc_data_isr(uint16_t* adc_data, BaseType_t* pxHigherPriorityTaskWoken);

/**
 * Получает мгновенное значение аналогового входа.
 * @param n Номер канала.
 * @return Значение.
 */
extern q15_t ain_value_inst(size_t n);

/**
 * Получает действующее значение аналогового входа.
 * @param n Номер канала.
 * @return Значение.
 */
extern q15_t ain_value(size_t n);

/**
 * Преобразует относительное значение
 * канала в абсолютное.
 * @param n Номер канала.
 * @param value Значение.
 * @return Значение.
 */
extern iq15_t ain_q15_to_real(size_t n, q15_t value);

/**
 * Преобразует абсолютное значение
 * канала в относительное.
 * @param n Номер канала.
 * @param value Значение.
 * @return Значение.
 */
extern q15_t ain_real_to_q15(size_t n, iq15_t value);

#endif /* AIN_H_ */
