#include "ini.h"
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

static const char* ini_space_chars = INI_WHITESPACE_CHARS;
static const char* ini_comment_chars = INI_COMMENT_CHARS;

err_t ini_init(ini_t* ini, ini_init_t* init)
{
    if(init == NULL) return E_NULL_POINTER;
    if(init->line == NULL) return E_NULL_POINTER;
    if(init->line_size == 0) return E_INVALID_VALUE;
    if(init->get_line == NULL && init->put_line == NULL) return E_NULL_POINTER;

    ini->line = init->line;
    ini->line_size = init->line_size;
    ini->stream = NULL;
    ini->get_line = init->get_line;
    ini->put_line = init->put_line;
    ini->rewind = init->rewind;
    ini->on_section = init->on_section;
    ini->on_keyvalue = init->on_keyvalue;
    ini->on_error = init->on_error;

    return E_NO_ERROR;
}

void ini_set_stream(ini_t* ini, void* stream)
{
	ini->stream = stream;
}

/**
 * Позиционирует поток на начало.
 * @param ini Парсер ini.
 */
ALWAYS_INLINE static void ini_rewind(ini_t* ini)
{
	if(ini->rewind) ini->rewind(ini->stream);
}

/**
 * Получает очередную линию из потока.
 * @param ini Парсер ini.
 * @param stream Поток.
 * @return Флаг получения строки.
 */
ALWAYS_INLINE static bool ini_get_line(ini_t* ini)
{
    return ini->get_line(ini->line, ini->line_size, ini->stream) != NULL;
}

/**
 * Записывает очередную линию в поток.
 * @param ini Парсер ini.
 * @param stream Поток.
 * @return Флаг записи строки.
 */
ALWAYS_INLINE static bool ini_put_line(ini_t* ini)
{
    return ini->put_line(ini->line, ini->stream) != EOF;
}

/**
 * Проверяет строку на начало комментария.
 * @param str Строка.
 * @return Флаг комментария.
 */
static bool ini_is_comment(char* str)
{
    return *str && strchr(ini_comment_chars, *str) != NULL;
}

/**
 * Проверяет строку на пробельный символ.
 * @param str Строка.
 * @return Флаг пробельного символа.
 */
static bool ini_is_ws(char* str)
{
    return *str && strchr(ini_space_chars, *str) != NULL;
}

/**
 * Проверяет строку на конец.
 * @param str Строка.
 * @return Флаг конца строки.
 */
static bool ini_eol(char* str)
{
    return *str == '\0';
}

/**
 * Проверяет строку на конец полезных данных.
 * @param str Строка.
 * @return Флаг конца полезных данных.
 */
static bool ini_eod(char* str)
{
    return ini_eol(str) || ini_is_comment(str);
}

/**
 * Пропускает пробельные символы.
 * @param str Строка.
 * @return Указатель на непробельный символ.
 */
static char* ini_skip_spaces(char* str)
{
    size_t n = strspn(str, ini_space_chars);
    return str + n;
}

/**
 * Пропускает пробельные символы справа.
 * @param str Строка.
 * @return Указатель на непробельный символ.
 */
static char* ini_rskip_spaces(char* str, char* rstr)
{
    char* rnext = NULL;

    while(rstr > str){
        rnext = rstr - 1;
        if(!ini_is_ws(rnext)) break;
        rstr = rnext;
    }

    return rstr;
}

/**
 * Получает флаг допустимого для имени символа.
 * @param c Символ.
 * @return Флаг допустимого для имени символа.
 */
static bool ini_is_name_char(char c)
{
    if(isalnum((int)(unsigned char)c)) return true;
    if(c == '.') return true;
    if(c == '_') return true;
    return false;
}

/**
 * Пропускает имя.
 * @param str Строка.
 * @return Указатель на строку после имени.
 */
static char* ini_skip_name(char* str)
{
    while(*str){
        if(!ini_is_name_char(*str)) break;
        str ++;
    }
    return str;
}

/**
 * Пропускает значение.
 * @param str Строка.
 * @return Указатель на строку после значения.
 */
static char* ini_skip_value(char* str)
{
    //char* start_str = str;

    while(*str){
        if(ini_eod(str)) break;
        str ++;
    }

    return str;
}

