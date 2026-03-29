/*
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * COPYRIGHT � 2024 CoCoKraft.com
 * 
 */

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "pbx3cagi.h"
#include "cagi.h"
#include "bsd_compat.h"
#include <sys/wait.h>
#include "sqlite3.h"

AGI_TOOLS agi;
AGI_CMD_RESULT res;

const char *qlogFile = QLOG;
char abstimeout[32] = {'\0'};             // ABSTIMEOUT for any call
char rescols[MAX_SQL_COLS][MAX_SQL_CLEN]; // result columns
char eparm[64] = {'\0'};                  // Used by the Directory function (*57*)
char **myargv;                            // AGI arguments
char vmsg[255] = {'\0'};                  // console mesage buffer
char uniqueid[64] = {'\0'};               // unique call id from Asterisk
char callerid[MAX_EXT_LEN] = {'\0'};      // CLI number from Asterisk
char calleridname[MAX_EXT_LEN] = {'\0'};  // CLI name from Asterisk
char channel[64] = {'\0'};                // AGI channel
char chanId[64] = {'\0'};                 // True SIP endpoint ID of the calling channel
char clidline[MAX_EXT_LEN] = {'\0'};      // CLI from trunk DB entry
char clidphone[MAX_EXT_LEN] = {'\0'};     // CLI from phone DB entry
char clidstrng[MAX_EXT_LEN] = {'\0'};     // CLI build string for sprintf
char context[MAX_CLUSTER_LEN] = {'\0'};   // The curent context
char agi_dnid[MAX_EXT_LEN] = {'\0'};          // agi_dnid from Asterisk
char extension[MAX_EXT_LEN] = {'\0'};     // agi_extension from Asterisk
char rdnis[MAX_EXT_LEN] = {'\0'};         // agi_rdnis from Asterisk
char myCluster[MAX_CLUSTER_LEN] = {'\0'}; // The cluster(Tenant) assigned to this call
char myClusterclid[MAX_EXT_LEN] = {'\0'}; // The cluster(Tenant) CLID
char myClusterContext[MAX_CLUSTER_LEN] = "qrxvtmny";
char myClusterId[3] = {'\0'};        // The cluster(Tenant) ID
char routeclassoverride[8] = {'\0'}; // Holiday scheduler route class override
char routeoverride[32] = {'\0'};     // Holiday scheduler route override
char routeclassopen[8] = {'\0'};     // open routeclass
char routeclassclosed[8] = {'\0'};   // closed routeclass
char openroute[32] = {'\0'};         // open route
char closeroute[32] = {'\0'};        // closed route
char myQuery[255] = {'\0'};          // regular SQL query
char setcdrcmd[64] = "CHANNEL(accountcode)=";


long debug = FALSE; // debug on/off
int abstimeint = 14400; // default (4 hours)
int myargc;             // numargs
int switchdig;          // function number
int callee_is_local = FALSE;
int caller_is_local = FALSE;
int rdnis_is_local = FALSE;
int rdnis_is_set = FALSE;

/**
 * only used five times for two variants CFIM CFBS - get rid) - set in the cases
*/
char *cfTab[50] =
    {
        "", "", "", "", "", "", "", "", "", "", "",
        "", "", "", "", "", "", "", "cfim", "cfim", "cfim",
        "cfim", "cfbs", "", "", "", "", "cfim", "cfimopen", "cfbsopen", "",
        "", "", "", "", "", "", "", "cfimclosed", "cfbsclosed", "",
        "cfim", "cfim", "", "", "", "", "", "", ""};

void sig_handler(int signum)
{

    DebugFunctionTrace(__FUNCTION__);

    exit(signum);
}

void DebugFunctionTrace(const char *thisFunc)
{

    char debugMsg[MAX_MSG_LEN] = {'\0'};
    ;

    if (debug)
    {
        sprintf(debugMsg, "Trace Entered  %s", thisFunc);
        AGITool_verbose(&agi, &res, debugMsg, 1);
    }
    return;
}

void DebugFunctionMsg(const char *thisFunc, const char *thisMsg)
{

    char debugMsg[MAX_MSG_LEN] = {'\0'};

    sprintf(debugMsg, "(Function %s) %s ", thisFunc, thisMsg);
    AGITool_verbose(&agi, &res, debugMsg, 1);

    return;
}

cluster_cfg_t g_cluster_cfg;
/* Prepared SELECT helpers: sql must contain exactly one ? (bind1) or two ? in order (bind2). */
static char *sqlQueryBind1(const char *sql, const char *arg1);
static char *sqlQueryBind2(const char *sql, const char *arg1, const char *arg2);

static void cluster_cfg_apply_defaults(cluster_cfg_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->abstimeout_sec = 14400;
    strlcpy(cfg->voipmax_str, "30", sizeof(cfg->voipmax_str));
    strlcpy(cfg->callrecord_1, "None", sizeof(cfg->callrecord_1));
    strlcpy(cfg->allowhashxfer, "enabled", sizeof(cfg->allowhashxfer));
    strlcpy(cfg->playbeep, "YES", sizeof(cfg->playbeep));
    strlcpy(cfg->playbusy, "YES", sizeof(cfg->playbusy));
    strlcpy(cfg->playcongested, "YES", sizeof(cfg->playcongested));
    strlcpy(cfg->playtransfer, "YES", sizeof(cfg->playtransfer));
    strlcpy(cfg->voiceinstr, "YES", sizeof(cfg->voiceinstr));
    strlcpy(cfg->int_ring_delay, "20", sizeof(cfg->int_ring_delay));
    strlcpy(cfg->maxin_str, "30", sizeof(cfg->maxin_str));
    strlcpy(cfg->ringdelay_str, "20", sizeof(cfg->ringdelay_str));
    strlcpy(cfg->lterm_str, "NO", sizeof(cfg->lterm_str));
    strlcpy(cfg->cfwd_progress, "enabled", sizeof(cfg->cfwd_progress));
    strlcpy(cfg->cfwd_answer, "enabled", sizeof(cfg->cfwd_answer));
    strlcpy(cfg->ivr_key_wait, "6", sizeof(cfg->ivr_key_wait));
    strlcpy(cfg->ivr_digit_wait_str, "6000", sizeof(cfg->ivr_digit_wait_str));
    strlcpy(cfg->syspass, "4444", sizeof(cfg->syspass));
    strlcpy(cfg->spy_pass, "3333", sizeof(cfg->spy_pass));
    strlcpy(cfg->chanmax_str, "3", sizeof(cfg->chanmax_str));
    strlcpy(cfg->usemohcustom, "NO", sizeof(cfg->usemohcustom));
    strlcpy(cfg->masteroclo, "AUTO", sizeof(cfg->masteroclo));
    cfg->oclo[0] = '\0';
    cfg->routeoverride[0] = '\0';
}

static sqlite3 *g_sqlite_handle = NULL;

static void sqlCloseSharedHandle(void)
{
    if (g_sqlite_handle != NULL)
    {
        sqlite3_close(g_sqlite_handle);
        g_sqlite_handle = NULL;
    }
}

static sqlite3 *sqlGetSharedHandle(void)
{
    int retval;

    if (g_sqlite_handle != NULL)
    {
        return g_sqlite_handle;
    }

    retval = sqlite3_open(SQLITEDB, &g_sqlite_handle);
    if (retval)
    {
        if (g_sqlite_handle != NULL)
        {
            sqlite3_close(g_sqlite_handle);
            g_sqlite_handle = NULL;
        }
        snprintf(vmsg, sizeof(vmsg), "Database connection failed, retval is %i", retval);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return NULL;
    }

    atexit(sqlCloseSharedHandle);
    return g_sqlite_handle;
}

static void escape_sql_literal(char *out, size_t outlen, const char *in)
{
    size_t o = 0;
    size_t i;

    for (i = 0; in[i] != '\0' && o + 1 < outlen; i++)
    {
        if (in[i] == '\'')
        {
            if (o + 2 >= outlen)
            {
                break;
            }
            out[o++] = '\'';
            out[o++] = '\'';
        }
        else
        {
            out[o++] = in[i];
        }
    }
    out[o] = '\0';
}

int load_cluster_cfg(const char *cluster_pkey, cluster_cfg_t *cfg)
{
    char esc[128];
    char query[1400];
    sqlite3 *handle = NULL;
    sqlite3_stmt *stmt = NULL;
    int retval;
    int i;

    cluster_cfg_apply_defaults(cfg);

    if (cluster_pkey == NULL || cluster_pkey[0] == '\0')
    {
        DebugFunctionMsg(__FUNCTION__, "load_cluster_cfg: empty cluster pkey");
        return -1;
    }

    escape_sql_literal(esc, sizeof(esc), cluster_pkey);
    snprintf(
        query, sizeof(query),
        "SELECT abstimeout, voip_max, allow_hash_xfer, play_beep, play_busy, play_congested, "
        "play_transfer, voice_instr, bounce_alert, blind_busy, int_ring_delay, maxin, ringdelay, lterm, "
        "cfwd_progress, cfwd_answer, ivr_key_wait, ivr_digit_wait, syspass, spy_pass, dynamicfeatures, "
        "clusterclid, chanmax, usemohcustom, callrecord_1, masteroclo, oclo, routeoverride "
        "FROM cluster WHERE pkey='%s' OR shortuid='%s'",
        esc, esc);

    handle = sqlGetSharedHandle();
    if (handle == NULL)
    {
        DebugFunctionMsg(__FUNCTION__, "load_cluster_cfg: sqlite open failed");
        return -1;
    }

    for (i = 0; i < 3; i++)
    {
        retval = sqlite3_prepare_v2(handle, query, -1, &stmt, NULL);
        if (retval == SQLITE_OK)
        {
            break;
        }
        if (retval == SQLITE_LOCKED || retval == SQLITE_BUSY)
        {
            AGITool_exec(&agi, &res, "Wait", "0.5");
        }
        else
        {
            break;
        }
    }

    if (retval != SQLITE_OK)
    {
        snprintf(vmsg, sizeof(vmsg), "load_cluster_cfg: prepare failed %i", retval);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return -1;
    }

    retval = sqlite3_step(stmt);
    if (retval != SQLITE_ROW)
    {
        snprintf(vmsg, sizeof(vmsg), "load_cluster_cfg: no cluster row for pkey=%s", cluster_pkey);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        sqlite3_finalize(stmt);
        return -1;
    }

    cfg->abstimeout_sec = sqlite3_column_type(stmt, 0) == SQLITE_NULL ? 14400 : sqlite3_column_int(stmt, 0);
    {
        int vm = sqlite3_column_type(stmt, 1) == SQLITE_NULL ? 30 : sqlite3_column_int(stmt, 1);
        snprintf(cfg->voipmax_str, sizeof(cfg->voipmax_str), "%d", vm);
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 2);
        strlcpy(cfg->allowhashxfer, t ? (const char *)t : "enabled", sizeof(cfg->allowhashxfer));
    }
    {
        int v = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? 1 : sqlite3_column_int(stmt, 3);
        strlcpy(cfg->playbeep, v ? "YES" : "NO", sizeof(cfg->playbeep));
    }
    {
        int v = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? 1 : sqlite3_column_int(stmt, 4);
        strlcpy(cfg->playbusy, v ? "YES" : "NO", sizeof(cfg->playbusy));
    }
    {
        int v = sqlite3_column_type(stmt, 5) == SQLITE_NULL ? 1 : sqlite3_column_int(stmt, 5);
        strlcpy(cfg->playcongested, v ? "YES" : "NO", sizeof(cfg->playcongested));
    }
    {
        int v = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? 1 : sqlite3_column_int(stmt, 6);
        strlcpy(cfg->playtransfer, v ? "YES" : "NO", sizeof(cfg->playtransfer));
    }
    {
        int vi = sqlite3_column_type(stmt, 7) == SQLITE_NULL ? 1 : sqlite3_column_int(stmt, 7);
        strlcpy(cfg->voiceinstr, (vi == 0) ? "NO" : "YES", sizeof(cfg->voiceinstr));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 8);
        strlcpy(cfg->bounce_alert, t ? (const char *)t : "", sizeof(cfg->bounce_alert));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 9);
        strlcpy(cfg->blind_busy, t ? (const char *)t : "", sizeof(cfg->blind_busy));
    }
    {
        int v = sqlite3_column_type(stmt, 10) == SQLITE_NULL ? 20 : sqlite3_column_int(stmt, 10);
        snprintf(cfg->int_ring_delay, sizeof(cfg->int_ring_delay), "%d", v);
    }
    {
        int v = sqlite3_column_type(stmt, 11) == SQLITE_NULL ? 30 : sqlite3_column_int(stmt, 11);
        snprintf(cfg->maxin_str, sizeof(cfg->maxin_str), "%d", v);
    }
    {
        int v = sqlite3_column_type(stmt, 12) == SQLITE_NULL ? 20 : sqlite3_column_int(stmt, 12);
        snprintf(cfg->ringdelay_str, sizeof(cfg->ringdelay_str), "%d", v);
    }
    {
        int lt = sqlite3_column_type(stmt, 13) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 13);
        strlcpy(cfg->lterm_str, lt ? "YES" : "NO", sizeof(cfg->lterm_str));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 14);
        strlcpy(cfg->cfwd_progress, t ? (const char *)t : "enabled", sizeof(cfg->cfwd_progress));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 15);
        strlcpy(cfg->cfwd_answer, t ? (const char *)t : "enabled", sizeof(cfg->cfwd_answer));
    }
    {
        int v = sqlite3_column_type(stmt, 16) == SQLITE_NULL ? 6 : sqlite3_column_int(stmt, 16);
        snprintf(cfg->ivr_key_wait, sizeof(cfg->ivr_key_wait), "%d", v);
    }
    {
        int v = sqlite3_column_type(stmt, 17) == SQLITE_NULL ? 6000 : sqlite3_column_int(stmt, 17);
        snprintf(cfg->ivr_digit_wait_str, sizeof(cfg->ivr_digit_wait_str), "%d", v);
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 18);
        strlcpy(cfg->syspass, t ? (const char *)t : "4444", sizeof(cfg->syspass));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 19);
        strlcpy(cfg->spy_pass, t ? (const char *)t : "3333", sizeof(cfg->spy_pass));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 20);
        strlcpy(cfg->dynamicfeatures, t ? (const char *)t : "", sizeof(cfg->dynamicfeatures));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 21);
        strlcpy(cfg->clusterclid, t ? (const char *)t : "", sizeof(cfg->clusterclid));
    }
    {
        int v = sqlite3_column_type(stmt, 22) == SQLITE_NULL ? 3 : sqlite3_column_int(stmt, 22);
        snprintf(cfg->chanmax_str, sizeof(cfg->chanmax_str), "%d", v);
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 23);
        strlcpy(cfg->usemohcustom, t ? (const char *)t : "NO", sizeof(cfg->usemohcustom));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 24);
        strlcpy(cfg->callrecord_1, t ? (const char *)t : "None", sizeof(cfg->callrecord_1));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 25);
        strlcpy(cfg->masteroclo, t ? (const char *)t : "AUTO", sizeof(cfg->masteroclo));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 26);
        strlcpy(cfg->oclo, t ? (const char *)t : "", sizeof(cfg->oclo));
    }
    {
        const unsigned char *t = sqlite3_column_text(stmt, 27);
        strlcpy(cfg->routeoverride, t ? (const char *)t : "", sizeof(cfg->routeoverride));
    }

    cfg->loaded = 1;
    sqlite3_finalize(stmt);
    return 0;
}

