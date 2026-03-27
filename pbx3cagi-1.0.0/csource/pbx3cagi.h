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
#define PARM_PM1 *(myargv+3)
#define PARM_PM2 *(myargv+4)
#define PARM_PM3 *(myargv+5)
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
} cluster_cfg_t;

extern cluster_cfg_t g_cluster_cfg;

int load_cluster_cfg(const char *cluster_pkey, cluster_cfg_t *cfg);
int AuthenticatePassword(const char *password_plain);

//static void CmdFunc(int argc, char *argv[]);
char* GetExt(char* number);
//void* Dtime(char* dsec);
void miscSvcs();
void DebugFunctionTrace(const char* thisFunc);
void DebugFunctionMsg(const char* thisFunc, const char* thisMsg);
void SetCluster();
void setMoh();
void hangUp();
char* Mangle(char* preSel, char* transformList, char* data);
void RecGreet();
void OutRoute();
void OutGroup(char* mykey);
void OutTrunk(char *key);
void OutVoip(char *key);
int Authenticate(char* password);
int GetRecOption();
void LepDial();
void PrepDial(char* number, char* type, char* twin, char* vmbox);
char* SetRecord(char* extension, char* compass);
void Page();
void Alias();
void RingGroup(char* exts, char* num);
void HuntGroup(char* exts, char* num);
char* CFCheck(char* type, char* number);
void CFToggle();
void CFVMailSet();
void CFVMailToggle();
void FollowMe();
void SetCFExtrn();
void CFExtrn(char* type, char* toNum);
void CFOff();
void SetRingDelay();
char* StripPreselect(char* preSel, char* number);
void SetOperator();
void Voicemail();
void EchoTest();
void SetTimer();
void DateTime();
void SysRestart();
void AgentLogin();
void AgentLogout();
void AgentPause();
void AgentUnpause();
void ChanSpyWhisper();
void ChanSpy();
void Ingress();
void CheckState(char* remotenum);
char *CheckTime(char *cluster);
void routeClass(char* gdest, char* rc_char, char* rc_dest, char* cluster);
//void Open(char* remotenum);
void IVR(char *ivrname);
void IVRAction(char* menu, char* press);
//void Closed(char* remotenum);
char* DBGet(char* family, char* key);
void DBPut(char* family, char* key, char* val);
void DBDel(char* family, char* key);
char* GetDBProp(char* key, char* prop);
//void SetDBProp(char* key, char* prop, char* val);
char* GetKeys(char* type);
void DialBack();
char* DBQueryKeys(char* table);
char* sqlQuery(char* myQuery);
void sig_handler(int signum);
//void OutQmt(char* number, char* channel, char* queue, char* agent);
void OutQmt();
void QLogWrite(char* buffer);
void outboundClip(char *key);
void consoleMsg(char* vmsg, int level);
#endif