/**
 * Получает флаг начала секции.
 * @param str Строка.
 * @return Флаг начала секции.
 */
static bool ini_is_section_begin(char* str)
{
    return *str == '[';
}

/**
 * Получает флаг конца секции.
 * @param str Строка.
 * @return Флаг конца секции.
 */
static bool ini_is_section_end(char* str)
{
    return *str == ']';
}

/**
 * Парсит строку с именем секции.
 * @param str Строка.
 * @param sect Имя секции.
 * @param err_pos Позиция ошибки.
 * @return Ошибка.
 */
static ini_error_t ini_parse_section(char* str, char** sect, char** err_pos)
{
    // Начало и конец секции.
    char* sect_beg = NULL, *sect_end = NULL;

    // Пропуск [.
    str ++;

    // Пропустим пробелы.
    str = ini_skip_spaces(str);
    // Если конец данных - ошибка, ожидается имя секции.
    if(ini_eod(str)){
        if(err_pos) *err_pos = str;
        return INI_ERROR_UNEXPECTED_END;
    }

    // Начало имени секции.
    sect_beg = str;

    // Пропуск имени.
    str = ini_skip_name(str);

    // Конец имени секции.
    sect_end = str;

    // Если имя имеет нулевую длину - некорректное имя.
    if(sect_end == sect_beg){
        if(err_pos) *err_pos = str;
        return INI_ERROR_INVALID_NAME;
    }

    // Если конец данных - ошибка, ожидается конец секции.
    if(ini_eod(str)){
        if(err_pos) *err_pos = str;
        return INI_ERROR_UNEXPECTED_END;
    }
    // Если не конец секции и не пробельный символ - некорректное имя.
    if(!ini_is_section_end(str) && !ini_is_ws(str)){
        if(err_pos) *err_pos = str;
        return INI_ERROR_INVALID_NAME;
    }

    // Пропустим пробелы.
    str = ini_skip_spaces(str);
    // Еслим конец данных - ошибка, ожидается конец секции.
    if(ini_eod(str)){
        if(err_pos) *err_pos = str;
        return INI_ERROR_UNEXPECTED_END;
    }
    // Если не конец секции - ошибка, ожидается конец секции.
    if(!ini_is_section_end(str)){
        if(err_pos) *err_pos = str;
        return INI_ERROR_UNEXPECTED_DATA;
    }

    // Пропуск ].
    str ++;

    // Пропустим пробелы.
    str = ini_skip_spaces(str);
    // Если не конец данных - ошибка, ожидается конец данных.
    if(!ini_eod(str)){
        if(err_pos) *err_pos = str;
        return INI_ERROR_UNEXPECTED_DATA;
    }

    // Конец имени секции.
    *sect_end = '\0';

    // Секция.
    *sect = sect_beg;

    // on_section(sect_beg);
    //printf("Section: \"%s\"\n", sect_beg);

    // Позицию ошибки на конец строки.
    if(err_pos) *err_pos = str;

    return INI_ERROR_NONE;
}

/**
 * Получает флаг начала имени.
 * @param str Строка.
 * @return Флаг начала имени.
 */
ALWAYS_INLINE static bool ini_is_name(char* str)
{
    return ini_is_name_char(*str);
}

/**
 * Получает флаг наличия символа равенства.
 * @param str Строка.
 * @return Флаг наличия символа равенства.
 */
ALWAYS_INLINE static bool ini_is_eq(char* str)
{
    return *str == '=';
}

/**
 * Парсит строку ключ-значение.
 * @param str Строка.
 * @param key Имя ключа.
 * @param value Значение.
 * @param err_pos Позиция ошибки.
 * @return Ошибка.
 */
