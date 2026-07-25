/*
* function prototypes for PBX3CAGI.c
*
*/
  
#ifndef _PBX3CAGI_H
#define _PBX3CAGI_H

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
#define PARM_NUM myargc
#define PARM_CMD *(myargv+1)
#define PARM_KEY *(myargv+2)
#define PARM_CLST *(myargv+3)
#define PARM_PM1 *(myargv+4)
#define PARM_PM2 *(myargv+5)
#define PARM_PM3 *(myargv+6)
#define SQLITEDB "/opt/pbx3/db/sqlite.rdonly.db"
#define SOUNDIR "/usr/share/asterisk/extra-sounds/"
#define QLOG "/var/log/asterisk/queue_log" 
#define SIPDRIVER "PJSIP"
#define ASTDLIM ","
  

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
    char fqdn[128]; /* tenant SIP domain (cluster.fqdn / cname) for RURI */
} cluster_cfg_t;

/**
 * Phase 1.1 — call / AGI-arg context (still process-global today).
 * Phase 3 will pass pointers into handlers; compatibility macros in pbx3cagi.c
 * keep existing `callerid` / `myargv` names working unchanged.
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

extern agi_call_ctx_t g_call;
extern agi_parms_t g_parms;

/* pbx3 SQLite (rescols, g_cluster_cfg, sqlQueryBind*, load_cluster_cfg) */
#include "agi_sqlite.h"

int AuthenticatePassword(const char *password_plain);

char* GetExt(char* number);
void DebugFunctionTrace(const char* thisFunc);
void DebugFunctionMsg(const char* thisFunc, const char* thisMsg);
void setMoh();
char* Mangle(char* preSel, char* transformList, char* data);
void RecGreet();
void OutRoute();
void OutTrunk(char *key);
void OutVoip(char *key);
int Authenticate(char* password);
int GetRecOption();
void LepDial();
void PostDial();
void PrepDial(char* number, char* type, char* twin, char* vmbox);
char* SetRecord(char* extension, char* compass);
char* CFCheck(char* type, char* number);
void CFToggle();
void CFVMailSet();
void CFVMailToggle();
void FollowMe();
void CFOff();
void SetRingDelay();
char* StripPreselect(char* preSel, char* number);
void AgentLogin();
void AgentLogout();
void AgentPause();
void AgentUnpause();
void ChanSpyWhisper();
void ChanSpy();
void Ingress();
void CheckState(char* remotenum);
char *CheckTime(char *cluster);
void IVR(char *ivrname);
void IVRAction(char* menu, char* press);

char* DBGet(char* family, char* key);
void DBPut(char* family, char* key, char* val);
void DBDel(char* family, char* key);

void sig_handler(int signum);

void OutQmt();
void QLogWrite(char* buffer);
void outboundClip(char *key);
void consoleMsg(char* vmsg, int level);
#endif