int main(int argc, char **argv)
{

    char numwork[32] = {'\0'};
    char chardig[4] = {'\0'};

    char *cmdTab[11] =
        {"OutTrunk", "OutRoute", "LepDial", "Ingress", "Dial", "IVR", "OutQmt"};
        
    int i = 0;

    myargv = argv; // agi srgs
    myargc = argc; // agi arg count

    AGITool_Init(&agi);

    AGITool_get_variable(&agi, &res, "DEBUG"); // DEBUG is an Asterisk variable set from the console
    if (!strcmp(res.data, "ON"))
    {
        debug = TRUE;
    }

    DebugFunctionTrace(__FUNCTION__);

    strlcpy(uniqueid, AGITool_ListGetVal(agi.agi_vars, "agi_uniqueid"), sizeof(uniqueid));
    strlcpy(callerid, AGITool_ListGetVal(agi.agi_vars, "agi_callerid"), sizeof(callerid));
    strlcpy(calleridname, AGITool_ListGetVal(agi.agi_vars, "agi_calleridname"), sizeof(calleridname));
    strlcpy(context, AGITool_ListGetVal(agi.agi_vars, "agi_context"), sizeof(context));
    strlcpy(extension, AGITool_ListGetVal(agi.agi_vars, "agi_extension"), sizeof(extension));
    strlcpy(rdnis, AGITool_ListGetVal(agi.agi_vars, "agi_rdnis"), sizeof(rdnis));
    strlcpy(channel, AGITool_ListGetVal(agi.agi_vars, "agi_channel"), sizeof(channel));
    strlcpy(agi_dnid, AGITool_ListGetVal(agi.agi_vars, "agi_dnid"), sizeof(agi_dnid));

	
    if (isdigit(callerid[0])) {
        if (strlen(callerid) < 6) {
            caller_is_local = TRUE;
        }
        else {
            if (strlen(callerid) == 8) {
                caller_is_local = TRUE;
            }
        }
    }

    if (isdigit(extension[0])) {
        if (strlen(extension) < 6) {
            callee_is_local = TRUE;
        }
        else {
            if (strlen(extension) == 8) {
                callee_is_local = TRUE;
            }
        }
    }

	
	if (strcmp(rdnis, "unknown")) { 
		if (strlen(rdnis) < 6) {
			rdnis_is_local = TRUE;
		}
		rdnis_is_set = TRUE;
	}

    if (argc > 3 && myargv[3] != NULL && PARM_CLST[0] != '\0') {
        strlcpy(myCluster, PARM_CLST, sizeof(myCluster));
    } else {
        strlcpy(myCluster, context, sizeof(myCluster));
    }

    strlcpy(myClusterContext, myCluster, sizeof(myClusterContext));
    if (!strcmp(myCluster, "default")) {
        strlcpy(myClusterContext, "qrxvtmny", sizeof(myClusterContext));
    }

    strlcat(setcdrcmd, myCluster, sizeof(setcdrcmd));

    AGITool_exec(&agi, &res, "Set", setcdrcmd);

    /* Before argc==1 / argc<2 exits: only read argv slots that exist. */
    {
        const char *log_cmd = "";
        const char *log_clst = "";
        if (argc > 1 && myargv[1] != NULL) {
            log_cmd = PARM_CMD;
        }
        if (argc > 3 && myargv[3] != NULL) {
            log_clst = PARM_CLST;
        }
        snprintf(vmsg, sizeof(vmsg),
                 "Phase Main effective_cluster=%s agi_context=%s argv PARM_CMD=%s PARM_CLST=%s",
                 myCluster, context, log_cmd, log_clst);
    }
    DebugFunctionMsg(__FUNCTION__, vmsg);

    load_cluster_cfg(myCluster, &g_cluster_cfg);
    abstimeint = g_cluster_cfg.abstimeout_sec;

    // No parameters means the special Queues "Local" backcall
    if (argc == 1)
    {
        SetRecord("Qexec", "Inbound");




        return 0;
    }
    // check whether a function was specified
    if (argc < 2)
    {
        DebugFunctionMsg(__FUNCTION__, "Performed an AGI call without specifying a function.");
        return 1;
    }
    // if this is a feature code dial...
    // Digitize the Feature codes, answer the call and Insert a wait to allow the line to settle.
    if (!strncmp(argv[1], "*", 1))
    {
        AGITool_answer(&agi, &res);
        AGITool_exec(&agi, &res, "Wait", "0.5");
        strlcpy(numwork, *(argv + 1), sizeof(numwork));
        strlcpy(chardig, numwork + 1, sizeof(chardig));
        switchdig = atoi(chardig);
    }
    // Digitise the cmd sequences (there are only 11 - see the cmdTab)
    else
    {
        while (i < 11)
        {
            if (!strcmp(*(cmdTab + i), *(argv + 1)))
            {
                switchdig = i + 1;
                break;
            }
            i++;
        }
    }

    if (debug)
    {
        snprintf(vmsg, sizeof(vmsg), "switchdig is %i", switchdig);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        snprintf(vmsg, sizeof(vmsg), "PARM_CMD is %s", PARM_CMD);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        if (argc > 2 && myargv[2] != NULL) {
            snprintf(vmsg, sizeof(vmsg), "PARM_KEY is %s", PARM_KEY);
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }
        if (argc > 3 && myargv[3] != NULL) {
            snprintf(vmsg, sizeof(vmsg), "PARM_CLST is %s", PARM_CLST);
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }
        if (argc > 4 && myargv[4] != NULL) {
            snprintf(vmsg, sizeof(vmsg), "PARM_PM1 is %s", PARM_PM1);
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }
        if (argc > 5 && myargv[5] != NULL) {
            snprintf(vmsg, sizeof(vmsg), "PARM_PM2 is %s", PARM_PM2);
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }
        if (argc > 6 && myargv[6] != NULL) {
            snprintf(vmsg, sizeof(vmsg), "PARM_PM3 is %s", PARM_PM3);
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }
        snprintf(vmsg, sizeof(vmsg), "Cluster is %s", myCluster);
        DebugFunctionMsg(__FUNCTION__, vmsg);
//        snprintf(vmsg, sizeof(vmsg), "ClusterId is %s", myClusterId);
//        DebugFunctionMsg(__FUNCTION__, vmsg);
    }

/*
 *	set MOH
 */
	setMoh();

/*
 *   the input command now has an integer number assigned to it (switchdig)
 *   so we can just use a switch to  select the command processor
 */

    switch (switchdig)
    {

    case 1:
        OutTrunk(PARM_KEY);
        break;
    case 2:
        OutRoute();     // routed dial to a peer
        break;
    case 3:
        LepDial();      // local endpoint (extension) dial
        break;
    case 4:
        Ingress();      // inbound call from peer
        break;
    case 5:
        PrepDial(PARM_KEY,PARM_PM1,"","");   // direct dial
        break;
    case 6:
        IVR(PARM_KEY);          // IVR menus
        break;
    case 7:
        OutQmt();       // Queuemetrics outbound stuff 
        break;


    case 18:
        CFVMailSet();
        break;
    case 19:
        CFVMailSet();
        break;
    case 20:
        CFVMailToggle();
        break;
    case 21:
        CFToggle();
        break;
    case 22:
        CFToggle();
        break;
    case 23:
        CFOff();
        break;
    case 26:
        SetRingDelay();
        break;
    case 27:
        FollowMe();
        break;
    case 60:
        RecGreet();
        break;
    case 63:
        AgentPause();
        break;
    case 64:
        AgentUnpause();
        break;
    case 65:
        AgentLogin();
        break;
    case 66:
        AgentLogout();
        break;
// These need work for MultiTenant
    case 67:
        ChanSpyWhisper();
        break;
    case 68:
        ChanSpy();
        break;
    default:
        sprintf(vmsg, "Function-call %i does not exist in the core", switchdig);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        break;
    }
    AGITool_Destroy(&agi);
    return 0;
}

/***********************************************************************
  END OF MAINLINE
************************************************************************/

void setMoh()
{

    DebugFunctionTrace(__FUNCTION__);

    char setmohcmd[64] = "CHANNEL(musicclass)=moh-";
    char mohfolder[64] = "/usr/share/asterisk/moh-";
    char cmd[1024];
    int status, exitcode;

    strlcat(mohfolder, myCluster, sizeof(mohfolder));

    if (strcmp(g_cluster_cfg.usemohcustom, "YES"))
    {

        snprintf(vmsg, sizeof(vmsg), "MOH folder %s is disabled usemohcustom=NO, MOH not set", mohfolder);
        DebugFunctionMsg(__FUNCTION__, vmsg);

        return;
    }

    // check to see if the target folder has any files in it
    snprintf(cmd, 1024, "test $(ls -A \"%s\" 2>/dev/null | wc -l) -ne 0", mohfolder);
    status = system(cmd);
    exitcode = WEXITSTATUS(status);
    if (exitcode == 1)
    {

        snprintf(vmsg, sizeof(vmsg), "MOH folder %s is empty MOH not set", mohfolder);
        DebugFunctionTrace(__FUNCTION__);
        return;
    }

    // set the target moh class

    snprintf(vmsg, sizeof(vmsg), "Setting MOH folder %s as music class", mohfolder);
    DebugFunctionTrace(__FUNCTION__);

    strlcat(setmohcmd, myCluster, sizeof(setmohcmd));
    AGITool_exec(&agi, &res, "Set", setmohcmd);
    return;
}

void hangUp()
{

    DebugFunctionTrace(__FUNCTION__);

    AGITool_exec(&agi, &res, "Hangup", "");
    return;
}

char *GetExt(char *number)
{

    DebugFunctionTrace(__FUNCTION__);

    snprintf(vmsg, sizeof(vmsg), "Trace Entered GetExt with %s", number);
    DebugFunctionMsg(__FUNCTION__, vmsg);

    char ext[MAX_EXT_LEN] = {'\0'};
    char *pExt = &ext[0];
    char tmp[MAX_EXT_LEN] = {'\0'};
    int i;

    strlcpy(tmp, number, sizeof(tmp));
    for (i = 0; i < (int)(strlen(tmp) - 4); i++)
    {
        ext[i] = tmp[i + 4];
    }
    snprintf(vmsg, sizeof(vmsg), "Trace returned from GetExt with %s", ext);
    DebugFunctionMsg(__FUNCTION__, vmsg);
    return pExt;
}

char *Mangle(char *preSel, char *transformList, char *data)
{

    DebugFunctionTrace(__FUNCTION__);

    char *transform, *left, *right;
    char transformArr[MAX_NUM_TRANSFORM][MAX_TRANSFORM_LEN];
    char operand[MAX_NUM_LEN] = {'\0'};
    char *pOperand = &operand[0];
    int i = 0, j = 0, k = 0, len = 0;

    if (!strcmp(data, "CLI"))
    {
        strlcpy(operand, callerid, sizeof(operand));
    }
    else
    {
        strlcpy(operand, extension, sizeof(operand));
    }
    if (preSel)
    {
        strlcpy(operand, StripPreselect(preSel, operand), sizeof(operand));
    }

    // extract each transform from the list
    transform = strtok(transformList, " ");
    while (transform)
    {
        strlcpy(transformArr[i], transform, sizeof(transformArr[i]));
        transform = strtok(NULL, " ");
        i++;
    }
    for (j = 0; j < i; j++)
    {
        if (transformArr[j][0] == ':')
        {
            left = "";
            right = strtok(transformArr[j], ":");
        }
        else
        {
            left = strtok(transformArr[j], ":");
            right = strtok(NULL, "\0");
        }
        // check if the left value of the transform matches the left side of
        // the operand
        if (!strncmp(operand, left, strlen(left)))
        {
            // if the transform matches shift the operand to the left to
            // remove transform and then cat to right transform
            len = strlen(left);
            for (k = 0; k < (int)strlen(operand); k++)
            {
                operand[k] = operand[k + len];
            }
            if (right)
            {
                strlcat(right, operand, sizeof(right));
                strlcpy(operand, right, sizeof(operand));
            }
        }
    }

    return pOperand;
}