static ini_error_t ini_parse_keyvalue(char* str, char** key, char** value, char** err_pos)
{
    // Начало и конец ключа.
    char* key_beg = NULL, *key_end = NULL;
    // Начало и конец значения.
    char* val_beg = NULL, *val_end = NULL;

    // Начало имени ключа.
    key_beg = str;

    str = ini_skip_name(str);

    // Конец имени ключа.
    key_end = str;

    // Если имя ключа имеет нулевую длину - некорректное имя.
    if(key_beg == key_end){
        if(err_pos) *err_pos = str;
        return INI_ERROR_INVALID_NAME;
    }

    // Если конец данных - ошибка, ожидается символ равно.
    if(ini_eod(str)){
        if(err_pos) *err_pos = str;
        return INI_ERROR_UNEXPECTED_END;
    }
    // Если не символ равенства и не пробельный символ - некорректное имя.
    if(!ini_is_eq(str) && !ini_is_ws(str)){
        if(err_pos) *err_pos = str;
        return INI_ERROR_INVALID_NAME;
    }

    // Пропустим пробелы.
    str = ini_skip_spaces(str);
    // Еслим конец данных - ошибка, ожидается символ равенства.
    if(ini_eod(str)){
        if(err_pos) *err_pos = str;
        return INI_ERROR_UNEXPECTED_END;
    }
    // Если не конец секции - ошибка, ожидается символ равенства.
    if(!ini_is_eq(str)){
        if(err_pos) *err_pos = str;
        return INI_ERROR_UNEXPECTED_DATA;
    }

    // Пропуск '='.
    str ++;

    // Пропустим пробелы.
    str = ini_skip_spaces(str);
    // Еслим конец данных - ошибка, ожидается конец секции.
    if(ini_eod(str)){
        if(err_pos) *err_pos = str;
        return INI_ERROR_UNEXPECTED_END;
    }

    // Начало значения.
    val_beg = str;

    // Пропустим символы значения - до конца строки.
    str = ini_skip_value(str);

    // ini_skip_value ищет конец данных.
    // Пропустим пробелы.
    //str = ini_skip_spaces(str);
    // Если не конец данных - ошибка, ожидается конец данных.
    //if(!ini_eod(str)){
    //    if(err_pos) *err_pos = str;
    //    return INI_ERROR_UNEXPECTED_DATA;
    //}

    // Для конца значения отбросим пробельные символы.
    val_end = ini_rskip_spaces(val_beg, str);

    // Конца имён ключа и значения.
    *key_end = '\0';
    *val_end = '\0';

    // Ключ и значение.
    *key = key_beg;
    *value = val_beg;

    // on_keyval(key_beg, val_beg);
    //printf("Key: \"%s\" Value: \"%s\"\n", key_beg, val_beg);

    // Позицию ошибки на конец строки.
    if(err_pos) *err_pos = str;

    return INI_ERROR_NONE;
}

ini_error_t ini_parse_line(char* line, ini_expr_type_t* expr_type,
                                  char** section, char** key, char** value, char** err_pos)
{
    // Линия.
    //char* line = ini->line;

    line = ini_skip_spaces(line);

    // Конец строки.
    if(ini_eod(line)){
        *expr_type = INI_EXPR_EMPTY;
        return INI_ERROR_NONE;
    }
    // Секция.
    else if(ini_is_section_begin(line)){
        *expr_type = INI_EXPR_SECTION;
        return ini_parse_section(line, section, err_pos);
    }
    // Ключ - значение.
    else if(ini_is_name(line)){
        *expr_type = INI_EXPR_KEYVALUE;
        return ini_parse_keyvalue(line, key, value, err_pos);
    }

    if(err_pos) *err_pos = line;

    return INI_ERROR_INVALID_NAME;
}

/**
 * Вызывает каллбэк ошибки.
 * @param ini Парсер ini.
 * @param error Ошибка.
 * @param line Линия.
 * @param pos Символ.
 * @param line_str Начало ошибки.
 */
ALWAYS_INLINE static void ini_on_error(ini_t* ini, ini_error_t error, size_t line, size_t pos, const char* line_str)
{
    if(ini->on_error) ini->on_error(error, line, pos, line_str);
}

/**
 * Вызывает каллбэк секции.
 * @param ini Парсер ini.
 * @param section Имя секции.
 */
ALWAYS_INLINE static void ini_on_section(ini_t* ini, const char* section)
{
    if(ini->on_section) ini->on_section(section);
}

/**
 * Вызывает каллбэк ключ-значения.
 * @param ini Парсер ini.
 * @param key Имя ключ.
 * @param value Значение.
 */
