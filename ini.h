/**
 * @file ini.h Библиотека чтения/записи ini-файлов.
 */

#ifndef INI_H_
#define INI_H_

#include "errors/errors.h"
#include "defs/defs.h"
#include <stdint.h>
#include <stddef.h>


//! Пробельный символы.
#define INI_WHITESPACE_CHARS " \t\r\n"
//! Символы комментария.
#define INI_COMMENT_CHARS ";#"

//! Экспонента числа с фиксированной запятой.
#define INI_Q15_EXP 15
//! Знаменатель числа с фиксированной запятой.
#define INI_Q15_MOD (1 << (INI_Q15_EXP))
//! Знаменятель по основанию 10 числа с фиксированной запятой.
#define INI_Q15_MOD10 (10000)
//! Маска дробной части числа с фиксированной запятой.
#define INI_Q15_FRACT_MASK ((INI_Q15_MOD) - 1)
//! Формат числа с фиксированной запятой.
#define INI_Q15_STRFMT "%u.%04u"

//! Тип числа с фиксированной запятой.
typedef int32_t ini_q15_t;


//! Функция чтения очередной линии файла.
typedef char* (*ini_get_line_t)(char* line, int num, void* stream);
//! Функция записи очередной линии файла.
typedef int (*ini_put_line_t)(char* line, void* stream);
//! Функция установки на начало файла.
typedef void (*ini_rewind_t)(void* stream);


//! Перечисление ошибок разбора.
typedef enum _Ini_Error {
    INI_ERROR_NONE = 0, //!< Нет ошибки.
    INI_ERROR_UNEXPECTED_END = 1, //!< Неожиданный конец.
    INI_ERROR_UNEXPECTED_DATA = 2, //!< Неожидаемые данные.
    INI_ERROR_INVALID_NAME = 3 //!< Некорректное имя секции или ключа.
} ini_error_t;

//! Перечисление типов конструкций.
typedef enum _Ini_Expr_Type {
    INI_EXPR_EMPTY = 0, //!< Пустая строка.
    INI_EXPR_SECTION = 1, //!< Секция.
    INI_EXPR_KEYVALUE = 2 //!< Ключ-значение.
} ini_expr_type_t;


//! Каллбэк начала секции.
typedef void (*ini_on_section_t)(const char* section);
//! Каллбэк пары "ключ-значение".
typedef void (*ini_on_keyvalue_t)(const char* key, const char* value);
//! Каллбэк ошибки.
typedef void (*ini_on_error_t)(ini_error_t error, size_t line, size_t pos, const char* line_str);


//! Структура ini.
typedef struct _Ini {
    char* line; //!< Буфер линии.
    size_t line_size; //!< Размер буфера линии.
    void* stream; //!< Поток.
    // Ввод-вывод.
    ini_get_line_t get_line; //!< Функция получения линии.
    ini_put_line_t put_line; //!< Функция записи линии.
    ini_rewind_t rewind; //!< Функция установки на начало потока.
    // Каллбэки.
    ini_on_section_t on_section; //!< Секция.
    ini_on_keyvalue_t on_keyvalue; //!< Ключ-значение.
    ini_on_error_t on_error; //!< Ошибка.
} ini_t;

//! Структура инициализации ini.
typedef struct _Ini_Init {
    char* line; //!< Буфер линии.
    size_t line_size; //!< Размер буфера линии.
    // Ввод-вывод.
    ini_get_line_t get_line; //!< Функция получения линии.
    ini_put_line_t put_line; //!< Функция записи линии.
    ini_rewind_t rewind; //!< Функция установки на начало потока.
    // Каллбэки.
    ini_on_section_t on_section; //!< Секция.
    ini_on_keyvalue_t on_keyvalue; //!< Ключ-значение.
    ini_on_error_t on_error; //!< Ошибка.
} ini_init_t;


/**
 * Инициализирует парсер ini.
 * @param ini Парсер.
 * @param init Структура иницаилизации.
 * @return Код ошибки.
 */
extern err_t ini_init(ini_t* ini, ini_init_t* init);

/**
 * Устанавливает поток.
 * @param ini Парсер.
 * @param stream Поток.
 */
extern void ini_set_stream(ini_t* ini, void* stream);

/**
 * Парсит линию ini.
 * @param line Линия.
 * @param expr_type Тип выражения.
 * @param section Имя секции.
 * @param key Ключ.
 * @param value Значение.
 * @param err_pos Начало ошибки.
 * @return Ошибка парсинга.
 */
extern ini_error_t ini_parse_line(char* line, ini_expr_type_t* expr_type,
                                  char** section, char** key, char** value, char** err_pos);

/**
 * Парсит поток ini.
 * Вызывает каллбэки get_line,
 * on_section, on_keyvalue, on_error.
 * @param ini Парсер ini.
 * @return Код ошибки.
 */
extern err_t ini_parse(ini_t* ini);

/**
 * Ищет значение
 * заданного ключа
 * в заданной секции.
 * @param ini Парсер ini.
 * @param section Секция.
 * @param key Ключ.
 * @param defval Значение по-умолчанию.
 * @return Значение ключа.
 */
extern const char* ini_value(ini_t* ini, const char* section, const char* key, const char* defval);

/**
 * Ищет числовое значение
 * заданного ключа
 * в заданной секции.
 * @param ini Парсер ini.
 * @param section Секция.
 * @param key Ключ.
 * @param defval Значение по-умолчанию.
 * @return Значение ключа.
 */
extern int ini_valuei(ini_t* ini, const char* section, const char* key, int defval);

/**
 * Ищет числовое значение
 * с фиксированной запятой
 * заданного ключа
 * в заданной секции.
 * @param ini Парсер ini.
 * @param section Секция.
 * @param key Ключ.
 * @param defval Значение по-умолчанию.
 * @return Значение ключа.
 */
extern ini_q15_t ini_valuef(ini_t* ini, const char* section, const char* key, ini_q15_t defval);

/**
 * Записывает секцию в файл.
 * @param ini Парсер ini.
 * @param section Имя секции.
 * @return Код ошибки.
 */
extern err_t ini_write_section(ini_t* ini, const char* section);

/**
 * Записывает пару ключ-значение в файл.
 * @param ini Парсер ini.
 * @param key Ключ.
 * @param value Значение.
 * @return Код ошибки.
 */
extern err_t ini_write_keyvalue(ini_t* ini, const char* key, const char* value);

/**
 * Записывает пару ключ-значение в файл.
 * @param ini Парсер ini.
 * @param key Ключ.
 * @param value Значение.
 * @return Код ошибки.
 */
extern err_t ini_write_keyvaluei(ini_t* ini, const char* key, int value);

/**
 * Записывает пару ключ-значение в файл.
 * @param ini Парсер ini.
 * @param key Ключ.
 * @param value Значение.
 * @return Код ошибки.
 */
extern err_t ini_write_keyvaluef(ini_t* ini, const char* key, ini_q15_t value);

#endif /* INI_H_ */