void RecGreet()
{

    DebugFunctionTrace(__FUNCTION__);

    int press = '2';
    char newGreetFile[MAX_FILENAME_LEN] = {'\0'};
    char tmpGreetFile[MAX_FILENAME_LEN] = {'\0'};
    char ext[MAX_EXT_LEN] = {'\0'};

    srand((unsigned int)time(NULL));
    int r = rand();

    snprintf(tmpGreetFile, sizeof(tmpGreetFile), "%s%s/ug%i", SOUNDIR, myCluster, r);

    strlcpy(ext, GetExt(PARM_CMD), sizeof(ext));
    snprintf(newGreetFile, sizeof(newGreetFile), "%s%s/usergreeting%s.wav", SOUNDIR, myCluster, ext);

    // check password
    if (Authenticate("SYSPASS") != 0) //** syspass is held in the tenant table (used to be in Globals)
    {
        return;
    }
    // message to record
    AGITool_exec(&agi, &res, "Playback", "pm-announcement-number");
    AGITool_exec(&agi, &res, "SayDigits", ext);
    AGITool_exec(&agi, &res, "Playback", "is-now-being-recorded");
    AGITool_exec(&agi, &res, "Playback", "silence/1");
    AGITool_exec(&agi, &res, "Playback", "press-pound-save-changes");

    // record message
    while (press == '2')
    {
        AGITool_record_file(&agi, &res, tmpGreetFile, "wav", "#", 120000, 10, 10, 0);
        AGITool_stream_file(&agi, &res, tmpGreetFile, "", 0);
        press = GetRecOption();
    }
    strlcat(tmpGreetFile, ".wav", sizeof(tmpGreetFile));
    // save message
    if (press == '1')
    {
        snprintf(vmsg, sizeof(vmsg), "tmpfilename is %s", tmpGreetFile);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        snprintf(vmsg, sizeof(vmsg), "newfilename is %s", newGreetFile);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        rename(tmpGreetFile, newGreetFile);
        AGITool_exec(&agi, &res, "Playback", "your-msg-has-been-saved");
        AGITool_exec(&agi, &res, "Playback", "goodbye");
        return;
    }

    remove(tmpGreetFile);
    AGITool_exec(&agi, &res, "Playback", "cancelled");
}


int AuthenticatePassword(const char *password_plain)
{

    DebugFunctionTrace(__FUNCTION__);
    char authbuf[64] = {'\0'};

    if (password_plain == NULL || password_plain[0] == '\0')
    {
        snprintf(vmsg, sizeof(vmsg), "Unable to find password in the database.");
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return -1;
    }

    strlcpy(authbuf, password_plain, sizeof(authbuf));
    AGITool_exec(&agi, &res, "Playback", "silence/1");
    AGITool_exec(&agi, &res, "Authenticate", authbuf);
    return atoi(res.result);
}

int Authenticate(char *password)
{

    DebugFunctionTrace(__FUNCTION__);

    if (!strcmp(password, "SYSPASS"))
    {
        return AuthenticatePassword(g_cluster_cfg.syspass);
    }
    if (!strcmp(password, "SPYPASS"))
    {
        return AuthenticatePassword(g_cluster_cfg.spy_pass);
    }

    snprintf(vmsg, sizeof(vmsg), "Authenticate: unknown password key %s", password);
    DebugFunctionMsg(__FUNCTION__, vmsg);
    return -1;
}

int GetRecOption()
{

    DebugFunctionTrace(__FUNCTION__);

    int count = 1;

    while (count < 3)
    {
        // When message has been recorded give options
        AGITool_stream_file(&agi, &res, "save-announce-press", "123", 0);
        if (atoi(res.result))
            return atoi(res.result);
        AGITool_stream_file(&agi, &res, "digits/1", "123", 0);
        if (atoi(res.result))
            return atoi(res.result);
        AGITool_stream_file(&agi, &res, "to-rerecord-announce", "123", 0);
        if (atoi(res.result))
            return atoi(res.result);
        AGITool_stream_file(&agi, &res, "digits/2", "123", 0);
        if (atoi(res.result))
            return atoi(res.result);
        AGITool_stream_file(&agi, &res, "to-cancel-this-msg", "123", 0);
        if (atoi(res.result))
            return atoi(res.result);
        AGITool_stream_file(&agi, &res, "press", "123", 0);
        if (atoi(res.result))
            return atoi(res.result);
        AGITool_stream_file(&agi, &res, "digits/3", "123", 0);
        if (atoi(res.result))
            return atoi(res.result);
        AGITool_stream_file(&agi, &res, "silence/5", "123", 0);
        if (atoi(res.result))
            return atoi(res.result);
        count++;
    }

    return '3';
}

void AgentLogin()
{

    DebugFunctionTrace(__FUNCTION__);

    int finished = FALSE;                  // while control
    int i;                                 // control index
    char agent[8] = {'\0'};                // dtmf agent number
    char oldagent[8] = {'\0'};             // a dangling agent left logged in
    char agentpasswd[8] = {'\0'};          // dtmf agent passwd
    char queuearg[256] = {'\0'};           // argument for the AddQueueMember/RemoveQueuMember
    char agentqueue[32] = {'\0'};          // used to construct the queue column
    char queuename[32] = {'\0'};           // queuename
    char extenAgent[MAX_EXT_LEN] = {'\0'}; // holds exten if an agent is already logged in
    char agentchan[64] = {'\0'};           // full channel name - i.e. local/401@context
    char statechan[64] = {'\0'};           // state channel - i.e. PJSIP/401@context
    char buffer[1024] = {'\0'};            // QLOG buffer
    char epoch[32] = {'\0'};               // ${EPOCH}
    char startepoch[32] = {'\0'};          // ${EPOCH} saved from a previous login
    char f_eAgent[64] = {'\0'};
    char f_dAgent[64] = {'\0'};
    char f_dynLogin[64] = {'\0'};

    int timediff;

    snprintf(f_eAgent, sizeof(f_eAgent), "%s/eAgent", myClusterContext);
    snprintf(f_dAgent, sizeof(f_dAgent), "%s/dAgent", myClusterContext);
    snprintf(f_dynLogin, sizeof(f_dynLogin), "%s/DYNLOGIN", myClusterContext);

    strlcpy(agentchan, "Local/", sizeof(agentchan));
    strlcat(agentchan, callerid, sizeof(agentchan));
    strlcat(agentchan, "@", sizeof(agentchan));
    strlcat(agentchan, myClusterContext, sizeof(agentchan));
    strlcpy(statechan, "Local/", sizeof(statechan));
    strlcat(statechan, callerid, sizeof(statechan));

    AGITool_get_data(&agi, &res, "agent-user", 7000, 5);

    while (!finished)
    {
        if (atoi(res.result))
        {
            // check if it's a valid agent
            snprintf(agent, sizeof(agent), "%s", res.result);
            if (strcmp(sqlQueryBind1("SELECT pkey FROM agent WHERE pkey=?", agent), ""))
            {
                strlcpy(agentpasswd, sqlQueryBind1("SELECT passwd FROM agent WHERE pkey=?", agent), sizeof(agentpasswd));
                AGITool_exec(&agi, &res, "Authenticate", agentpasswd);
                if (atoi(res.result) == 0)
                {
                    strlcpy(oldagent, DBGet(f_eAgent, callerid), sizeof(oldagent));
                    strlcpy(extenAgent, DBGet(f_dAgent, agent), sizeof(extenAgent));
                    if (!strcmp(oldagent, ""))
                    {
                        strlcpy(extenAgent, DBGet(f_dAgent, agent), sizeof(extenAgent));
                        strlcpy(oldagent, DBGet(f_eAgent, extenAgent), sizeof(oldagent));
                    }
                    if (strcmp(oldagent, ""))
                    {
                        strlcpy(startepoch, DBGet(f_dynLogin, oldagent), sizeof(startepoch));
                        AGITool_get_variable(&agi, &res, "EPOCH"); //Asterisk system variable EPOCH
                        strlcpy(epoch, res.data, sizeof(epoch));
                        for (i = 1; i < 7; i++)
                        {
                            snprintf(agentqueue, sizeof(agentqueue), "queue%i", i);
                            snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM agent WHERE pkey=?", agentqueue);
                            strlcpy(queuename, sqlQueryBind1(myQuery, oldagent), sizeof(queuename));
                            if (strcmp(queuename, "None"))
                            {
                                //								snprintf (queuearg, sizeof(queuearg), "%s%sLocal/%s@queues",queuename,ASTDLIM,extenAgent);
                                snprintf(queuearg, sizeof(queuearg), "%s,%s", queuename, agentchan);
                                AGITool_exec(&agi, &res, "RemoveQueueMember", queuearg);
                            }
                        }
                        if (strcmp(DBGet(f_dynLogin, oldagent), ""))
                        {
                            timediff = atoi(epoch) - atoi(startepoch);
                        }
                        snprintf(buffer, sizeof(buffer), "%s|%s|NONE|Agent/%s|AGENTLOGOFF|-|%i", epoch, uniqueid, oldagent, timediff);
                        QLogWrite(buffer);
                        DBDel(f_dynLogin, oldagent);
                        DBDel(f_dAgent, oldagent);
                        DBDel(f_eAgent, extenAgent);
                        AGITool_exec(&agi, &res, "Wait", "1");
                    }
                    for (i = 1; i < 7; i++)
                    {
                        snprintf(agentqueue, sizeof(agentqueue), "queue%i", i);
                        snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM agent WHERE pkey=?", agentqueue);
                        strlcpy(queuename, sqlQueryBind1(myQuery, agent), sizeof(queuename));
                        if (strcmp(queuename, "None"))
                        {
                            snprintf(queuearg, sizeof(queuearg), "%s,%s,,,Agent/%s",
                                     queuename, agentchan, agent);
                            AGITool_exec(&agi, &res, "AddQueueMember", queuearg);
                        }
                    }
                    AGITool_get_variable(&agi, &res, "EPOCH"); //Asterisk system variable EPOCH
                    strlcpy(epoch, res.data, sizeof(epoch));
                    snprintf(buffer, sizeof(buffer), "%s|%s|NONE|Agent/%s|AGENTLOGIN|%s", epoch, uniqueid, agent, agentchan);
                    QLogWrite(buffer);
                    if (!strcmp(DBGet("DYNLOGIN", agent), ""))
                    {
                        DBPut(f_dynLogin, agent, epoch);
                    }
                    DBPut(f_dAgent, agent, callerid);
                    DBPut(f_eAgent, callerid, agent);
                    AGITool_exec(&agi, &res, "Playback", "agent-loginok");
                    finished = TRUE;
                    continue;
                }
            }
        }
        AGITool_get_data(&agi, &res, "agent-incorrect", 7000, 5);
    }
}
void AgentLogout()
{

    DebugFunctionTrace(__FUNCTION__);

    int i;                        // control index
    char agent[8] = {'\0'};       // agent number
    char agentpasswd[8] = {'\0'}; // dtmf agent passwd
    char queuearg[256] = {'\0'};  // argument for the AddQueueMember/RemoveQueuMember
    char agentqueue[32] = {'\0'}; // used to construct the queue column
    char queuename[32] = {'\0'};  // queuename
    char agentchan[64] = {'\0'};  // full channel name - i.e. local/401@extensions
    char statechan[64] = {'\0'};  // state channel - i.e. PJSIP/401@extensions
    char buffer[1024] = {'\0'};   // QLOG buffer
    char epoch[32] = {'\0'};      // ${EPOCH}
    char startepoch[32] = {'\0'}; // ${EPOCH} saved from login
    char f_eAgent[64] = {'\0'};
    char f_dAgent[64] = {'\0'};
    char f_dynLogin[64] = {'\0'};
    int timediff;

    snprintf(f_eAgent, sizeof(f_eAgent), "%s/eAgent", myClusterContext);
    snprintf(f_dAgent, sizeof(f_dAgent), "%s/dAgent", myClusterContext);
    snprintf(f_dynLogin, sizeof(f_dynLogin), "%s/DYNLOGIN", myClusterContext);

    strlcpy(agentchan, "Local/", sizeof(agentchan));
    strlcat(agentchan, callerid, sizeof(agentchan));
    strlcat(agentchan, "@", sizeof(agentchan));
    strlcat(agentchan, myClusterContext, sizeof(agentchan));
    strlcpy(statechan, "Local/", sizeof(statechan));
    strlcat(statechan, callerid, sizeof(statechan));

    strcpy(agent, DBGet(f_eAgent, callerid));
    strlcpy(agentpasswd, sqlQueryBind1("SELECT passwd FROM agent WHERE pkey=?", agent), sizeof(agentpasswd));

    // check if they are logged in - if not just exit

    if (!strcmp(agent, ""))
    {
        AGITool_exec(&agi, &res, "Playback", "agent-loggedoff");
        return;
    }

    AGITool_get_variable(&agi, &res, "EPOCH"); //Asterisk system variable EPOCH
    strlcpy(epoch, res.data, sizeof(epoch));
    strlcpy(startepoch, DBGet(f_dynLogin, agent), sizeof(startepoch));
    //	AGITool_exec(&agi,&res,"Authenticate",agentpasswd);
    //	if (atoi(res.result)==0) {
    for (i = 1; i < 7; i++)
    {
        snprintf(agentqueue, sizeof(agentqueue), "queue%i", i);
        snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM agent WHERE pkey=?", agentqueue);
        strlcpy(queuename, sqlQueryBind1(myQuery, agent), sizeof(queuename));
        if (strcmp(queuename, "None"))
        {
            snprintf(queuearg, sizeof(queuearg), "%s,%s", queuename, agentchan);
            AGITool_exec(&agi, &res, "RemoveQueueMember", queuearg);
        }
    }
    if (strcmp(DBGet(f_dynLogin, agent), ""))
    {
        timediff = atoi(epoch) - atoi(startepoch);
    }
    snprintf(buffer, sizeof(buffer), "%s|%s|NONE|Agent/%s|AGENTLOGOFF|-|%i", epoch, uniqueid, agent, timediff);
    QLogWrite(buffer);
    DBDel(f_dynLogin, agent);
    DBDel(f_dAgent, agent);
    DBDel(f_eAgent, callerid);
    AGITool_exec(&agi, &res, "Playback", "agent-loggedoff");
    //	}
}

void AgentPause()
{

    DebugFunctionTrace(__FUNCTION__);

    char agent[8] = {'\0'};      // agent number
    char queuearg[256] = {'\0'}; // argument
    char f_eAgent[64] = {'\0'};

    snprintf(f_eAgent, sizeof(f_eAgent), "%s/eAgent", myClusterContext);
    strlcpy(agent, DBGet(f_eAgent, callerid), sizeof(agent));

    if (strcmp(agent, ""))
    {
        snprintf(queuearg, sizeof(queuearg), ",Local/%s@%s", callerid, myClusterContext);
        AGITool_exec(&agi, &res, "PauseQueueMember", queuearg);
    }
    AGITool_exec(&agi, &res, "Playback", "beep");
}