ALWAYS_INLINE static void ini_on_keyvalue(ini_t* ini, const char* key, const char* value)
{
    if(ini->on_keyvalue) ini->on_keyvalue(key, value);
}

err_t ini_parse(ini_t* ini)
{
    if(ini->get_line == NULL) return E_STATE;

    ini_error_t ini_err = INI_ERROR_NONE;
    ini_expr_type_t type = INI_EXPR_EMPTY;
    char* section = NULL;
    char* key = NULL, *value = NULL;

    char* err_pos = NULL;
    size_t line = 0;

    while(ini_get_line(ini)){
        line ++;
        ini_err = ini_parse_line(ini->line, &type, &section, &key, &value, &err_pos);
        if(ini_err == INI_ERROR_NONE){
            switch(type){
            default:
                break;
            case INI_EXPR_SECTION:
                ini_on_section(ini, section);
                break;
            case INI_EXPR_KEYVALUE:
                ini_on_keyvalue(ini, key, value);
                break;
            }
        }else{
            //printf("Parse error %d at line %d: %s\n", (int)ini_err, (int)line, err_pos);
            ini_on_error(ini, ini_err, line, err_pos - ini->line, ini->line);
        }
    }

    return E_NO_ERROR;
}

const char* ini_value(ini_t* ini, const char* section, const char* key, const char* defval)
{
    if(ini->get_line == NULL) return defval;

    ini_error_t ini_err = INI_ERROR_NONE;
    ini_expr_type_t type = INI_EXPR_EMPTY;

    char* ini_section = NULL;
    char* ini_key = NULL, *ini_value = NULL;

    bool section_finded = false;

    ini_rewind(ini);

    while(ini_get_line(ini)){
        ini_err = ini_parse_line(ini->line, &type, &ini_section, &ini_key, &ini_value, NULL);
        if(ini_err == INI_ERROR_NONE){
            switch(type){
            default:
                break;
            case INI_EXPR_SECTION:
                section_finded = (strcmp(ini_section, section) == 0);
                break;
            case INI_EXPR_KEYVALUE:
                if(section_finded){
                    if(strcmp(ini_key, key) == 0){
                        return ini_value;
                    }
                }
                break;
            }
        }
    }

    return defval;
}

int ini_valuei(ini_t* ini, const char* section, const char* key, int defval)
{
    // Получим строковое значение.
    const char* str_val = ini_value(ini, section, key, NULL);
    // Если NULL - возврат значения по-умолчанию.
    if(str_val == NULL) return defval;

    char* end_val = NULL;

    int res = strtol(str_val, &end_val, 0);

    // Если неправильное число - возврат значния по-умолчанию.
    if(end_val == NULL ||  *end_val != '\0') return defval;

    return res;
}

/**
 * Преобразует число в дробной части в
 * дробную часть числа с фиксированной запятой.
 * @param num Число в дробной части.
 * @return Число с фиксированной запятой.
 */
static ini_q15_t ini_strtoq_i2f(int num)
{
    int64_t res64 = 0;
    int d = 10;

    while(num > d){
        d *= 10;
    }

    res64 = (int64_t)num << INI_Q15_EXP;
    res64 /= d;

    return (ini_q15_t)res64;
}

/**
 * Преобразует строку в число с фиксированной запятой.
 * @param str Строка.
 * @param endptr Указатель на следующий символ.
 * @param base Основание.
 * @return Число с фиксированной запятой.
 */
static ini_q15_t ini_strtoq(const char* str, char** endptr, int base)
{
    char* endstr = NULL;

    int int_part = 0, fract_part = 0, exp_part = 0;
    ini_q15_t fract_q_part = 0;

    int64_t res64 = 0;

    int_part = strtol(str, &endstr, base);

    if(*endstr == '.' || *endstr == ','){
        endstr ++;
        fract_part = strtol(endstr, &endstr, base);
        fract_q_part = ini_strtoq_i2f(fract_part);
    }

    if(*endstr == 'e' || *endstr == 'E' ){
        endstr ++;
        exp_part = strtol(endstr, &endstr, base);
    }

    if(endptr) *endptr = endstr;

    // Int part.
    res64 = (int64_t)int_part << INI_Q15_EXP;
    // Fract part.
    if(res64 >= 0) res64 += fract_q_part;
    else res64 -= fract_q_part;
    // Exp.
    while(exp_part > 0){ res64 *= 10; exp_part --; }
    while(exp_part < 0){ res64 /= 10; exp_part ++; }

    // Clamp.
    if(res64 > INT32_MAX) res64 = INT32_MAX;
    if(res64 < INT32_MIN) res64 = INT32_MIN;

    return (ini_q15_t) res64;
}

