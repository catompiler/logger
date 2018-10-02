#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*
 * Конфигурация FreeRTOS.
 * См. http://www.freertos.org/a00110.html
 */

// Тайминги и счётчики.
// Частота CPU.
#define configCPU_CLOCK_HZ                          72000000
// Частота системного таймера.
#define configTICK_RATE_HZ                          1000
// Использование 16 битного системного счётчика.
#define configUSE_16_BIT_TICKS                      0
// Использование останова системного счётчика.
#define configUSE_TICKLESS_IDLE                     0

// Память.
// Минимальный размер стека в машинных словах.
#define configMINIMAL_STACK_SIZE                    128
// Полный размер кучи.
#define configTOTAL_HEAP_SIZE                       4096
// Кучу выделяет приложение.
#define configAPPLICATION_ALLOCATED_HEAP            1
// Поддержка статического выделения памяти.
#define configSUPPORT_STATIC_ALLOCATION             1
// Поддержка динамического выделения памяти.
#define configSUPPORT_DYNAMIC_ALLOCATION            0
// Проверка переполнения стека.
#define configCHECK_FOR_STACK_OVERFLOW              2
// Сохранение максимального адреса стека.
#define configRECORD_STACK_HIGH_ADDRESS             0

// Планировщик.
// Использование вытеснения.
#define configUSE_PREEMPTION                        1
// Максимальное число приоритетов.
#define configMAX_PRIORITIES                        16
// Чередовать задачи с равным приоритетом.
#define configUSE_TIME_SLICING                      1
// Использование оптимизированного выбора задач.
#define configUSE_PORT_OPTIMISED_TASK_SELECTION     1

// Задачи.
// Максимальная длина имени задачи.
#define configMAX_TASK_NAME_LEN                     16
// Задача idle должна отдавать процессорное время.
#define configIDLE_SHOULD_YIELD                     1
// Использование структуры Newlib reent.
#define configUSE_NEWLIB_REENTRANT                  0
// Количество пользовательских указателей.
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS     0

// Таймеры.
// Использование таймеров.
#define configUSE_TIMERS                            1
// Приоритет задачи управления таймерами.
#define configTIMER_TASK_PRIORITY                   5
// Длина очереди команд управления таймерами.
#define configTIMER_QUEUE_LENGTH                    8
// Размер стека задачи управления таймерами.
#define configTIMER_TASK_STACK_DEPTH                (configMINIMAL_STACK_SIZE * 2)

// IPC.
// Уведомления потоков.
#define configUSE_TASK_NOTIFICATIONS                0
// Мютексы.
#define configUSE_MUTEXES                           0
// Рекурсивные мютексы.
#define configUSE_RECURSIVE_MUTEXES                 0
// Счётные семафоры.
#define configUSE_COUNTING_SEMAPHORES               0
// Максимум зарегистрированных очередей.
#define configQUEUE_REGISTRY_SIZE                   4
// Наборы очередей.
#define configUSE_QUEUE_SETS                        0

// Сопрограммы.
// Использование сопрограмм.
#define configUSE_CO_ROUTINES                       0
// Максимальное число приоритетов сопрограмм.
#define configMAX_CO_ROUTINE_PRIORITIES             2

// Хуки.
// Передача управления задаче idle.
#define configUSE_IDLE_HOOK                         0
// Ошибка при выделении памяти.
#define configUSE_MALLOC_FAILED_HOOK                0

// Статистика.
// Сбор статистики времени выполнения.
#define configGENERATE_RUN_TIME_STATS               1
// Функционал для трассировки и визуализации выполнения.
#define configUSE_TRACE_FACILITY                    1
// Функции получения форматированной статистики.
#define configUSE_STATS_FORMATTING_FUNCTIONS        0
// Первый запуск задачи управления таймерами.
#define configUSE_DAEMON_TASK_STARTUP_HOOK          0
// Тик системного счётчика.
#define configUSE_TICK_HOOK                         0
// Макросы для статистики времени выполнения.
#if configGENERATE_RUN_TIME_STATS != 0
// Настройка счётчика времени выполнения.
extern void initHiresCounter(void);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() initHiresCounter()
// Получение значение счётчика времени выполнения.
extern uint32_t getHiresCounterValue(void);
#define portGET_RUN_TIME_COUNTER_VALUE() getHiresCounterValue()
#endif

// Прерывания.
// Приоритет прерывания ядра.
#define configKERNEL_INTERRUPT_PRIORITY             255
// Максимальный приоритет прерываний, использующих
// системные вызовы FreeRTOS.
// См. http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html
// 143 (0x8f) соответствует 8 прерываниям (0-7),
// из которых нельзя вызывать функции FreeRTOS и
// 8 прерываниям (8-15), из которых можно вызывать
// функции FreeRTOS.
#define configMAX_SYSCALL_INTERRUPT_PRIORITY        143
// Обработчики системных прерываний.
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

// Дополнительные функции.
#define INCLUDE_vTaskPrioritySet                    0
#define INCLUDE_uxTaskPriorityGet                   0
#define INCLUDE_vTaskDelete                         1
#define INCLUDE_vTaskSuspend                        1
#define INCLUDE_xTaskResumeFromISR                  1
#define INCLUDE_vTaskDelayUntil                     0
#define INCLUDE_vTaskDelay                          1
#define INCLUDE_xTaskGetSchedulerState              0
#define INCLUDE_xTaskGetCurrentTaskHandle           0
#define INCLUDE_uxTaskGetStackHighWaterMark         0
#define INCLUDE_xTaskGetIdleTaskHandle              0
#define INCLUDE_eTaskGetState                       0
#define INCLUDE_xEventGroupSetBitFromISR            0
#define INCLUDE_xTimerPendFunctionCall              0
#define INCLUDE_xTaskAbortDelay                     0
#define INCLUDE_xTaskGetHandle                      0

//extern void vFreeRtosAssert(void);
//#define configASSERT( x ) if( ( x ) == 0 ) { vFreeRtosAssert(); }

#endif  //FREERTOS_CONFIG_H