void AgentUnpause()
{

    DebugFunctionTrace(__FUNCTION__);

    char agent[8] = {'\0'};      // agent number
    char queuearg[256] = {'\0'}; // argument
    char f_eAgent[64] = {'\0'};

    snprintf(f_eAgent, sizeof(f_eAgent), "%s/eAgent", myClusterContext);
    strlcpy(agent, DBGet(f_eAgent, callerid), sizeof(agent));

    if (strcmp(agent, ""))
    {
        snprintf(queuearg, sizeof(queuearg), ",Local/%s@%s", callerid, myClusterContext);
        AGITool_exec(&agi, &res, "UnPauseQueueMember", queuearg);
    }
    AGITool_exec(&agi, &res, "Playback", "beep");
}

void AgentSpy()
{

    DebugFunctionTrace(__FUNCTION__);

    if (Authenticate("SPYPASS") != 0) //** spy_pass is held in the tenant table (used to be in Globals)
    {
        return;
    }
    AGITool_exec(&agi, &res, "ChanSpy", "Agent");
}

//ToDo needs to become extenSpy
void ChanSpyWhisper()
{

    DebugFunctionTrace(__FUNCTION__);

    char ext[MAX_EXT_LEN] = {'\0'};
    char options[32] = {'\0'};
    if (Authenticate("SPYPASS") != 0) //** spy_pass is held in the tenant table (used to be in Globals)
    {
        return;
    }
 
    strlcat(ext, GetExt(PARM_CMD), sizeof(ext));
    strlcpy(options, SIPDRIVER, sizeof(options));
    strlcat(options, ext, sizeof(options));
    strlcat(options, ",qw", sizeof(options));
    AGITool_exec(&agi, &res, "ChanSpy", options);
}

//ToDo needs to become extenSpy
void ChanSpy()
{

    DebugFunctionTrace(__FUNCTION__);

    char ext[MAX_EXT_LEN] = {'\0'};
    char options[32] = {'\0'};
    if (Authenticate("SPYPASS") != 0) //** spy_pass is held in the tenant table (used to be in Globals)
    {
        return;
    }
//    strlcpy(ext, myClusterId, sizeof(ext));
    strlcat(ext, GetExt(PARM_CMD), sizeof(ext));
    strlcpy(options, SIPDRIVER, sizeof(options));
    strlcat(options, "/", sizeof(options));
    strlcat(options, ext, sizeof(options));
    strlcat(options, ",q", sizeof(options));
    AGITool_exec(&agi, &res, "ChanSpy", options);
}


void OutRoute()
{

    DebugFunctionTrace(__FUNCTION__);

    char path[4][128] = {{'\0'}};
    char active[4] = {'\0'};
    char technology[64] = {'\0'};
    char alternate[MAX_EXT_LEN] = {'\0'};
    char strategy[8] = {'\0'};
    char auth[4] = {'\0'};
    char setlast[64] = "GLOBAL(";
    char clusterGroup[64] = {'\0'};
    char clusterCount[64] = {'\0'};
    char extenAbstimeout[16] = {'\0'};
    char dfbuf[640] = {'\0'};
    int i;
    int last;

    strlcpy(myClusterclid, g_cluster_cfg.clusterclid, sizeof(myClusterclid));

    /* cluster.dynamicfeatures is the Asterisk DYNAMIC_FEATURES string (replaces SET_DYNAMIC_FEATURES dialplan global). */
    if (g_cluster_cfg.dynamicfeatures[0] != '\0')
    {
        snprintf(dfbuf, sizeof(dfbuf), "__DYNAMIC_FEATURES=%s", g_cluster_cfg.dynamicfeatures);
        AGITool_exec(&agi, &res, "Set", dfbuf);
    }

    
    /*
     * Cluster absolute timeout (tenant); 0 means barred.
     */
    if (g_cluster_cfg.abstimeout_sec == 0)
    {
        AGITool_exec(&agi, &res, "Playtones", "busy");
        AGITool_exec(&agi, &res, "Busy", "");
        return;
    }
    abstimeint = g_cluster_cfg.abstimeout_sec;

    /*
     * set timeout to the extenAbstimeout (if present)...
     * ...and check if the exten is barred (extenAbstimeout=0)
     *
     */
    if (caller_is_local)
    {
        sqlQueryBind2("SELECT abstimeout FROM ipphone WHERE shortuid=? AND cluster=?", extension, myCluster);
        strlcpy(extenAbstimeout, rescols[0], sizeof(extenAbstimeout));
        if (strcmp(extenAbstimeout, ""))
        {
            if (atoi(extenAbstimeout) == 0)
            {
                AGITool_exec(&agi, &res, "Playtones", "busy");
                AGITool_exec(&agi, &res, "Busy", "");
                return;
            }
            abstimeint = atoi(extenAbstimeout);
        }
    }

    // now we can set the final timeout for the call
    snprintf(abstimeout, sizeof(abstimeout), "TIMEOUT(absolute)=%i", abstimeint);

    /*
     * if a chanmax value is set in the cluster then check it.  If not then
     * leave the check to the global voipmax if this is a voip call
     *
     */
    if (g_cluster_cfg.chanmax_str[0] != '\0')
    {
        snprintf(clusterGroup, sizeof(clusterGroup), "GROUP(%s)", myCluster);
        AGITool_set_variable(&agi, &res, clusterGroup, myCluster);
        snprintf(clusterCount, sizeof(clusterCount), "GROUP_COUNT(%s)", myCluster);
        AGITool_get_variable(&agi, &res, clusterCount); // clusterCount is the number of active outbound calls
        if (atoi(res.data) > atoi(g_cluster_cfg.chanmax_str))
        {
            AGITool_exec(&agi, &res, "Playtones", "busy");
            AGITool_exec(&agi, &res, "Busy", "");
            return;
        }
    }

    sqlQueryBind1("SELECT auth,path1,path2,path3,path4,alternate,strategy FROM Route WHERE pkey=?", PARM_KEY);

    strlcpy(auth, rescols[0], sizeof(auth));
    strlcpy(path[0], rescols[1], sizeof(path[0]));
    strlcpy(path[1], rescols[2], sizeof(path[1]));
    strlcpy(path[2], rescols[3], sizeof(path[2]));
    strlcpy(path[3], rescols[4], sizeof(path[3]));
    strlcpy(alternate, rescols[5], sizeof(alternate));
    strlcpy(strategy, rescols[6], sizeof(strategy));

    if (!strncmp(auth, "YES", 3))
    {
        snprintf(eparm, sizeof(eparm), "/etc/asterisk/selauth.conf%sa", ASTDLIM);
        AGITool_exec(&agi, &res, "Authenticate", eparm);
        if (atoi(res.result) != 0)
        {
            return;
        }
    }
    /*
     * set the first path (if we are balancing)
     */
    last = 0;
    if (!strncmp(strategy, "balance", 7))
    {
        AGITool_get_variable(&agi, &res, PARM_KEY); // PARM_KEY is the path key - no change for PBX3
        last = atoi(res.data);
        if (last < 3)
        {
            last++;
        }
        else
        {
            last = 0;
        }
    }
    /*
     *  iterate through the available trunks
     *  i is just a counter, the integer 'last'
     *  points to the next selected path
     *
     */
    for (i = 0; i < 3; i++)
    {       
        strlcpy(active, sqlQueryBind1("SELECT active FROM trunks WHERE pkey=?", path[last]), sizeof(active));
        if (!strcmp(active, "YES"))
        {
            if (!strncmp(strategy, "balance", 7))
            {
// GLOBAL is the global call counter
                snprintf(setlast, sizeof(setlast), "GLOBAL(%s)=%d", PARM_KEY, last);
                AGITool_exec(&agi, &res, "Set", setlast);
            }
            else if (strcmp(path[last], "None"))
            {
                OutVoip(path[last]);
            }
        }
    
        AGITool_get_variable(&agi, &res, "DIALSTATUS"); // DIALSTATUS is the status of the call - answered, busy, cancelled
        if (!strcmp(res.data, "ANSWER"))
        {
            return;
        }
        if (!strcmp(res.data, "BUSY"))
        {
            if (!strncmp(g_cluster_cfg.playbusy, "YES", 3))
            {
                AGITool_exec(&agi, &res, "Playtones", "busy");
                AGITool_exec(&agi, &res, "Busy", "");
            }
            else
            {
                AGITool_exec(&agi, &res, "Playback", "numb-dialled-busy");
                AGITool_exec(&agi, &res, "Playback", "silence/1");
                AGITool_exec(&agi, &res, "Playback", "please-try-again-later");
            }
            return;
        }

        if (!strcmp(res.data, "CANCEL"))
        {
            return;
        }

        if (!strncmp(g_cluster_cfg.playbeep, "YES", 3) && strcmp(path[last], "None"))
        {
            AGITool_exec(&agi, &res, "Playback", "beep");
        }
        last++;
        if (last >= 3)
        {
            last = 0;
        }
    }   

    if (strcmp(alternate, "") && strcmp(alternate, extension))
    {
        AGITool_set_priority(&agi, &res, 1);
        AGITool_set_extension(&agi, &res, alternate);
        AGITool_set_context(&agi, &res, myClusterContext);
        return;
    }

    if (!strncmp(g_cluster_cfg.playcongested, "YES", 3))
    {
        AGITool_exec(&agi, &res, "Playtones", "congestion");
        AGITool_exec(&agi, &res, "Congestion", "");
    }
    else
    {
        AGITool_exec(&agi, &res, "Playback", "were-sorry");
        AGITool_exec(&agi, &res, "Playback", "call-cannot-complete");
        AGITool_exec(&agi, &res, "Playback", "please-hang-up-and-try-again");
    }
}


void OutTrunk(char *key)
{

    DebugFunctionTrace(__FUNCTION__);

    char active[4] = {'\0'};

    strlcpy(active, sqlQueryBind1("SELECT active FROM trunks WHERE pkey=?", PARM_KEY), sizeof(active));


    if (!strcmp(active, "YES"))
    {
        OutVoip(key);
        AGITool_get_variable(&agi, &res, "DIALSTATUS"); // DIALSTATUS is the status of the call - answered, busy, cancelled
        if (!strcmp(res.data, "ANSWER"))
        {
        }
        else if (!strcmp(res.data, "BUSY"))
        {
            if (!strncmp(g_cluster_cfg.playbusy, "YES", 3))
            {
                AGITool_exec(&agi, &res, "Playtones", "busy");
                AGITool_exec(&agi, &res, "Busy", "");
            }
            else
            {
                AGITool_exec(&agi, &res, "Playback", "numb-dialled-busy");
                AGITool_exec(&agi, &res, "Playback", "silence/1");
                AGITool_exec(&agi, &res, "Playback", "please-try-again-later");
            }
        }
        else
        {
            if (!strncmp(g_cluster_cfg.playcongested, "YES", 3))
            {
                AGITool_exec(&agi, &res, "Playtones", "congestion");
                AGITool_exec(&agi, &res, "Congestion", "");
            }
            else
            {
                AGITool_exec(&agi, &res, "Playback", "were-sorry");
                AGITool_exec(&agi, &res, "Playback", "call-cannot-complete");
                AGITool_exec(&agi, &res, "Playback", "please-hang-up-and-try-again");
            }
        }
    }
}

void OutVoip(char *key)
{

    DebugFunctionTrace(__FUNCTION__);

    char dialString[MAX_DIALSTR_LEN] = {'\0'};
    char technology[128] = {'\0'};
    char username[128] = {'\0'};
    char voipnum[128] = {'\0'};
    char peername[128] = {'\0'};
    char desc[128] = {'\0'};
    char callprogress[4] = {'\0'};
    char transform[MAX_TRANSFORM_LIST_LEN] = {'\0'};
    char number[MAX_EXT_LEN] = {'\0'};
    char preSel[MAX_PRESEL_LEN] = {'\0'};
    char recRet[8] = {'\0'};

    sqlQueryBind1("SELECT username,peername,callprogress,desc,transform,match,technology FROM trunks WHERE pkey=?", key);
    strlcpy(username, rescols[0], sizeof(username)); 
    strlcpy(peername, rescols[1], sizeof(peername)); 
    strlcpy(callprogress, rescols[3], sizeof(callprogress));
    strlcpy(desc, rescols[3], sizeof(desc));
    strlcpy(transform, rescols[4], sizeof(transform));
    strlcpy(preSel, rescols[5], sizeof(preSel));    //match
    strlcpy(technology, rescols[6], sizeof(technology));


    if (!strcmp(peername, ""))
    {
        strlcpy(peername, desc, sizeof(peername));
    }

    if (strcmp(transform, ""))
    {
        strlcpy(number, Mangle(preSel, transform, ""), sizeof(number));
    }
    else
    {
        strcpy(number, extension);
        if (strcmp(preSel, ""))
        {
            strlcpy(number, StripPreselect(preSel, number), sizeof(number));
        }
    }

    if (!strcmp(technology, "IAX2"))
    {
        snprintf(dialString, sizeof(dialString), "IAX2/%s@%s/%s", username, peername, number);
    }
    else 
    {
        snprintf(dialString, sizeof(dialString), "%s/%s@%s", SIPDRIVER, number, peername);
    }

    // CLIP
    // rename these trunks to intersite
        // if this is a sx to sx trunk (called InterSARK) then leave the clip as the extension number even if there
        // are overrides.  This caters for inter site calls where the caller wants to send their extension number even
        // though they normally send a DDI on an outbound call.
        if (strcmp(technology, "SailToSail") && strcmp(technology, "InterSARK"))
    {
        outboundClip(PARM_KEY);
    }
    // ENDCLIP

    strlcat(dialString, ASTDLIM, sizeof(dialString));
    strlcat(dialString, ASTDLIM, sizeof(dialString));

    if (!strcmp(g_cluster_cfg.allowhashxfer, "enabled"))
    {
        strlcat(dialString, "T", sizeof(dialString));
    }
    // early media
    if (!strcmp(callprogress, "YES"))
    {
        strlcat(dialString, "r", sizeof(dialString));
    }
    /* Call-forward to external: optional early media + answer before Dial (tenant cluster cfg). */
    if (rdnis_is_set)
    {
        if (!strcmp(g_cluster_cfg.cfwd_progress, "enabled"))
        {
            strlcat(dialString, "r", sizeof(dialString));
        }
        if (!strcmp(g_cluster_cfg.cfwd_answer, "enabled"))
        {
            AGITool_answer(&agi, &res);
        }
    }

    AGITool_set_variable(&agi, &res, "GROUP()", "OUTBOUND_GROUP");
    AGITool_get_variable(&agi, &res, "GROUP_COUNT()"); // GROUP_COUNT() is the number of active outbound calls
    if (atoi(res.data) <= atoi(g_cluster_cfg.voipmax_str))
    {
        strlcpy(recRet, SetRecord(callerid, "Outbound"), sizeof(recRet));
        strlcat(dialString, recRet, sizeof(dialString));
        //  Abs timeout
        AGITool_exec(&agi, &res, "Set", abstimeout);
        //  acounting
        // setOutbound_cdr_userfield();
        // Dial
        AGITool_exec(&agi, &res, "Dial", dialString);
    }
}