/**
 * Преобразует число с фиксированной запятой в строку.
 * @param q Число с фиксированной запятой.
 * @param str Строка.
 * @param max_len Максимальное число символов.
 * @return Число записанных символов.
 */
static int ini_qtostr(ini_q15_t q, char* str, int max_len)
{
    const char* sign = NULL;

    int32_t int_part = 0;
    int32_t fract_part = 0;

    if(q > 0){
        sign = "";
    }else{
        sign = "-";
        q = -q;
    }

    int_part = q >> INI_Q15_EXP;
    fract_part = (((int64_t)q & INI_Q15_FRACT_MASK) * INI_Q15_MOD10) >> INI_Q15_EXP;

    return snprintf(str, max_len, "%s"INI_Q15_STRFMT, sign,
                    (unsigned int)int_part, (unsigned int)fract_part);
}

ini_q15_t ini_valuef(ini_t* ini, const char* section, const char* key, ini_q15_t defval)
{
    // Получим строковое значение.
    const char* str_val = ini_value(ini, section, key, NULL);
    // Если NULL - возврат значения по-умолчанию.
    if(str_val == NULL) return defval;

    char* end_val = NULL;

    ini_q15_t res = ini_strtoq(str_val, &end_val, 0);

    // Если неправильное число - возврат значния по-умолчанию.
    if(end_val == NULL ||  *end_val != '\0') return defval;

    return res;
}

err_t ini_write_section(ini_t* ini, const char* section)
{
    if(ini->put_line == NULL) return E_STATE;
    if(section == NULL) return E_NULL_POINTER;

    int n = snprintf(ini->line, ini->line_size, "[%s]\n", section);
    if(n < 0 || n >= ini->line_size) return E_INVALID_VALUE;

    if(!ini_put_line(ini)) return E_IO_ERROR;

    return E_NO_ERROR;
}

err_t ini_write_keyvalue(ini_t* ini, const char* key, const char* value)
{
    if(ini->put_line == NULL) return E_STATE;
    if(key == NULL) return E_NULL_POINTER;
    if(value == NULL) return E_NULL_POINTER;

    int n = snprintf(ini->line, ini->line_size, "%s=%s\n", key, value);
    if(n < 0 || n >= ini->line_size) return E_INVALID_VALUE;

    if(!ini_put_line(ini)) return E_IO_ERROR;

    return E_NO_ERROR;
}

err_t ini_write_keyvaluei(ini_t* ini, const char* key, int value)
{
    if(ini->put_line == NULL) return E_STATE;
    if(key == NULL) return E_NULL_POINTER;

    int n = snprintf(ini->line, ini->line_size, "%s=%d\n", key, value);
    if(n < 0 || n >= ini->line_size) return E_INVALID_VALUE;

    if(!ini_put_line(ini)) return E_IO_ERROR;

    return E_NO_ERROR;
}

err_t ini_write_keyvaluef(ini_t* ini, const char* key, ini_q15_t value)
{
    if(ini->put_line == NULL) return E_STATE;
    if(key == NULL) return E_NULL_POINTER;

    char* line = ini->line;
    size_t size = ini->line_size;
    int n = 0;

    n = snprintf(line, size, "%s=", key);
    if(n < 0 || n >= size) return E_INVALID_VALUE;
    line += n;
    size -= n;

    n = ini_qtostr(value, line, size);
    if(n < 0 || n >= size) return E_INVALID_VALUE;
    line += n;
    size -= n;

    n = snprintf(line, size, "%s", "\n");
    if(n < 0 || n >= size) return E_INVALID_VALUE;

    if(!ini_put_line(ini)) return E_IO_ERROR;

    return E_NO_ERROR;
}
