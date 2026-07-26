#ifndef AGI_WRAP_H
#define AGI_WRAP_H

/* Thin AGI facade over cagi AGITool_* (Phase 3.2). Leave cagi.c untouched. */
struct agi_session;

int agi_answer(struct agi_session *s);
int agi_exec(struct agi_session *s, char *application, char *options);
int agi_get_data(struct agi_session *s, char *filename, int timeout, int max_digits);
int agi_get_variable(struct agi_session *s, char *variable);
int agi_set_variable(struct agi_session *s, char *variable, char *value);
int agi_set_callerid(struct agi_session *s, char *cid);
int agi_set_context(struct agi_session *s, char *context);
int agi_set_extension(struct agi_session *s, char *extension);
int agi_set_priority(struct agi_session *s, int priority);
int agi_stream_file(struct agi_session *s, char *filename, char *escape_digits, int offset);
int agi_verbose(struct agi_session *s, char *message, int level);
int agi_record_file(struct agi_session *s, char *file, char *format, char *escape_digits,
                    int timeout, int beep, int silence, int offset);
int agi_database_get(struct agi_session *s, char *family, char *key);
int agi_database_put(struct agi_session *s, char *family, char *key, char *value);
int agi_database_del(struct agi_session *s, char *family, char *key);

#endif