void LepDial()
{

    DebugFunctionTrace(__FUNCTION__);

    char vmbox[64] = {'\0'};
    char blindtransfer[MAX_EXT_LEN] = {'\0'};
    char transferer[MAX_EXT_LEN] = {'\0'};
    char cellphone[MAX_EXT_LEN] = {'\0'};
    char celltwindial[MAX_EXT_LEN] = {'\0'};
    char *pBtr = &blindtransfer[4];
    char calleridsave[MAX_EXT_LEN] = {'\0'};
    char dstatus[16] = {'\0'};
    char vmflags[4] = {'\0'};
    char calledCluster[MAX_CLUSTER_LEN] = {'\0'};
    char includeClusters[8192] = {'\0'};
    char extalert[128] = {'\0'};

    AGITool_get_variable(&agi, &res, "BLINDTRANSFER");  //set in extensions.conf
    strlcpy(blindtransfer, res.data, sizeof(blindtransfer));

    sqlQueryBind2("SELECT dvrvmail,extalert,cluster FROM ipphone WHERE shortuid=? AND cluster=?", extension, myCluster);

    strlcpy(vmbox, rescols[0], sizeof(vmbox));
    strlcpy(extalert, rescols[1], sizeof(extalert));
    strlcpy(calledCluster, rescols[2], sizeof(calledCluster));

    if (strcmp(vmbox, "None"))
    {
        strlcat(vmbox, "@", sizeof(vmbox));
        strlcat(vmbox, calledCluster, sizeof(vmbox));
    }

    strlcpy(vmflags, ASTDLIM, sizeof(vmflags));
    if (!strcmp(g_cluster_cfg.voiceinstr, "NO"))
    {
        strlcat(vmflags, "s", sizeof(vmflags));
    }
    /*
        else {
            strcat(vmflags, "u");
        }
    */
    if (!strcmp(DBGet("cfim", extension), extension))
    {
        if (!strcmp(vmbox, "None"))
        {
            if (strcmp(blindtransfer, ""))
            {
                if (strcmp(agi_dnid, extension))
                {
                    AGITool_exec(&agi, &res, "Playback", "silence/1");
                    if (!strcmp(g_cluster_cfg.playtransfer, "YES"))
                    {
                        AGITool_exec(&agi, &res, "Playback", "pls-hold-while-try");
                    }
                    strlcpy(transferer, pBtr, sizeof(transferer));
                    AGITool_set_priority(&agi, &res, 1);
                    AGITool_set_extension(&agi, &res, transferer);
                    AGITool_set_context(&agi, &res, myClusterContext);
                    return;
                }
            }
            else
            {
                AGITool_exec(&agi, &res, "Playtones", "congestion");
                AGITool_exec(&agi, &res, "congestion", "");
                return;
            }
        }
        else
        {
            //            AGITool_exec(&agi,&res,"Playback","silence/1");
            strlcat(vmflags, "u", sizeof(vmflags));
            AGITool_exec(&agi, &res, "Voicemail", strcat(vmbox, vmflags));
            return;
        }
    }
    /*
     *  check and send PBX forwards
     */
    if (!CFCheck("cfim", extension))
    {
        if (debug)
        {
            snprintf(vmsg, sizeof(vmsg), "Trace left CFCheck with extension=%s",extension); 
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }

        return;
    }

/**
 *  Add a SIP header if we have one (usually a hook directive or distinctive ring)
 */
    if (strcmp(extalert, ""))
    {
        AGITool_exec(&agi, &res, "SIPAddHeader", extalert);
    }

 /**
  *  set a pickup mark for directed call pickup
  */
    AGITool_set_variable(&agi, &res, "__PICKUPMARK", extension);

/**
 *  Add a twin if we have one enabled
 */
    strlcpy(cellphone, DBGet("srktwin", extension), sizeof(cellphone));
    if (strcmp(cellphone, ""))
    {
        strlcpy(celltwindial, "&Local/", sizeof(celltwindial));
        strlcat(celltwindial, cellphone, sizeof(celltwindial));
        strlcat(celltwindial, "@", sizeof(celltwindial));
        strlcat(celltwindial, myClusterContext, sizeof(celltwindial));
    }
    PrepDial(extension, "", celltwindial, vmbox);

/**
 *  SET both the dialstring, the CFBS outcome AND voicemail here so
 *  the dialplan doesn't have to ever come back through the same exten
 *  on this call-leg - maybe
 */


/**
 * 
 *  Below here is after the dial has ended
 *  We should likely split this here to release the agi
 *  during the call
 * 
 */

    /*
     * read any stuff hanging around in the pipe
     */
    //	AGITool_Init(&agi);

    AGITool_get_variable(&agi, &res, "DIALSTATUS"); // DIALSTATUS is the status of the call - answered, busy, cancelled
    strcpy(dstatus, res.data);
    if (!strcmp(dstatus, "ANSWER"))
    { // shouldn't ever happen when running HUP'd

        return;
    }

    // check busy or no-answer call forwards

    if (!CFCheck("cfbs", extension))
    {
        return;
    }

    if (!strcmp(dstatus, "NOANSWER"))
    {
        if (!strcmp(vmbox, "None"))
        {
            if (strcmp(blindtransfer, ""))
            {
                if (strcmp(agi_dnid, extension))
                {
                    if (g_cluster_cfg.bounce_alert[0] != '\0')
                    {
                        AGITool_exec(&agi, &res, "SIPAddHeader", g_cluster_cfg.bounce_alert);
                    }
                    strlcpy(calleridsave, callerid, sizeof(calleridsave));
                    strlcpy(callerid, "R", sizeof(callerid));
                    strlcat(callerid, calleridsave, sizeof(callerid));
                    AGITool_set_callerid(&agi, &res, callerid);
                    strlcpy(transferer, pBtr, sizeof(transferer));
                    AGITool_set_priority(&agi, &res, 1);
                    AGITool_set_extension(&agi, &res, transferer);
                    AGITool_set_context(&agi, &res, myClusterContext);
                    return;
                }
            }
        }
        else
        {
            strlcat(vmflags, "u", sizeof(vmflags));
            //               AGITool_exec(&agi,&res,"Playback","silence/1");
            AGITool_exec(&agi, &res, "Voicemail", strcat(vmbox, vmflags));
            return;
        }
    }
    //
    //    BUSY/CONGESTION/CHANUNAVAIL/Bounce-now-busy & yada yada yada
    //
    //    if (!strcmp(dstatus, "BUSY")) {
    if (!strcmp(vmbox, "None"))
    {
        if (strcmp(blindtransfer, ""))
        {
            if (strcmp(agi_dnid, extension))
            {
                AGITool_exec(&agi, &res, "Playback", "silence/1");
                if (!strcmp(g_cluster_cfg.playtransfer, "YES"))
                {
                    AGITool_exec(&agi, &res, "Playback", "pls-hold-while-try");
                }

                if (g_cluster_cfg.bounce_alert[0] != '\0')
                {
                    AGITool_exec(&agi, &res, "SIPAddHeader", g_cluster_cfg.bounce_alert);
                }
                strlcpy(transferer, pBtr, sizeof(transferer));
                AGITool_set_priority(&agi, &res, 1);
                AGITool_set_extension(&agi, &res, transferer);
                AGITool_set_context(&agi, &res, myClusterContext);
                return;
            }
            else
            {
                if (g_cluster_cfg.blind_busy[0] != '\0')
                {
                    AGITool_set_priority(&agi, &res, 1);
                    AGITool_set_extension(&agi, &res, g_cluster_cfg.blind_busy);
                    AGITool_set_context(&agi, &res, myClusterContext);
                    return;
                }
                else
                {
                    AGITool_exec(&agi, &res, "Playtones", "busy");
                    AGITool_exec(&agi, &res, "Busy", "");
                    return;
                }
            }
        }
        else
        {
            AGITool_exec(&agi, &res, "Playtones", "busy");
            AGITool_exec(&agi, &res, "Busy", "");
            return;
        }
    }
    else
    {
        strlcat(vmflags, "b", sizeof(vmflags));
        AGITool_exec(&agi, &res, "Voicemail", strcat(vmbox, vmflags));
        return;
    }

    AGITool_exec(&agi, &res, "Playtones", "busy");
    AGITool_exec(&agi, &res, "Busy", "");
    return;
}

void PrepDial(char *number, char *type, char *twin, char *vmbox)
{

    DebugFunctionTrace(__FUNCTION__);

    char dialString[MAX_DIALSTR_LEN] = {'\0'};
    char intRingDelay[MAX_RING_LEN] = {'\0'};
    char userRingDelay[MAX_RING_LEN] = {'\0'};
    char recRet[8] = {'\0'};
    char setcdrcmduser[64] = "CDR(userfield)=";

/**
 *  set the dialled number (DNID) in the CDR user field
 */
    strlcat(setcdrcmduser, agi_dnid, sizeof(setcdrcmduser));
    AGITool_exec(&agi, &res, "Set", setcdrcmduser);

/**
 *  begin to set up the diasltring
 */
    strlcpy(dialString, SIPDRIVER, sizeof(dialString));
    strlcat(dialString, "/", sizeof(dialString));
    strlcat(dialString, number, sizeof(dialString));

/**
 *  Set the ring timeout for everything but dials coming in off the 
 *  Queues subsystem - they set their own
 */

    if (strcmp(type, "queue"))
    {
        strlcpy(userRingDelay, DBGet("ringdelay", number), sizeof(userRingDelay));
        if (!strcmp(vmbox, "None"))
        {
            strlcpy(intRingDelay, "", sizeof(intRingDelay));
        }
        else if (!strcmp(userRingDelay, ""))
        {
            strlcpy(intRingDelay, g_cluster_cfg.int_ring_delay, sizeof(intRingDelay));
        }
        else if (!strcmp(userRingDelay, "0"))
        {
            strlcpy(intRingDelay, "", sizeof(intRingDelay));
        }

        else
        {
            strlcpy(intRingDelay, userRingDelay, sizeof(intRingDelay));
        }
/**
 *      deal with cellphone twinning
 */

        if (strcmp(twin, ""))
        {
            strlcat(dialString, twin, sizeof(dialString));
        }

        strlcat(dialString, ASTDLIM, sizeof(dialString));
        strlcat(dialString, intRingDelay, sizeof(dialString));
        strlcat(dialString, ASTDLIM, sizeof(dialString));
/**
 *       Don't allow external callers to drive a # transfer
 */       
        if (caller_is_local)
        {
            strlcat(dialString, "ktT", sizeof(dialString));
        }
        else
        {
            strlcat(dialString, "kt", sizeof(dialString));
        }
    }
    else
/**
 *      This is a queue dial
 */
    {
        strlcat(dialString, ASTDLIM, sizeof(dialString));
        strlcat(dialString, ASTDLIM, sizeof(dialString));
    }

/**
 *  stop queue dial-legs (which do come through here) from firing off "ghost" recordings
 */
    if (strcmp(type, "queue"))
    {
        strlcpy(recRet, SetRecord(number, "Inbound"), sizeof(recRet));
        strlcat(dialString, recRet, sizeof(dialString));
    }
/**
 *  If the dial definition has asked for MOH instead of ringback then set it here
 */
    AGITool_get_variable(&agi, &res, "MOH"); //set ON/OFF in inbound routes from the dial definition
    if (!strcmp(res.data, "YES"))
    {
        strlcat(dialString, "m", sizeof(dialString));
    }
/**
 *  Send it to the dialler
 */
    AGITool_exec(&agi, &res, "Dial", dialString);
/**
 * placeHolder
 
        AGITool_set_priority(&agi, &res, 1);
        AGITool_set_extension(&agi, &res, cfnum);
        AGITool_set_context(&agi, &res, myClusterContext);
*/
        return;
}

