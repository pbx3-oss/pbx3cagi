#include <stdio.h>
#include "pbx3cagi.h"
#include "cagi.h"
#include "agi_wrap.h"

int agi_answer(struct agi_session *s)
{
    return AGITool_answer(s->agi, s->res);
}

int agi_exec(struct agi_session *s, char *application, char *options)
{
    return AGITool_exec(s->agi, s->res, application, options);
}

int agi_get_data(struct agi_session *s, char *filename, int timeout, int max_digits)
{
    return AGITool_get_data(s->agi, s->res, filename, timeout, max_digits);
}

int agi_get_variable(struct agi_session *s, char *variable)
{
    return AGITool_get_variable(s->agi, s->res, variable);
}

int agi_set_variable(struct agi_session *s, char *variable, char *value)
{
    return AGITool_set_variable(s->agi, s->res, variable, value);
}

int agi_set_callerid(struct agi_session *s, char *cid)
{
    return AGITool_set_callerid(s->agi, s->res, cid);
}

int agi_set_context(struct agi_session *s, char *context)
{
    return AGITool_set_context(s->agi, s->res, context);
}

int agi_set_extension(struct agi_session *s, char *extension)
{
    return AGITool_set_extension(s->agi, s->res, extension);
}

int agi_set_priority(struct agi_session *s, int priority)
{
    return AGITool_set_priority(s->agi, s->res, priority);
}

int agi_stream_file(struct agi_session *s, char *filename, char *escape_digits, int offset)
{
    return AGITool_stream_file(s->agi, s->res, filename, escape_digits, offset);
}

int agi_verbose(struct agi_session *s, char *message, int level)
{
    return AGITool_verbose(s->agi, s->res, message, level);
}

int agi_record_file(struct agi_session *s, char *file, char *format, char *escape_digits,
                    int timeout, int beep, int silence, int offset)
{
    return AGITool_record_file(s->agi, s->res, file, format, escape_digits,
                               timeout, beep, silence, offset);
}

int agi_database_get(struct agi_session *s, char *family, char *key)
{
    return AGITool_database_get(s->agi, s->res, family, key);
}

int agi_database_put(struct agi_session *s, char *family, char *key, char *value)
{
    return AGITool_database_put(s->agi, s->res, family, key, value);
}

int agi_database_del(struct agi_session *s, char *family, char *key)
{
    return AGITool_database_del(s->agi, s->res, family, key);
}
