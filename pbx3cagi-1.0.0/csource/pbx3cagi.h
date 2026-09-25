/*
* function prototypes for PBX3CAGI.c
*
*/
  
#ifndef _PBX3CAGI_H
#define _PBX3CAGI_H

#include <stddef.h>

#define MAX_NUM_LEN 32
#define MAX_TRANSFORM_LEN 65
#define MAX_NUM_TRANSFORM 128
#define MAX_TRANSFORM_LIST_LEN 1024
#define MAX_PRESEL_LEN 8
#define MAX_DIALNUM_LEN 32
#define MAX_RING_LEN 16
#define MAX_DIAL_STATUS_LEN 16
#define MAX_EXT_LEN 64
#define MAX_CALL_FWD_CHAIN_LEN 128
#define MAX_FWD_NUM_LEN 32
#define MAX_DB_LINE_LEN 65536
#define MAX_DIALSTR_LEN 256
#define MAX_FILENAME_LEN 512
#define MAX_CLUSTER_LEN 64
#define MAX_COS_LEN 32
#define MAX_NUM_KEYS 1024
#define MAX_KEY_LEN 64
#define MAX_MSG_LEN 1024
#define MAX_SQL_COLS 32		/* Maximum number of columns in a single query */
#define MAX_SQL_CLEN 1024   /* Maximum SQL field length */
#define MAX_REC_LEN 4096 /* Maximum size of input buffer */
#define CR 13            /* Decimal code of Carriage Return char */
#define LF 10            /* Decimal code of Line Feed char */
#define EOF_MARKER 26    /* Decimal code of DOS end-of-file marker */
#define TRUE 1
#define FALSE 0
/* Dialplan AGI argv (script name at [0]); see g_parms. */
#define PARM_NUM (g_parms.argc)
#define PARM_CMD (*(g_parms.argv+1))
#define PARM_KEY (*(g_parms.argv+2))
#define PARM_CLST (*(g_parms.argv+3))
#define PARM_PM1 (*(g_parms.argv+4))
#define PARM_PM2 (*(g_parms.argv+5))
#define PARM_PM3 (*(g_parms.argv+6))
#define SQLITEDB "/opt/pbx3/db/sqlite.rdonly.db"
#define SOUNDIR "/usr/share/asterisk/sounds/"
#define QLOG "/var/log/asterisk/queue_log" 
#define SIPDRIVER "PJSIP"
#define ASTDLIM ","

/* Incomplete types — full defs in cagi.h (avoid typedef redefinition). */
struct _asterisk_tools_;
struct _asterisk_cmd_result_;

/* Tenant cluster row: loaded once per AGI invocation from SQLite `cluster` (replaces generator-injected Asterisk globals). */
typedef struct cluster_cfg {
    int loaded;
    int abstimeout_sec;
    char voipmax_str[16];
    char callrecord_1[32];
    char allowhashxfer[32];
    char playbeep[8];
    char playbusy[8];
    char playcongested[8];
    char playtransfer[8];
    char voiceinstr[8];
    char bounce_alert[256];
    char blind_busy[128];
    char int_ring_delay[16];
    char maxin_str[16];
    char ringdelay_str[16];
    char lterm_str[8];
    char cfwd_progress[16];
    char cfwd_answer[16];
    char ivr_key_wait[8];
    char ivr_digit_wait_str[16];
    char syspass[64];
    char spy_pass[64];
    char dynamicfeatures[512];
    char clusterclid[MAX_EXT_LEN];
    char chanmax_str[16];
    char usemohcustom[8];
    char masteroclo[16];
    char oclo[16];
    char routeoverride[32];
    char sched_mode[32];           /* day-parts timer mode (dual-read with oclo) */
    char holiday_force_dest[32];   /* optional redesign field; may mirror routeoverride */
    char fqdn[128]; /* tenant SIP domain (cluster.fqdn / cname) for RURI */
} cluster_cfg_t;

/**
 * Phase 1.1 — call / AGI-arg context (still process-global storage).
 * Phase 3: command handlers take agi_session_t *.
 * Phase 3 follow-on: helpers take s; field access via s->call / s->agi / s->res
 * (storage still g_call / g_parms / agi / res).
 */