char *SetRecord(char *key, char *compass)
{

/**
 *   As of Asterisk 21, MONITOR is no longer supported.   This has forced a rewrite of this code.
 * 
 *     https://www.asterisk.org/monitor-is-deprecated-farewell/
 * 
 *  You should also familiarize yourself with PCI DSS if you plan on recording where financial data is exchanged in the call
 *  
 *     https://www.pcisecuritystandards.org/standards/
 * 
 *  If you are planning to take sensitive financial info over the phone you should use a specialist third party to mask the 
 *  spoken/DTMF input and to do the checking for you.  It's relatively easy withiin Asterisk but it does need some setup. 
 *  The other alternative is to simply NOT record inbound calls which are likely to involve any kind of financial detail. 
 */

    DebugFunctionTrace(__FUNCTION__);
    snprintf(vmsg, sizeof(vmsg), "key is %s, compass is %s",key,compass);
    DebugFunctionMsg(__FUNCTION__, vmsg);

    char devicerec[16] = {'\0'};
    char callRecord[16] = {'\0'};
    char soundFile[MAX_FILENAME_LEN] = {'\0'};
    char execFile[MAX_FILENAME_LEN] = {'\0'};
    char filename[MAX_FILENAME_LEN] = {'\0'};
    //    char recFilename[MAX_FILENAME_LEN] = {'\0'};
    char filework[MAX_FILENAME_LEN] = {'\0'};
    //    char recChannel[64] = {'\0'};
    //    char recChanname[64] = {'\0'};
    char dialChar[2] = {'\0'};
    char dialString[2] = {'\0'};
    char *pdial = &dialString[0];
    int record = 0;

    time_t now;
    now = time(0);

/**
 *   Check default recording setting from the cluster
 *   callrecord1    'in:None,OTR,OTRR,Inbound,Outbound,Both'
 *   myCluster is the dialplan context = cluster.shortuid; load_cluster_cfg matches pkey OR shortuid.
 */
    strlcpy(callRecord, g_cluster_cfg.callrecord_1, sizeof(callRecord));
    snprintf(vmsg, sizeof(vmsg), "callrecord1 is %s",callRecord);
    DebugFunctionMsg(__FUNCTION__, vmsg);

/**
 *  Check if terget is a queue
 */
    if (strcmp(key, "Qexec")) 
    {
/**
 *  Check phone recording setting
 */
        sqlQueryBind1("SELECT devicerec FROM ipphone WHERE shortuid=?", key);
        strlcpy(devicerec, rescols[0], sizeof(devicerec));
    } 
    else
    {
/**
 *  Check Queue recording setting
 *  and copy the key ("Qexec") to the beginning of the filename var
 *  This will used later by the sweeper task (MONITOR_EXEC) to rename the file and
 *  add the queuename and agent (if any)
 */
        strcpy(filename, key);
        sqlQueryBind2("SELECT devicerec FROM Queue WHERE pkey=? AND cluster=?", agi_dnid, myCluster);
        strlcpy(devicerec, rescols[0], sizeof(devicerec));        
    }

    snprintf(vmsg, sizeof(vmsg), "devicerec is %s",devicerec);
    DebugFunctionMsg(__FUNCTION__, vmsg);
/**
 *  device recording is off - exit
 */
    if (!strcmp(devicerec, "None"))
    {
        return 0;
    }
/**
 *  If there's a rec value in the phone, use it
 */
    if (strcmp(devicerec, "")) 
    {
        if (strcmp(devicerec, "default"))
        {
            strlcpy(callRecord, devicerec, sizeof(callRecord));
        }
    }
    snprintf(vmsg, sizeof(vmsg), "callrecord1 is set to %s",devicerec);
    DebugFunctionMsg(__FUNCTION__, vmsg);



/**
 * Set a char (dialChar) to use later when constructing the dialstring
 * OTR W = callee, w = caller
 * check if call is inbound or outbound and set accordingly
 *
 */ 
    strlcpy(dialChar, "W", sizeof(dialChar));
    if (!strcmp(compass, "Inbound"))
    {
        strlcpy(dialChar, "w", sizeof(dialChar));
    }

/**
 * 
 *  Regular call 
 */
 
    if (!strcmp(callRecord, "OTR"))
    {
        strlcpy(dialString, dialChar, sizeof(dialString));
    }
    if (!strcmp(callRecord, compass) || !strcmp(callRecord, "Both"))
    {
        record = 1;
    }



    if (record)
    {
        snprintf(filework, sizeof(filework), "%d-%s-%s-%s", (int)time(&now), myCluster, agi_dnid, callerid);
        strlcat(filename, filework, sizeof(filename));

        /**
         * FILE refs MUST BE CHANGED *DONE*
         */
        snprintf(soundFile, sizeof(soundFile), " /var/spool/asterisk/monitor/%s/%s.wav", myCluster, filename);
        AGITool_exec(&agi, &res, "MixMonitor", soundFile);

    }
    return pdial;
}

void Page()
/**
 * No longer needs localip
 */
{

    DebugFunctionTrace(__FUNCTION__);

    char dialStr[2048] = {'\0'};
    char sipHeader[64] = {'\0'};
    char speedKey[32] = {'\0'};
    char ext[MAX_EXT_LEN] = {'\0'};

    strlcpy(ext, GetExt(PARM_CMD), sizeof(ext));
    strlcat(speedKey, ext, sizeof(speedKey));

    snprintf(sipHeader, sizeof(sipHeader), "Call-Info:<sip:127.0.0.1>;answer-after=0");
    // Page all extensions
    if (!strcmp(ext, ""))
    {
        sqlQueryBind1("SELECT pagegroup FROM page WHERE pkey=?", "pageall");
        strlcpy(dialStr, rescols[0], sizeof(dialStr));
        AGITool_exec(&agi, &res, "SIPAddHeader", sipHeader);
        AGITool_exec(&agi, &res, "Page", dialStr);
    }
    // Page single extension
    else
    {
        if (!strcmp(ext, sqlQueryBind1("SELECT pkey FROM IPphone WHERE pkey=?", ext)))
        {
            strlcpy(dialStr, SIPDRIVER, sizeof(dialStr));
            strlcat(dialStr, "/", sizeof(dialStr));
            strlcat(dialStr, ext, sizeof(dialStr));
            AGITool_exec(&agi, &res, "SIPAddHeader", sipHeader);
            AGITool_exec(&agi, &res, "Page", dialStr);
        }
        // Page a group of extensions
        else
        {
            if (!strcmp(ext, sqlQueryBind1("SELECT pkey FROM speed WHERE pkey=?", speedKey)))
            {
                strlcpy(dialStr, sqlQueryBind1("SELECT pagegroup FROM speed WHERE pkey=?", speedKey), sizeof(dialStr));
                AGITool_exec(&agi, &res, "SIPAddHeader", sipHeader);
                AGITool_exec(&agi, &res, "Page", dialStr);
            }
        }
    }
}

/**
 *  Checks for a call forward and actions it.
 *  type - cfbs or cfim
 *  number - number to check
 */
char *CFCheck(char *type, char *number)
{

    DebugFunctionTrace(__FUNCTION__);

    //    char cflist[MAX_CALL_FWD_CHAIN_LEN][MAX_FWD_NUM_LEN] = {{'\0'}};
    char cfnum[MAX_FWD_NUM_LEN] = {'\0'};
    //    int i, cnt=0;
    char rdnis_string[32] = "CALLERID(rdnis)=";
    char vmflags[4] = {'\0'};
    char vmbox[MAX_EXT_LEN] = {'\0'};
    char speedKey[32] = {'\0'};
/**
 * ***********************N.B.***************************
 *  VOICEINSTR is currently global.   It needs to move to Cluster or DB
 * ******************************************************
 */
    strlcpy(vmflags, ASTDLIM, sizeof(vmflags));

    if (!strcmp(g_cluster_cfg.voiceinstr, "NO"))
    {
        strlcat(vmflags, "su", sizeof(vmflags));
    }
    else
    {
        strlcat(vmflags, "u", sizeof(vmflags));
    }

    if (strcmp(DBGet(type, number), ""))
/**
 *  We have a forward set...
 */
    {
        strlcpy(cfnum, DBGet(type, number), sizeof(cfnum));
/* Is this DND? */
        if (!strcmp(cfnum,agi_dnid)) 
        {
/**
 *  then go to voicemail
 */
            sprintf(vmbox,"%s@%s%s",agi_dnid,myCluster,vmflags);
            AGITool_exec(&agi,&res,"Voicemail",vmbox);
			return NULL;
        }
/**
 *  Is this a local divert or are we heading out to the PSTN?
 */
        if (strlen(number) > 5)
/**
 *      Our forward is not local.  The default behaviour is to play a comfort message at this point
 *      to cover the uncertainty of a possible audio pause while the upstream is switching
 *      You can turn this behaviour off if you wish by setting the cluster control variable PLAYTRANSFER to false. 
 */
        {
            AGITool_exec(&agi, &res, "Playback", "silence/1");
            if (!strcmp(g_cluster_cfg.playtransfer, "YES"))
            {
                AGITool_exec(&agi, &res, "Playback", "pls-hold-while-try");
            }
        }
/**
 *      Forwarding to a local extension...
 */
        

/**
 *  Set the rdnis to the original CLI 
 */
    if (strcmp(rdnis,"unknown"))
    {
        strcat(rdnis_string, callerid);
        AGITool_exec(&agi, &res, "Set", rdnis_string);
    }
/**
 *  and branch...
 */
        AGITool_set_priority(&agi, &res, 1);
        AGITool_set_extension(&agi, &res, cfnum);
        AGITool_set_context(&agi, &res, myClusterContext);
        return NULL;
    }
    return number;
}
/// @brief CFToggle - set a call forward in AstDB.  
/**
 *  e.g. *21*1104 - sets a CFIM or CFBS to extension 1104
 *       *21* cancels any forwards
*/
void CFToggle()
{

    DebugFunctionTrace(__FUNCTION__);

    char toNum[32] = {'\0'};
    char *technology;
    char *sipId;

/**
 *  Get the SIP endpoint ID from the channel variable
*/
    technology = strtok(channel,"/");
    sipId = strtok(NULL,"-");

    strlcpy(toNum, GetExt(PARM_CMD), sizeof(toNum));

    DBPut(PARM_KEY, sipId, toNum);

    AGITool_exec(&agi, &res, "Playback", "call-forwarding");
    if (strcmp(toNum, ""))
    {
        AGITool_exec(&agi, &res, "Playback", "activated");
    }
    else
    {
        AGITool_exec(&agi, &res, "Playback", "de-activated");
    }
}

void CFVMailSet()
{

    DebugFunctionTrace(__FUNCTION__);

    char fromNum[32] = {'\0'};
    char *technology;
    char *sipId;

    strlcat(fromNum, callerid, sizeof(fromNum));

/**
 *  Get the SIP endpoint ID from the channel variable
*/
    technology = strtok(channel,"/");
    sipId = strtok(NULL,"-");

    // if the action is ON then activate otherwise de-activate
    AGITool_exec(&agi, &res, "Playback", "call-forwarding");

    // 18 & 19 (ON/OFF) and 20 (toggle self) are the documented ones 

    if (!strncmp(PARM_CMD, "*18*", 3)) // toggle ON
    {
        DBPut("cfim", sipId, fromNum);
        AGITool_exec(&agi, &res, "Playback", "activated");
    }
    else // toggle OFF
    {
        DBPut("cfim", sipId, "");
        AGITool_exec(&agi, &res, "Playback", "de-activated");
    }
}

void CFVMailToggle()
{

    DebugFunctionTrace(__FUNCTION__);

    char fromNum[32] = {'\0'};
    char toNum[32] = {'\0'};
    char *technology;           // Change name - CONFUSING !!!!!!!!!!!
    char *sipId;

    strlcat(fromNum, callerid, sizeof(fromNum));
/**
 *  Get the SIP endpoint ID from the channel variable
*/
    technology = strtok(channel,"/");
    sipId = strtok(NULL,"-");

    // if the property is empty in the DB then activate otherwise de-activate
    AGITool_exec(&agi, &res, "Playback", "call-forwarding");
    if (!strcmp(DBGet("cfim", sipId), ""))
    {
        DBPut("cfim", sipId, fromNum);
        AGITool_exec(&agi, &res, "Playback", "activated");
    }
    else
    {
        DBPut("cfim", sipId, "");
        AGITool_exec(&agi, &res, "Playback", "de-activated");
    }
}

void FollowMe()
{
/**
 * NEEDS TO BE REWRITTEN - not sure we care
*/

/*
    DebugFunctionTrace(__FUNCTION__);

    char realFromNum[MAX_EXT_LEN] = {'\0'};

    //  get real fromnum
    strlcpy(realFromNum, myClusterId, sizeof(realFromNum));
    strcat(realFromNum, fromNum);
    AGITool_exec(&agi, &res, "VMauthenticate", realFromNum);
    if (!atoi(res.result))
    {
        DBPut("cfim", realFromNum, toNum);
        AGITool_exec(&agi, &res, "Playback", "activated");
    }
*/
}

void CFOff()
{

    DebugFunctionTrace(__FUNCTION__);

    char *technology;           // Change name - CONFUSING !!!!!!!!!!!
    char *sipId;
/**
 *  Get the SIP endpoint ID from the channel variable
*/
    technology = strtok(channel,"/");
    sipId = strtok(NULL,"-");

    DBPut("cfim", sipId, "");
    DBPut("cfbs", sipId, "");
    AGITool_exec(&agi, &res, "Playback", "call-forwarding");
    AGITool_exec(&agi, &res, "Playback", "de-activated");
}

void SetRingDelay()
{

    DebugFunctionTrace(__FUNCTION__);

    char fromNum[32] = {'\0'};
    char ringDelay[4] = {'\0'};

    char *technology;           // Change name - CONFUSING !!!!!!!!!!!
    char *sipId;
    

    strlcpy(fromNum, callerid, sizeof(fromNum));
/**
 *  Get the SIP endpoint ID from the channel variable
*/
    technology = strtok(channel,"/");
    sipId = strtok(NULL,"-");

    strncpy(ringDelay,PARM_CMD + 4,sizeof(ringDelay));

    DBPut("ringdelay", sipId, ringDelay);
    AGITool_exec(&agi, &res, "Playback", "silence/1");
    AGITool_exec(&agi, &res, "Playback", "activated");
}

char *StripPreselect(char *preSel, char *number)
{

    DebugFunctionTrace(__FUNCTION__);

    int i;

    // if the number has a preselect then remove it
    if (!strncmp(number, preSel, strlen(preSel)))
    {
        for (i = 0; i < (int)strlen(number); i++)
        {
            number[i] = number[i + strlen(preSel)];
        }
    }
    return number;
}

void SetTimer()
{

    DebugFunctionTrace(__FUNCTION__);

    if (Authenticate("SYSPASS") != 0) //** syspass is held in the tenant table (used to be in Globals)
    {
        return;
    }
    switch (switchdig)
    {
    case 30:
        DBPut("STAT", "OCSTAT", "AUTO");
        break;
    case 31:
        DBPut("STAT", "OCSTAT", "CLOSED");
        break;
    case 32:
        DBPut("STAT", "OCSTAT", "OPEN");
        break;
    default:
        snprintf(vmsg, sizeof(vmsg), "This SetTimer parameter (%i)is not supported", switchdig);
        AGITool_verbose(&agi, &res, vmsg, 1);
        return;
    }
    AGITool_exec(&agi, &res, "Playback", "activated");
    return;
}

void Ingress()
{

    DebugFunctionTrace(__FUNCTION__);

    char technology[32] = {'\0'};
    char tag[64] = {'\0'};
    char swoclip[8] = {'\0'};
    char prefix[32] = {'\0'};
    char moh[16] = {'\0'};
    char alertinfo[128] = {'\0'};
    char clicluster[MAX_CLUSTER_LEN] = {'\0'};
    char setcdrcmduser[64] = "CDR(userfield)=";

    /*
     *  check if we are at max inbound channels
     */
    if (g_cluster_cfg.maxin_str[0] != '\0')
    {
        AGITool_set_variable(&agi, &res, "GROUP(inbound)", "inbound");
        AGITool_get_variable(&agi, &res, "GROUP_COUNT(inbound)"); // GROUP_COUNT(inbound) is the number of active inbound calls

        if (atoi(res.data) > atoi(g_cluster_cfg.maxin_str))
        {
            AGITool_exec(&agi, &res, "Playtones", "busy");
            AGITool_exec(&agi, &res, "Busy", "");
            return;
        }
    }

    // set the dialled number (DDI) in the CDR userfield
    strlcat(setcdrcmduser, PARM_KEY, sizeof(setcdrcmduser));
    AGITool_exec(&agi, &res, "Set", setcdrcmduser);

    if (g_cluster_cfg.dynamicfeatures[0] != '\0')
    {
        char df_ingress[640];
        snprintf(df_ingress, sizeof(df_ingress), "__DYNAMIC_FEATURES=%s", g_cluster_cfg.dynamicfeatures);
        AGITool_exec(&agi, &res, "Set", df_ingress);
    }

    sqlQueryBind1("SELECT technology,tag,inprefix,alertinfo,moh,swoclip FROM inroutes WHERE pkey=?", PARM_KEY);
    strlcpy(technology, rescols[0], sizeof(technology));
    strlcpy(tag, rescols[1], sizeof(tag));
    strlcpy(prefix, rescols[2], sizeof(prefix));
    strlcpy(alertinfo, rescols[3], sizeof(alertinfo));
    strlcpy(moh, rescols[4], sizeof(moh));
    strlcpy(swoclip, rescols[5], sizeof(swoclip));

    AGITool_set_variable(&agi, &res, "__MOH", moh);

    // CLIP Processing

    //  DDI Prefix

    if (strcmp(prefix, ""))
    {
        if (!strcmp(prefix, "0 "))
        {
            strlcpy(prefix, "0", sizeof(prefix));
        }
        strlcat(prefix, callerid, sizeof(prefix));
        strlcpy(callerid, prefix, sizeof(callerid));
        snprintf(clidstrng, sizeof(clidstrng), "CALLERID(number)=%s", callerid);
        AGITool_exec(&agi, &res, "Set", clidstrng);
    }

    //...check CLIP routing and recurse if set
    //... This needs to change to a cluster-centric treatment ////////////////////////////////////////////////

    if (strcmp(swoclip, "NO") && strcmp(callerid, ""))
    {
        if (!strcmp(technology, "PTT_CLID") || !strcmp(technology, "CLID"))
        {
            if (strcmp(callerid, PARM_KEY))
            {
                strlcpy(clicluster, sqlQueryBind1("SELECT cluster FROM inroutes WHERE pkey=?", callerid), sizeof(clicluster));
                if (!strcmp(myCluster, clicluster))
                {
                    AGITool_set_priority(&agi, &res, 1);
                    AGITool_set_extension(&agi, &res, callerid);
                    AGITool_set_context(&agi, &res, "mainmenu");
                    return;
                }
            }
        }
    }

    // regular CLIP
    if (strcmp(tag, ""))
    {
        strlcpy(calleridname, tag, sizeof(calleridname));
        AGITool_exec(&agi, &res, "SetCallerPres", "allowed");
    }
    if (!strcmp(calleridname, "unknown"))
    {
        strlcpy(calleridname, "", sizeof(calleridname));
    }

    //	Alphatag
    if (strcmp(calleridname, ""))
    {
        snprintf(clidstrng, sizeof(clidstrng), "CALLERID(name)=%s", calleridname);
        AGITool_exec(&agi, &res, "Set", clidstrng);
    }

    if (strcmp(g_cluster_cfg.lterm_str, "YES"))
    {
        AGITool_answer(&agi, &res);
        // AGITool_exec(&agi,&res,"Ringing","");
    }
    // cause a ring for voip lines if requested
    if (!strcmp(technology, "SIP"))
    {
        if (strcmp(g_cluster_cfg.ringdelay_str, "0"))
        {
            AGITool_exec(&agi, &res, "Ringing", "");
            AGITool_exec(&agi, &res, "Wait", g_cluster_cfg.ringdelay_str);
        }
    }
    if (!strcmp(technology, "IAX2"))
    {
        if (strcmp(g_cluster_cfg.ringdelay_str, "0"))
        {
            AGITool_exec(&agi, &res, "Ringing", "");
            AGITool_exec(&agi, &res, "Wait", g_cluster_cfg.ringdelay_str);
        }
    }

    // distinctive ring (if present)
    if (strcmp(alertinfo, ""))
    {
        AGITool_exec(&agi, &res, "SIPAddHeader", alertinfo);
    }

    CheckState(PARM_KEY);
}

void CheckState(char *remotenum)
{

    DebugFunctionTrace(__FUNCTION__);

    char state[8] = {'\0'};
    char cluster[MAX_CLUSTER_LEN] = {'\0'};
    char rc_char[8] = {'\0'};

    sqlQueryBind1("SELECT cluster,openroute,closeroute FROM inroutes WHERE pkey=?", remotenum);
    strlcpy(cluster, rescols[0], sizeof(cluster));
    strlcpy(openroute, rescols[1], sizeof(openroute));
    strlcpy(closeroute, rescols[2], sizeof(closeroute));

/**
 * Check the master timers first...
 */

    strlcpy(state, DBGet("STAT", "OCSTAT"), sizeof(state));
/**
 *  If the master timer is closed we simply take the closed route...
 */
    if (!strcmp(state, "CLOSED"))
    {
        //   PBX is in hard CLOSED state - use closed route;
        AGITool_set_priority(&agi, &res, 1);
        AGITool_set_extension(&agi, &res, closeroute);
        AGITool_set_context(&agi, &res, myClusterContext);
    }
    else
/**
 *      If we get to here, the master timer is set to AUTO (usual state), or OPEN.
 *      So, now we can check the timers for the cluster and branch accordingly
 *      First check for hard closed...
 */
    {
        strlcpy(state, CheckTime(cluster), sizeof(state));
        if (!strcmp(state, "CLOSED"))
        {
            //   Closed(remotenum);
            AGITool_set_priority(&agi, &res, 1);
            AGITool_set_extension(&agi, &res, closeroute);
            AGITool_set_context(&agi, &res, myClusterContext);
        }
        else
/**
 *      So, the state must be OPEN/AUTO
 */
        {
            //    Open(remotenum);
            AGITool_set_priority(&agi, &res, 1);
            AGITool_set_extension(&agi, &res, openroute);
            AGITool_set_context(&agi, &res, myClusterContext);
        }
    }
}

char *CheckTime(char *cluster)
{

    DebugFunctionTrace(__FUNCTION__);

    char clusterdboclo[8] = {'\0'};

    if (strcmp(g_cluster_cfg.routeoverride, ""))
    {
        strlcpy(closeroute, g_cluster_cfg.routeoverride, sizeof(closeroute));
        return "CLOSED";
    }

    strlcpy(clusterdboclo, DBGet(cluster, "OCSTAT"), sizeof(clusterdboclo));
    if (!strcmp(clusterdboclo, "CLOSED"))
    {
        return "CLOSED";
    }

    if (!strcmp(DBGet(myCluster, "OCSTAT"), "CLOSED"))
    {
        return "CLOSED";
    }
    if (!strcmp(g_cluster_cfg.oclo, "OPEN"))
    {
        return "OPEN";
    }

    if (!strcmp(g_cluster_cfg.oclo, "CLOSED"))
    {
        return "CLOSED";
    }
    AGITool_exec(&agi, &res, "NoOp", "NO MTIME - returning OPEN");
    return "OPEN";
}

void IVR(char *ivrname)
{

    DebugFunctionTrace(__FUNCTION__);

    char msg[MAX_FILENAME_LEN] = {'\0'};
    char greetnum[8] = {'\0'};
    char option[4] = {'\0'};
    char optionStr[15] = {'\0'};
    char dtmf[8] = {'\0'};
    char ivrsilence[16] = "silence/";
    int ivrdigitwait = 6000;
    int i;

    strlcat(ivrsilence, "6", sizeof(ivrsilence));

    if (g_cluster_cfg.ivr_key_wait[0] != '\0')
    {
        strlcat(ivrsilence, g_cluster_cfg.ivr_key_wait, sizeof(ivrsilence));
    }

    if (g_cluster_cfg.ivr_digit_wait_str[0] != '\0')
    {
        ivrdigitwait = atoi(g_cluster_cfg.ivr_digit_wait_str);
    }

    if (strcmp(ivrname, ""))
    {
        // get the number of options for the menu and construct the option string
        sqlQueryBind1("SELECT option0,option1,option2,option3,option4,option5,option6,option7,option8,option9,option10,option11 FROM ivrmenu WHERE pkey=?", PARM_KEY);
        for (i = 0; i <= 11; i++)
        {
            snprintf(option, sizeof(option), "%d", i);
            if (strcmp(rescols[i], "None"))
            {
                if (i == 10)
                {
                    strlcat(optionStr, "*", sizeof(optionStr));
                }
                else if (i == 11)
                {
                    strlcat(optionStr, "#", sizeof(optionStr));
                }
                else
                {
                    strlcat(optionStr, option, sizeof(optionStr));
                }
            }
        }

        AGITool_exec(&agi, &res, "Wait", "0.5");
        AGITool_answer(&agi, &res);

        strlcpy(greetnum, sqlQueryBind1("SELECT greetnum FROM ivrmenu WHERE pkey=?", PARM_KEY), sizeof(greetnum));
        snprintf(msg, sizeof(msg), "%s%s/usergreeting%s", SOUNDIR, myCluster, greetnum);
        /*
         * We can use stream or get_data, depending upon whether the user wants the
         * ivr to "listen" for an extension dial as well as an ivr choice.
         * choose by setting listenforext to YES/NO in the ivr record
         * Set NO if you just have single dtmf choices because the system will
         * respond immediately to a single key press and won't wait for more dtmf.
         */

        //
        // here is the stream command, it only listens for
        // one digit press.

        if (strcmp(sqlQueryBind1("SELECT listenforext FROM ivrmenu WHERE pkey=?", PARM_KEY), "YES"))
        {
            AGITool_stream_file(&agi, &res, msg, optionStr, 0);
            if (strcmp(optionStr, "") && atoi(res.result))
            {
                snprintf(dtmf, sizeof(dtmf), "%c", atoi(res.result));
                IVRAction(PARM_KEY, dtmf);
                return;
            }
            if (strcmp(optionStr, ""))
            {
                AGITool_stream_file(&agi, &res, ivrsilence, optionStr, 0);
                if (strcmp(optionStr, "") && atoi(res.result))
                {
                    snprintf(dtmf, sizeof(dtmf), "%c", atoi(res.result));
                    IVRAction(PARM_KEY, dtmf);
                    return;
                }
            }
        }
        //
        // here is the get_data command
        // it can listen for more digits - we set it to listen for up to 4 digits.

        else
        {
            AGITool_get_data(&agi, &res, msg, ivrdigitwait, 4);
            if (atoi(res.result))
            {
                // check if it's an extension
                snprintf(dtmf, sizeof(dtmf), "%s", res.result);
                if (strlen(dtmf) > 1)
                {
                    AGITool_set_priority(&agi, &res, 1);
                    AGITool_set_extension(&agi, &res, dtmf);
                    AGITool_set_context(&agi, &res, myClusterContext);
                    return;
                }
                IVRAction(PARM_KEY, dtmf);
                return;
            }
        }

        // If no key is pressed then perform the timeout action
        if (!strcmp(sqlQueryBind1("SELECT timeout FROM ivrmenu WHERE pkey=?", PARM_KEY), "Repeat Message"))
        {
            IVR(ivrname);
            return;
        }
        else
        {
            IVRAction(PARM_KEY, "");
            return;
        }
    }
}

void IVRAction(char *menu, char *press)
{

    DebugFunctionTrace(__FUNCTION__);

    char dbOption[32] = "option";
    char dbAlert[32] = "alert";
    char action[128] = {'\0'};
    char alert[128] = {'\0'};
    char tag[8] = "tag";
    char tagID[32] = {'\0'};

    // if no key press get timeout action
    if (!strcmp(press, ""))
    {
        strlcpy(dbOption, "timeout", sizeof(dbOption));
    }
    else if (!strcmp(press, "*"))
    {
        strcat(dbOption, "10");
        strcat(dbAlert, "10");
        strcat(tag, "10");
    }
    else if (!strcmp(press, "#"))
    {
        strcat(dbOption, "11");
        strcat(dbAlert, "11");
        strcat(tag, "11");
    }
    else
    {
        strlcat(dbOption, press, sizeof(dbOption));
        strlcat(tag, press, sizeof(tag));
        strlcat(dbAlert, press, sizeof(dbAlert));
    }


    //  handle case of timeout being requested
    if (!strcmp(dbOption, "timeout"))
    {
        strlcpy(action, sqlQueryBind1("SELECT timeout FROM ivrmenu WHERE pkey=?", menu), sizeof(action));
    }
    else
    { // handle ordinary case
        snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM ivrmenu WHERE pkey=?", dbOption);
        strlcpy(action, sqlQueryBind1(myQuery, menu), sizeof(action));
        // set Alert-info if present
        snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM ivrmenu WHERE pkey=?", dbAlert);
        strlcpy(alert, sqlQueryBind1(myQuery, menu), sizeof(alert));
        if (strcmp(alert, ""))
        {
            AGITool_exec(&agi, &res, "SIPAddHeader", alert);
        }
        snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM ivrmenu WHERE pkey=?", tag);
        strcpy(tagID, sqlQueryBind1(myQuery, menu));
        if (strcmp(tagID, ""))
        {
            strlcpy(calleridname, tagID, sizeof(calleridname));
        }
    }
    // CLIP Processing

    if (!strcmp(calleridname, "unknown"))
    {
        strlcpy(calleridname, "", sizeof(calleridname));
    }
    //	Alphatag
    if (strcmp(calleridname, ""))
    {
        snprintf(clidstrng, sizeof(clidstrng), "CALLERID(name)=%s", calleridname);
        AGITool_exec(&agi, &res, "Set", clidstrng);
    }
    //  Route it
    if (strcmp(action, "None"))
    {
        AGITool_set_priority(&agi, &res, 1);
        AGITool_set_extension(&agi, &res, action);
        AGITool_set_context(&agi, &res, myClusterContext);
        return;
    }
    // bad key press?
    return;
}