typedef struct agi_call_ctx {
    char uniqueid[64];
    char callerid[MAX_EXT_LEN];
    char calleridname[MAX_EXT_LEN];
    char channel[64];
    char chanId[64];
    char context[MAX_CLUSTER_LEN];
    char agi_dnid[MAX_EXT_LEN];
    char extension[MAX_EXT_LEN];
    char rdnis[MAX_EXT_LEN];
    char myCluster[MAX_CLUSTER_LEN];
    char myClusterclid[MAX_EXT_LEN];
    char myClusterContext[MAX_CLUSTER_LEN];
    char myClusterId[3];
    int callee_is_local;
    int caller_is_local;
    int rdnis_is_local;
    int rdnis_is_set;
} agi_call_ctx_t;

typedef struct agi_parms {
    char **argv;
    int argc;
    int switchdig;
} agi_parms_t;

/**
 * Phase 3 — one session pointer for command handlers (points at process globals today).
 * Storage remains g_call / g_parms / agi / res; s points at those globals.
 */
typedef struct agi_session {
    agi_call_ctx_t *call;
    agi_parms_t *parms;
    struct _asterisk_tools_ *agi;
    struct _asterisk_cmd_result_ *res;
} agi_session_t;

extern agi_call_ctx_t g_call;
extern agi_parms_t g_parms;

/* pbx3 SQLite (rescols, g_cluster_cfg, sqlQueryBind*, load_cluster_cfg) */
#include "agi_sqlite.h"

int AuthenticatePassword(agi_session_t *s, const char *password_plain);

/** Fill call identity + tenant from AGI vars / dialplan argv; load cluster cfg. */
void agi_init_call_context(agi_session_t *s, int argc, char **argv);

/** Write digits after the leading 4-char feature prefix (*NN*) into out. */
void GetExt(char *out, size_t outsz, const char *number);
void DebugFunctionTrace(const char* thisFunc);
void DebugFunctionMsg(const char* thisFunc, const char* thisMsg);
void setMoh(agi_session_t *s);
/** Write mangled number into out (never returns a stack pointer). */
void Mangle(agi_session_t *s, char *preSel, char *transformList, char *data,
            char *out, size_t outsz);
void RecGreet(agi_session_t *s);
void OutRoute(agi_session_t *s);
void OutTrunk(agi_session_t *s, char *key);
void OutVoip(agi_session_t *s, char *key);
/** Tenant short dial: prefix → target_fqdn via Egress/SBC (fleet). */
void PrefixDial(agi_session_t *s);
int Authenticate(agi_session_t *s, char* password);
int GetRecOption(agi_session_t *s);
void LepDial(agi_session_t *s);
void PostDial(agi_session_t *s);
void PrepDial(agi_session_t *s, char* number, char* type, char* twin, char* vmbox);
char* SetRecord(agi_session_t *s, char* extension, char* compass);
char* CFCheck(agi_session_t *s, char* type, char* number);
void CFToggle(agi_session_t *s);
void CFVMailSet(agi_session_t *s);
void CFVMailToggle(agi_session_t *s);
void FollowMe(agi_session_t *s);
void CFOff(agi_session_t *s);
void SetRingDelay(agi_session_t *s);
char* StripPreselect(char* preSel, char* number);
void AgentLogin(agi_session_t *s);
void AgentLogout(agi_session_t *s);
void AgentPause(agi_session_t *s);
void AgentUnpause(agi_session_t *s);
void ChanSpyWhisper(agi_session_t *s);
void ChanSpy(agi_session_t *s);
void Ingress(agi_session_t *s);
void CheckState(agi_session_t *s, char* remotenum);
char *CheckTime(agi_session_t *s, char *cluster);
void IVR(agi_session_t *s, char *ivrname);
void IVRAction(agi_session_t *s, char* menu, char* press);

char* DBGet(agi_session_t *s, char* family, char* key);
void DBPut(agi_session_t *s, char* family, char* key, char* val);
void DBDel(agi_session_t *s, char* family, char* key);

void sig_handler(int signum);

void OutQmt(agi_session_t *s);
void QLogWrite(char* buffer);
void outboundClip(agi_session_t *s, char *key);
void consoleMsg(char* vmsg, int level);
#endif