char *DBGet(char *family, char *key)
{

    DebugFunctionTrace(__FUNCTION__);

    AGITool_database_get(&agi, &res, family, key);
    return res.data;
}

void DBPut(char *family, char *key, char *val)
{

    DebugFunctionTrace(__FUNCTION__);

    AGITool_database_put(&agi, &res, family, key, val);
}

void DBDel(char *family, char *key)
{

    DebugFunctionTrace(__FUNCTION__);

    AGITool_database_del(&agi, &res, family, key);
}

static char *sqlQueryBindInternal(const char *sql, const char *arg1, const char *arg2, int bind_count)
{
    char *pVal = &rescols[0][0];
    int retval, i;

    DebugFunctionTrace(__FUNCTION__);

    sqlite3 *handle = sqlGetSharedHandle();
    if (handle == NULL)
    {
        return "-1";
    }

    if (debug)
    {
        snprintf(vmsg, sizeof(vmsg), "Executing prepared SQL %s", sql);
        DebugFunctionMsg(__FUNCTION__, vmsg);
    }

    sqlite3_stmt *stmt = NULL;
    for (i = 0; i < 3; i++)
    {
        retval = sqlite3_prepare_v2(handle, sql, -1, &stmt, 0);
        if (retval == SQLITE_OK)
        {
            break;
        }
        if (retval == SQLITE_LOCKED || retval == SQLITE_BUSY)
        {
            DebugFunctionMsg(__FUNCTION__, "Database LOCK! retry in .5s");
            AGITool_exec(&agi, &res, "Wait", "0.5");
        }
        else
        {
            break;
        }
    }

    if (retval != SQLITE_OK)
    {
        snprintf(vmsg, sizeof(vmsg), "Prepared SQL failed, retval=%i, sql=%s", retval, sql);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return pVal;
    }

    if (bind_count >= 1)
    {
        retval = sqlite3_bind_text(stmt, 1, arg1 ? arg1 : "", -1, SQLITE_TRANSIENT);
        if (retval != SQLITE_OK)
        {
            snprintf(vmsg, sizeof(vmsg), "Bind #1 failed, retval=%i, sql=%s", retval, sql);
            DebugFunctionMsg(__FUNCTION__, vmsg);
            sqlite3_finalize(stmt);
            return pVal;
        }
    }
    if (bind_count >= 2)
    {
        retval = sqlite3_bind_text(stmt, 2, arg2 ? arg2 : "", -1, SQLITE_TRANSIENT);
        if (retval != SQLITE_OK)
        {
            snprintf(vmsg, sizeof(vmsg), "Bind #2 failed, retval=%i, sql=%s", retval, sql);
            DebugFunctionMsg(__FUNCTION__, vmsg);
            sqlite3_finalize(stmt);
            return pVal;
        }
    }

    retval = sqlite3_step(stmt);

    if (debug)
    {
        snprintf(vmsg, sizeof(vmsg), "Prepared SQL returned %i columns", sqlite3_column_count(stmt));
        DebugFunctionMsg(__FUNCTION__, vmsg);
    }

    if (sqlite3_column_count(stmt) > 0)
    {
        for (i = 0; i < sqlite3_column_count(stmt); i++)
        {
            const unsigned char *t = sqlite3_column_text(stmt, i);
            strlcpy(rescols[i], t ? (const char *)t : "", sizeof(rescols[i]));
            if (debug)
            {
                snprintf(vmsg, sizeof(vmsg), "Prepared col[%i] value='%s'", i, rescols[i]);
                DebugFunctionMsg(__FUNCTION__, vmsg);
            }
        }
    }

    sqlite3_finalize(stmt);
    return pVal;
}

static char *sqlQueryBind1(const char *sql, const char *arg1)
{
    return sqlQueryBindInternal(sql, arg1, NULL, 1);
}

static char *sqlQueryBind2(const char *sql, const char *arg1, const char *arg2)
{
    return sqlQueryBindInternal(sql, arg1, arg2, 2);
}

/***********************************************************************
   Code for Queuemetrics outbound call statistics. This ia a straight
   replacement for the queuemetrics perl AGI. It opens and writes directly to the
   Asterisk queue log.

   Expects CMD, KEY, PARM_CLST (cluster shortuid), PM1, PM2, PM3 after the script name.

   Ast 1.6/1.8+ - probabaly won't work with Ast 1.4

EXAMPLE CALL
    exten => _X.,1,AGI(pbx3cagi,OutQmt,number,<cluster>,,Qname,agent)

PARAMS
          PARM_KEY 	number -> number to Dial
          PARM_CLST	cluster shortuid (tenant)
          PARM_PM1	Trunk -> Trunk (given by the Line column in trunks),
            this param is no longer used but must still be present in the parameter list
            (so that we don't break older calls).
          PARM_PM2	Qname -> name of the Queuemetrics queue
          PARM_PM3	agent -> suggest it should be  "Agent/+CLID" (e.g. Agent/4020)

***********************************************************************/

// void OutQmt(char* number, char* channel, char* queue, char* agent) {
void OutQmt()
{

    DebugFunctionTrace(__FUNCTION__);

    char buffer[1024] = {'\0'};
    char whohungup[16] = {'\0'};

    int nowstart, nowend, answeredtime, waittime, connecttime;

    time_t now;
    now = time(0);
    nowstart = now;
    //
    //  set abstimeout
    //
    snprintf(abstimeout, sizeof(abstimeout), "TIMEOUT(absolute)=%i", abstimeint);
    AGITool_exec(&agi, &res, "Set", abstimeout);
    //
    //    initial log entries for queuemetrics
    //
    snprintf(buffer, sizeof(buffer), "%d|%s|%s|NONE|ENTERQUEUE|-|%s", nowstart, uniqueid, PARM_PM2, PARM_KEY);
    QLogWrite(buffer);
    snprintf(buffer, sizeof(buffer), "%d|%s|%s|%s|CONNECT|0||", nowstart, uniqueid, PARM_PM2, PARM_PM3);
    QLogWrite(buffer);
    //
    // 	turn off HUP so we can run "deadagi" after the dial
    //
    if (signal(SIGHUP, sig_handler) == SIG_ERR)
    {
        snprintf(vmsg, sizeof(vmsg), "Cant catch SIGHUP!!");
        AGITool_verbose(&agi, &res, vmsg, 1);
    }

    signal(SIGHUP, SIG_IGN);
    //
    //    Do the Dial
    //
    strlcpy(extension, PARM_KEY, sizeof(extension));
    OutVoip(PARM_PM1);
    if (atoi(res.result))
    {
        strcpy(whohungup, "COMPLETEAGENT");
    }
    else
    {
        strcpy(whohungup, "COMPLETECALLER");
    }
    //
    //   Clean up and do the paperwork
    //
    now = time(0);
    nowend = now;
    AGITool_get_variable(&agi, &res, "ANSWEREDTIME"); //Asterisk varaiable set when a queue ends
    answeredtime = atoi(res.data);

    if (answeredtime == 0)
    {
        snprintf(buffer, sizeof(buffer), "%d|%s|%s|NONE|ABANDON|1|1", nowend, uniqueid, PARM_PM2);
        QLogWrite(buffer);
        return;
    }

    waittime = (nowend - nowstart) - answeredtime;
    connecttime = nowend - answeredtime;
    snprintf(buffer, sizeof(buffer), "%d|%s|%s|%s|CONNECT|%d|", connecttime, uniqueid, PARM_PM2, PARM_PM3, waittime);
    QLogWrite(buffer);
    snprintf(buffer, sizeof(buffer), "%d|%s|%s|%s|%s|%d|%d|", nowend, uniqueid, PARM_PM2, PARM_PM3, whohungup, waittime, answeredtime);
    QLogWrite(buffer);

    return;
}

void QLogWrite(char *buffer)
{

    DebugFunctionTrace(__FUNCTION__);

    FILE *qloghndl = NULL;
    size_t lrecl = 0;

    strlcat(buffer, "\n", sizeof(buffer));
    qloghndl = fopen(qlogFile, "a");
    if (qloghndl == NULL)
    {
        snprintf(vmsg, sizeof(vmsg), "Couldn't open queuelog for output!!");
        AGITool_verbose(&agi, &res, vmsg, 1);
        return;
    }
    lrecl = strlen(buffer);
    fwrite(buffer, lrecl, 1, qloghndl);
    fclose(qloghndl);

    //    if (buffer != NULL) {
    //       free(buffer);
    //    }

    return;
}
/*
    END OF code for Queuemetrics
*/
void outboundClip(char *key)
{
/*
 * N.B. Look at the new version of this code in 6.2
 *
 */
 
    DebugFunctionTrace(__FUNCTION__);

	char clidwork[MAX_EXT_LEN] = {'\0'};

	// backstop CLID if all else fails - take the trunk's CLID (if it exists)
	strcpy(clidline, sqlQueryBind1("SELECT callerid FROM trunks WHERE pkey=?", key));
	if (strcmp(clidline, "")) {
		strcpy(clidwork, clidline);
		sprintf (vmsg,"trunks CLID  %s found for outbound call, using key %s", clidwork, key);
		AGITool_verbose(&agi,&res,vmsg,1);
	}
	else {
		sprintf (vmsg,"No trunks CLID found for outbound call, using key %s", key);
		AGITool_verbose(&agi,&res,vmsg,1);
	}


	// If there is a cluster CLID then it trumps the line 
	if (strcmp(myClusterclid, "")) {
		sprintf (vmsg,"Cluster CLID %s found", myClusterclid);
		AGITool_verbose(&agi,&res,vmsg,1);
		strcpy(clidwork, myClusterclid);
	}
	else {
		sprintf (vmsg,"no cluster CLID found for outbound call, using key %s", key);
		AGITool_verbose(&agi,&res,vmsg,1);
	}


	//if there is an extension CLID or RDNIS CLID then it trumps the line and cluster CLID 
	if (caller_is_local) {
		strcpy(clidphone, sqlQueryBind1("SELECT callerid FROM IPphone WHERE pkey=?", callerid));		
	}
	// if the RDNIS is local, set its CLID into clidphone
	else if (rdnis_is_local) {
		strcpy(clidphone, sqlQueryBind1("SELECT callerid FROM IPphone WHERE pkey=?", rdnis));
	}

	// Only take the extension clid if it is longer than 5 characters (i.e. - not an extension number)
	// we do not want to send an extension number CLID onto the PSTN
	if (strcmp(clidphone, "") && (strlen(clidphone) > 5)) {
		strcpy(clidwork, clidphone);
		sprintf (vmsg,"Extension CLID %s found for outbound call, using key %s", clidwork, key);
		AGITool_verbose(&agi,&res,vmsg,1);
	}
	else {
		sprintf (vmsg,"No PSTN extension CLID found for outbound call, using clid %s", clidphone);
		AGITool_verbose(&agi,&res,vmsg,1);
	}

	sprintf (vmsg,"Phase 1 CLID is %s for outbound call, using key %s", clidwork, key);
	AGITool_verbose(&agi,&res,vmsg,1);

	// At this point we should have a CLID in clidwork if we don't then we should just go with what we were given 
	// because we can do no more.
	// We now have to decide whether to use the given CLID (in the case of a straight passthru) or the derived CLID 
	// in the case of a local caller.

/* This area needs to be reworked, it does not work for multi-tenant/multi-trunk systems
	it should be trunk specific not global so we need a new value in the trunks table to turn it on/off
	
	if (strcmp(clidline, "")) {
		if (caller_is_local) {
			strcpy(clidwork, clidline);
		}  
		else if (!strcmp(sqlSelectEq("globals", "pkey", "global", "CFWDEXTRNRULE"), "enabled")) {

		}
	}
*/
	// check we have a CLID to set
	if (strcmp(clidwork,"")) {
		// If this is a locally originated call then we can use the CLID we found
		if (caller_is_local) {
			sprintf (vmsg,"Using CLID %s for outbound call, using key %s", clidwork, key);
			AGITool_verbose(&agi,&res,vmsg,1);
			sprintf(clidstrng,"CALLERID(number)=%s",clidwork);
			AGITool_exec(&agi,&res,"Set", clidstrng);		
		
			// if the RDNIS is set (which covers local diversions) then reset it to the CLID to make it safe.  
			// It will look odd because the DIVERT header will be the same as the FROM header but it is necessary 
			// in the case of a local call which already has RDNIS set.  This will happen if the diversion has come from a phone
			// (rather than a pbx divert, e.g. *21*) and the phone has set the RDNIS to the original callerid.
		
			if (rdnis_is_set) {
				sprintf (vmsg,"Using CLID %s to override local RDNIS", clidwork);
				AGITool_verbose(&agi,&res,vmsg,1);
				sprintf(clidstrng,"CALLERID(RDNIS)=%s",clidwork);
				AGITool_exec(&agi,&res,"Set", clidstrng);
			}
		}
		// if the inbound callerid is a real number then this is a hairpin call so set the RDNIS to our CLID to create a 
		// diversion header.
		
		else {
			sprintf(clidstrng,"CALLERID(RDNIS)=%s",clidwork);
			AGITool_exec(&agi,&res,"Set", clidstrng);
		}
	}   
	else {
		// we have no CLID to set so just log it
		sprintf (vmsg,"No override CLID found for outbound call, using key %s", key);
		AGITool_verbose(&agi,&res,vmsg,1);
	}
	return;
}

void consoleMsg(char *vmsg, int level)
{
    if (debug >= level)
    {
        AGITool_verbose(&agi, &res, vmsg, 1);
    }
    return;
}
