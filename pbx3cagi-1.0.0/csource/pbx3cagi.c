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
#include <strings.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "pbx3cagi.h"
#include "agi_helpers.h"
#include "agi_wrap.h"
#include "cagi.h"
#include "bsd_compat.h"
#include <sys/wait.h>

AGI_TOOLS agi;
AGI_CMD_RESULT res;

const char *qlogFile = QLOG;
char abstimeout[32] = {'\0'};             // ABSTIMEOUT for any call
char eparm[64] = {'\0'};                  // Used by the Directory function (*57*)
char vmsg[255] = {'\0'};                  // console mesage buffer
char clidline[MAX_EXT_LEN] = {'\0'};      // CLI from trunk DB entry
char clidphone[MAX_EXT_LEN] = {'\0'};     // CLI from phone DB entry
char clidstrng[128] = {'\0'};             // CALLERID(…) set string (sized for prefix + CLID)
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

/* Phase 3: name macros removed — use g_call.* / g_parms.* (storage);
 * handlers/helpers prefer s->call / s->parms when s is in scope. */
agi_call_ctx_t g_call = {
    .myClusterContext = "qrxvtmny",
};
agi_parms_t g_parms;

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

/* Fleet Phase A: dial PSTN via fixed Egress trunk (no path failover on node). */
static int pbx3_fleet_mode(void)
{
    const char *env = getenv("PBX3_FLEET_MODE");
    if (env != NULL && env[0] != '\0')
    {
        if (!strcasecmp(env, "1") || !strcasecmp(env, "true") || !strcasecmp(env, "yes"))
        {
            return 1;
        }
        if (!strcasecmp(env, "0") || !strcasecmp(env, "false") || !strcasecmp(env, "no"))
        {
            return 0;
        }
    }
    {
        const char *active = sqlQueryBind1("SELECT active FROM trunks WHERE pkey=?", "Egress");
        if (active != NULL && !strcmp(active, "YES"))
        {
            return 1;
        }
    }
    return 0;
}

/**
 * Distinctive ring for outbound PJSIP: store Alert-Info value on an inherited
 * channel var; PrepDial attaches Dial b(pbx3-pre-dial) so PJSIP_HEADER runs on
 * the INVITE leg. Strips a leading "Alert-Info:" if the DB stored a full header.
 */
static void set_pbx3_alert_info(agi_session_t *s, const char *raw)
{
    char value[128] = {'\0'};
    const char *p;

    if (raw == NULL || raw[0] == '\0')
    {
        agi_set_variable(s, "__PBX3_ALERT_INFO", "");
        return;
    }
    p = raw;
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }
    if (strncasecmp(p, "Alert-Info:", 11) == 0)
    {
        p += 11;
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }
    }
    strlcpy(value, p, sizeof(value));
    agi_set_variable(s, "__PBX3_ALERT_INFO", value);
}


/* Phase 2.1/3 — command table; handlers take agi_session_t *. */
typedef void (*agi_cmd_fn)(agi_session_t *s);

typedef struct {
    int case_num;          /* historic switchdig / *NN feature code */
    const char *name;      /* named AGI cmd (NULL = feature-code only) */
    agi_cmd_fn handler;
} agi_cmd_entry_t;

static void cmd_OutTrunk(agi_session_t *s) { OutTrunk(s, PARM_KEY); }
static void cmd_Dial(agi_session_t *s) { PrepDial(s, PARM_KEY, PARM_PM1, "", ""); }
static void cmd_IVR(agi_session_t *s) { IVR(s, PARM_KEY); }
static void cmd_PrefixDial(agi_session_t *s) { PrefixDial(s); }

static const agi_cmd_entry_t agi_cmd_table[] = {
    { 1, "OutTrunk", cmd_OutTrunk },
    { 2, "OutRoute", OutRoute },
    { 3, "LepDial", LepDial },
    { 4, "Ingress", Ingress },
    { 5, "Dial", cmd_Dial },
    { 6, "IVR", cmd_IVR },
    { 7, "OutQmt", OutQmt },
    { 8, "PostDial", PostDial },
    { 9, "PrefixDial", cmd_PrefixDial },
    { 18, NULL, CFVMailSet },
    { 19, NULL, CFVMailSet },
    { 20, NULL, CFVMailToggle },
    { 21, NULL, CFToggle },
    { 22, NULL, CFToggle },
    { 23, NULL, CFOff },
    { 26, NULL, SetRingDelay },
    { 27, NULL, FollowMe },
    { 60, NULL, RecGreet },
    { 63, NULL, AgentPause },
    { 64, NULL, AgentUnpause },
    { 65, NULL, AgentLogin },
    { 66, NULL, AgentLogout },
    { 67, NULL, ChanSpyWhisper },
    { 68, NULL, ChanSpy },
};

static int agi_cmd_lookup_name(const char *name)
{
    size_t i;

    if (name == NULL) {
        return 0;
    }
    for (i = 0; i < sizeof(agi_cmd_table) / sizeof(agi_cmd_table[0]); i++) {
        if (agi_cmd_table[i].name != NULL && !strcmp(agi_cmd_table[i].name, name)) {
            return agi_cmd_table[i].case_num;
        }
    }
    return 0;
}

static void agi_cmd_dispatch(agi_session_t *s, int case_num)
{
    size_t i;

    for (i = 0; i < sizeof(agi_cmd_table) / sizeof(agi_cmd_table[0]); i++) {
        if (agi_cmd_table[i].case_num == case_num) {
            agi_cmd_table[i].handler(s);
            return;
        }
    }
    snprintf(vmsg, sizeof(vmsg), "Function-call %i does not exist in the core", case_num);
    DebugFunctionMsg(__FUNCTION__, vmsg);
}

/*
 * Phase 2.3 — AGI env + dialplan argv → call/tenant context.
 * Cluster from extensions.conf (PARM_CLST); writes *ctx (callers pass &g_call).
 */
void agi_init_call_context(agi_session_t *s, int argc, char **argv)
{
    agi_call_ctx_t *ctx = s->call;
    const char *log_cmd = "";
    const char *log_clst = "";

    g_parms.argv = argv;
    g_parms.argc = argc;

    strlcpy(ctx->uniqueid, AGITool_ListGetVal(s->agi->agi_vars, "agi_uniqueid"), sizeof(ctx->uniqueid));
    strlcpy(ctx->callerid, AGITool_ListGetVal(s->agi->agi_vars, "agi_callerid"), sizeof(ctx->callerid));
    strlcpy(ctx->calleridname, AGITool_ListGetVal(s->agi->agi_vars, "agi_calleridname"), sizeof(ctx->calleridname));
    strlcpy(ctx->context, AGITool_ListGetVal(s->agi->agi_vars, "agi_context"), sizeof(ctx->context));
    strlcpy(ctx->extension, AGITool_ListGetVal(s->agi->agi_vars, "agi_extension"), sizeof(ctx->extension));
    strlcpy(ctx->rdnis, AGITool_ListGetVal(s->agi->agi_vars, "agi_rdnis"), sizeof(ctx->rdnis));
    strlcpy(ctx->channel, AGITool_ListGetVal(s->agi->agi_vars, "agi_channel"), sizeof(ctx->channel));
    strlcpy(ctx->agi_dnid, AGITool_ListGetVal(s->agi->agi_vars, "agi_dnid"), sizeof(ctx->agi_dnid));

    if (isdigit((unsigned char)ctx->callerid[0])) {
        if (strlen(ctx->callerid) < 6) {
            ctx->caller_is_local = TRUE;
        } else if (strlen(ctx->callerid) == 8) {
            ctx->caller_is_local = TRUE;
        }
    }

    if (isdigit((unsigned char)ctx->extension[0])) {
        if (strlen(ctx->extension) < 6) {
            ctx->callee_is_local = TRUE;
        } else if (strlen(ctx->extension) == 8) {
            ctx->callee_is_local = TRUE;
        }
    }

    if (strcmp(ctx->rdnis, "unknown")) {
        if (strlen(ctx->rdnis) < 6) {
            ctx->rdnis_is_local = TRUE;
        }
        ctx->rdnis_is_set = TRUE;
    }

    /* GenAst passes tenant as AGI argv[3]; fall back to agi_context. */
    if (argc > 3 && argv[3] != NULL && argv[3][0] != '\0') {
        strlcpy(ctx->myCluster, argv[3], sizeof(ctx->myCluster));
    } else {
        strlcpy(ctx->myCluster, ctx->context, sizeof(ctx->myCluster));
    }

    strlcpy(ctx->myClusterContext, ctx->myCluster, sizeof(ctx->myClusterContext));
    if (!strcmp(ctx->myCluster, "default")) {
        strlcpy(ctx->myClusterContext, "qrxvtmny", sizeof(ctx->myClusterContext));
    }

    strlcat(setcdrcmd, ctx->myCluster, sizeof(setcdrcmd));
    agi_exec(s, "Set", setcdrcmd);

    if (argc > 1 && argv[1] != NULL) {
        log_cmd = argv[1];
    }
    if (argc > 3 && argv[3] != NULL) {
        log_clst = argv[3];
    }
    snprintf(vmsg, sizeof(vmsg),
             "Phase Main effective_cluster=%s agi_context=%s argv PARM_CMD=%s PARM_CLST=%s",
             ctx->myCluster, ctx->context, log_cmd, log_clst);
    DebugFunctionMsg(__FUNCTION__, vmsg);

    load_cluster_cfg(ctx->myCluster, &g_cluster_cfg);
    abstimeint = g_cluster_cfg.abstimeout_sec;
}

int main(int argc, char **argv)
{
    char numwork[32] = {'\0'};
    char chardig[4] = {'\0'};
    agi_session_t sess;

    AGITool_Init(&agi);

    AGITool_get_variable(&agi, &res, "DEBUG");  /* before sess; keep raw */
    if (!strcmp(res.data, "ON")) {
        debug = TRUE;
    }

    DebugFunctionTrace(__FUNCTION__);

    sess.call = &g_call;
    sess.parms = &g_parms;
    sess.agi = &agi;
    sess.res = &res;

    agi_init_call_context(&sess, argc, argv);

    /* No parameters: Queues "Local" backcall */
    if (argc == 1) {
        SetRecord(&sess, "Qexec", "Inbound");
        return 0;
    }
    if (argc < 2) {
        DebugFunctionMsg(__FUNCTION__, "Performed an AGI call without specifying a function.");
        return 1;
    }

    /* Feature codes (*NN): answer, settle, digitize. Else named command. */
    if (!strncmp(argv[1], "*", 1)) {
        agi_answer(&sess);
        agi_exec(&sess, "Wait", "0.5");
        strlcpy(numwork, argv[1], sizeof(numwork));
        strlcpy(chardig, numwork + 1, sizeof(chardig));
        g_parms.switchdig = atoi(chardig);
    } else {
        g_parms.switchdig = agi_cmd_lookup_name(argv[1]);
    }

    if (debug) {
        snprintf(vmsg, sizeof(vmsg), "switchdig is %i", g_parms.switchdig);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        snprintf(vmsg, sizeof(vmsg), "PARM_CMD is %s", PARM_CMD);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        if (argc > 2 && g_parms.argv[2] != NULL) {
            snprintf(vmsg, sizeof(vmsg), "PARM_KEY is %s", PARM_KEY);
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }
        if (argc > 3 && g_parms.argv[3] != NULL) {
            snprintf(vmsg, sizeof(vmsg), "PARM_CLST is %s", PARM_CLST);
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }
        if (argc > 4 && g_parms.argv[4] != NULL) {
            snprintf(vmsg, sizeof(vmsg), "PARM_PM1 is %s", PARM_PM1);
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }
        if (argc > 5 && g_parms.argv[5] != NULL) {
            snprintf(vmsg, sizeof(vmsg), "PARM_PM2 is %s", PARM_PM2);
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }
        if (argc > 6 && g_parms.argv[6] != NULL) {
            snprintf(vmsg, sizeof(vmsg), "PARM_PM3 is %s", PARM_PM3);
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }
        snprintf(vmsg, sizeof(vmsg), "Cluster is %s", sess.call->myCluster);
        DebugFunctionMsg(__FUNCTION__, vmsg);
    }

    setMoh(&sess);
    agi_cmd_dispatch(&sess, g_parms.switchdig);
    AGITool_Destroy(&agi);
    return 0;
}

/***********************************************************************
  END OF MAINLINE
************************************************************************/

void setMoh(agi_session_t *s)
{

    DebugFunctionTrace(__FUNCTION__);

    char setmohcmd[64] = "CHANNEL(musicclass)=moh-";
    char mohfolder[64] = "/usr/share/asterisk/moh-";
    char cmd[1024];
    int status, exitcode;

    strlcat(mohfolder, s->call->myCluster, sizeof(mohfolder));

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

    strlcat(setmohcmd, s->call->myCluster, sizeof(setmohcmd));
    agi_exec(s, "Set", setmohcmd);
    return;
}

void GetExt(char *out, size_t outsz, const char *number)
{

    DebugFunctionTrace(__FUNCTION__);

    snprintf(vmsg, sizeof(vmsg), "Trace Entered GetExt with %s", number ? number : "(null)");
    DebugFunctionMsg(__FUNCTION__, vmsg);

    get_ext_digits(out, outsz, number);

    snprintf(vmsg, sizeof(vmsg), "Trace returned from GetExt with %s", out ? out : "");
    DebugFunctionMsg(__FUNCTION__, vmsg);
}

void Mangle(agi_session_t *s, char *preSel, char *transformList, char *data,
            char *out, size_t outsz)
{

    DebugFunctionTrace(__FUNCTION__);

    char operand[MAX_NUM_LEN] = {'\0'};

    if (!strcmp(data, "CLI"))
    {
        strlcpy(operand, s->call->callerid, sizeof(operand));
    }
    else
    {
        strlcpy(operand, s->call->extension, sizeof(operand));
    }

    mangle_number(out, outsz, operand, preSel, transformList);
}

void RecGreet(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    int press = '2';
    char newGreetFile[MAX_FILENAME_LEN] = {'\0'};
    char tmpGreetFile[MAX_FILENAME_LEN] = {'\0'};
    char ext[MAX_EXT_LEN] = {'\0'};

    srand((unsigned int)time(NULL));
    int r = rand();

    snprintf(tmpGreetFile, sizeof(tmpGreetFile), "%s%s/ug%i", SOUNDIR, s->call->myCluster, r);

    GetExt(ext, sizeof(ext), PARM_CMD);
    snprintf(newGreetFile, sizeof(newGreetFile), "%s%s/usergreeting%s.wav", SOUNDIR, s->call->myCluster, ext);

    // check password
    if (Authenticate(s, "SYSPASS") != 0) //** syspass is held in the tenant table (used to be in Globals)
    {
        return;
    }
    // message to record
    agi_exec(s, "Playback", "pm-announcement-number");
    agi_exec(s, "SayDigits", ext);
    agi_exec(s, "Playback", "is-now-being-recorded");
    agi_exec(s, "Playback", "silence/1");
    agi_exec(s, "Playback", "press-pound-save-changes");

    // record message
    while (press == '2')
    {
        agi_record_file(s, tmpGreetFile, "wav", "#", 120000, 10, 10, 0);
        agi_stream_file(s, tmpGreetFile, "", 0);
        press = GetRecOption(s);
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
        agi_exec(s, "Playback", "your-msg-has-been-saved");
        agi_exec(s, "Playback", "goodbye");
        return;
    }

    remove(tmpGreetFile);
    agi_exec(s, "Playback", "cancelled");
}


int AuthenticatePassword(agi_session_t *s, const char *password_plain)
{

    DebugFunctionTrace(__FUNCTION__);
    char authbuf[64] = {'\0'};

    if (is_insecure_feature_pass(password_plain))
    {
        snprintf(vmsg, sizeof(vmsg),
                 "Refuse Authenticate: missing or insecure default feature password.");
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return -1;
    }

    strlcpy(authbuf, password_plain, sizeof(authbuf));
    agi_exec(s, "Playback", "silence/1");
    agi_exec(s, "Authenticate", authbuf);
    return atoi(s->res->result);
}

int Authenticate(agi_session_t *s, char *password)
{

    DebugFunctionTrace(__FUNCTION__);

    if (!strcmp(password, "SYSPASS"))
    {
        return AuthenticatePassword(s, g_cluster_cfg.syspass);
    }
    if (!strcmp(password, "SPYPASS"))
    {
        return AuthenticatePassword(s, g_cluster_cfg.spy_pass);
    }

    snprintf(vmsg, sizeof(vmsg), "Authenticate: unknown password key %s", password);
    DebugFunctionMsg(__FUNCTION__, vmsg);
    return -1;
}

int GetRecOption(agi_session_t *s)
{

    DebugFunctionTrace(__FUNCTION__);

    int count = 1;

    while (count < 3)
    {
        // When message has been recorded give options
        agi_stream_file(s, "save-announce-press", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        agi_stream_file(s, "digits/1", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        agi_stream_file(s, "to-rerecord-announce", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        agi_stream_file(s, "digits/2", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        agi_stream_file(s, "to-cancel-this-msg", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        agi_stream_file(s, "press", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        agi_stream_file(s, "digits/3", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        agi_stream_file(s, "silence/5", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        count++;
    }

    return '3';
}

/*
 * agent.queueN stores queue pkey; GenAst names the Asterisk queue by shortuid.
 * Returns 0 and fills ast_name on success; -1 to skip (None / missing).
 */
static int agent_resolve_queue_name(agi_session_t *s, const char *queue_pkey,
                                    char *ast_name, size_t ast_name_sz)
{
    const char *suid;

    if (ast_name == NULL || ast_name_sz == 0) {
        return -1;
    }
    ast_name[0] = '\0';
    if (queue_pkey == NULL || queue_pkey[0] == '\0' || !strcmp(queue_pkey, "None")) {
        return -1;
    }

    sqlQueryBind2("SELECT shortuid FROM queue WHERE pkey=? AND cluster=?",
                  queue_pkey, s->call->myCluster);
    suid = rescols[0];
    if (suid == NULL || suid[0] == '\0') {
        sqlQueryBind1("SELECT shortuid FROM queue WHERE pkey=?", queue_pkey);
        suid = rescols[0];
    }
    /* Defensive: already a shortuid in agent.queueN (mis-seeded rows). */
    if (suid == NULL || suid[0] == '\0') {
        sqlQueryBind1("SELECT shortuid FROM queue WHERE shortuid=?", queue_pkey);
        suid = rescols[0];
    }
    if (suid == NULL || suid[0] == '\0') {
        snprintf(vmsg, sizeof(vmsg), "Agent: no queue shortuid for pkey=%s cluster=%s",
                 queue_pkey, s->call->myCluster);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return -1;
    }
    strlcpy(ast_name, suid, ast_name_sz);
    return 0;
}

/* Queue members: Local/Q{ext}@tenant/n (PrepDial path; /n keeps Local for park/xfer). */
static void agent_member_local(char *buf, size_t bufsz, const char *ext, const char *context)
{
    snprintf(buf, bufsz, "Local/Q%s@%s/n", ext, context);
}

void AgentLogin(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    int finished = FALSE;                  // while control
    int i;                                 // control index
    char agent[8] = {'\0'};                // dtmf agent number
    char oldagent[8] = {'\0'};             // a dangling agent left logged in
    char agentpasswd[8] = {'\0'};          // dtmf agent passwd
    char queuearg[256] = {'\0'};           // argument for the AddQueueMember/RemoveQueuMember
    char agentqueue[32] = {'\0'};          // used to construct the queue column
    char queue_pkey[32] = {'\0'};          // agent.queueN value (queue pkey)
    char queuename[32] = {'\0'};           // Asterisk queue name (shortuid)
    char extenAgent[MAX_EXT_LEN] = {'\0'}; // holds exten if an agent is already logged in
    char agentchan[64] = {'\0'};           // Local/Q{ext}@{tenant}/n
    char remove_chan[64] = {'\0'};
    char remove_ext[MAX_EXT_LEN] = {'\0'};
    char buffer[1024] = {'\0'};            // QLOG buffer
    char epoch[32] = {'\0'};               // ${EPOCH}
    char startepoch[32] = {'\0'};          // ${EPOCH} saved from a previous login
    char f_eAgent[64] = {'\0'};
    char f_dAgent[64] = {'\0'};
    char f_dynLogin[64] = {'\0'};

    int timediff;

    snprintf(f_eAgent, sizeof(f_eAgent), "%s/eAgent", s->call->myClusterContext);
    snprintf(f_dAgent, sizeof(f_dAgent), "%s/dAgent", s->call->myClusterContext);
    snprintf(f_dynLogin, sizeof(f_dynLogin), "%s/DYNLOGIN", s->call->myClusterContext);

    agent_member_local(agentchan, sizeof(agentchan), s->call->callerid, s->call->myClusterContext);

    agi_get_data(s, "agent-user", 7000, 5);

    while (!finished)
    {
        if (atoi(s->res->result))
        {
            // check if it's a valid agent
            snprintf(agent, sizeof(agent), "%s", s->res->result);
            if (strcmp(sqlQueryBind1("SELECT pkey FROM agent WHERE pkey=?", agent), ""))
            {
                strlcpy(agentpasswd, sqlQueryBind1("SELECT passwd FROM agent WHERE pkey=?", agent), sizeof(agentpasswd));
                agi_exec(s, "Authenticate", agentpasswd);
                if (atoi(s->res->result) == 0)
                {
                    strlcpy(oldagent, DBGet(s, f_eAgent, s->call->callerid), sizeof(oldagent));
                    strlcpy(extenAgent, DBGet(s, f_dAgent, agent), sizeof(extenAgent));
                    if (!strcmp(oldagent, ""))
                    {
                        strlcpy(extenAgent, DBGet(s, f_dAgent, agent), sizeof(extenAgent));
                        strlcpy(oldagent, DBGet(s, f_eAgent, extenAgent), sizeof(oldagent));
                    }
                    if (strcmp(oldagent, ""))
                    {
                        strlcpy(startepoch, DBGet(s, f_dynLogin, oldagent), sizeof(startepoch));
                        agi_get_variable(s, "EPOCH"); //Asterisk system variable EPOCH
                        strlcpy(epoch, s->res->data, sizeof(epoch));
                        strlcpy(remove_ext, DBGet(s, f_dAgent, oldagent), sizeof(remove_ext));
                        if (remove_ext[0] == '\0') {
                            strlcpy(remove_ext, s->call->callerid, sizeof(remove_ext));
                        }
                        agent_member_local(remove_chan, sizeof(remove_chan), remove_ext,
                                           s->call->myClusterContext);
                        for (i = 1; i < 7; i++)
                        {
                            snprintf(agentqueue, sizeof(agentqueue), "queue%i", i);
                            snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM agent WHERE pkey=?", agentqueue);
                            strlcpy(queue_pkey, sqlQueryBind1(myQuery, oldagent), sizeof(queue_pkey));
                            if (agent_resolve_queue_name(s, queue_pkey, queuename, sizeof(queuename)) == 0)
                            {
                                snprintf(queuearg, sizeof(queuearg), "%s,%s", queuename, remove_chan);
                                agi_exec(s, "RemoveQueueMember", queuearg);
                            }
                        }
                        if (strcmp(DBGet(s, f_dynLogin, oldagent), ""))
                        {
                            timediff = atoi(epoch) - atoi(startepoch);
                        }
                        snprintf(buffer, sizeof(buffer), "%s|%s|NONE|Agent/%s|AGENTLOGOFF|-|%i", epoch, s->call->uniqueid, oldagent, timediff);
                        QLogWrite(buffer);
                        DBDel(s, f_dynLogin, oldagent);
                        DBDel(s, f_dAgent, oldagent);
                        DBDel(s, f_eAgent, extenAgent);
                        agi_exec(s, "Wait", "1");
                    }
                    for (i = 1; i < 7; i++)
                    {
                        snprintf(agentqueue, sizeof(agentqueue), "queue%i", i);
                        snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM agent WHERE pkey=?", agentqueue);
                        strlcpy(queue_pkey, sqlQueryBind1(myQuery, agent), sizeof(queue_pkey));
                        if (agent_resolve_queue_name(s, queue_pkey, queuename, sizeof(queuename)) == 0)
                        {
                            snprintf(queuearg, sizeof(queuearg), "%s,%s,,,Agent/%s",
                                     queuename, agentchan, agent);
                            agi_exec(s, "AddQueueMember", queuearg);
                        }
                    }
                    agi_get_variable(s, "EPOCH"); //Asterisk system variable EPOCH
                    strlcpy(epoch, s->res->data, sizeof(epoch));
                    snprintf(buffer, sizeof(buffer), "%s|%s|NONE|Agent/%s|AGENTLOGIN|%s", epoch, s->call->uniqueid, agent, agentchan);
                    QLogWrite(buffer);
                    if (!strcmp(DBGet(s, "DYNLOGIN", agent), ""))
                    {
                        DBPut(s, f_dynLogin, agent, epoch);
                    }
                    DBPut(s, f_dAgent, agent, s->call->callerid);
                    DBPut(s, f_eAgent, s->call->callerid, agent);
                    agi_exec(s, "Playback", "agent-loginok");
                    finished = TRUE;
                    continue;
                }
            }
        }
        agi_get_data(s, "agent-incorrect", 7000, 5);
    }
}
void AgentLogout(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    int i;                        // control index
    char agent[8] = {'\0'};       // agent number
    char agentpasswd[8] = {'\0'}; // dtmf agent passwd
    char queuearg[256] = {'\0'};  // argument for the AddQueueMember/RemoveQueuMember
    char agentqueue[32] = {'\0'}; // used to construct the queue column
    char queue_pkey[32] = {'\0'};
    char queuename[32] = {'\0'};  // Asterisk queue name (shortuid)
    char agentchan[64] = {'\0'};  // Local/Q{ext}@{tenant}/n
    char buffer[1024] = {'\0'};   // QLOG buffer
    char epoch[32] = {'\0'};      // ${EPOCH}
    char startepoch[32] = {'\0'}; // ${EPOCH} saved from login
    char f_eAgent[64] = {'\0'};
    char f_dAgent[64] = {'\0'};
    char f_dynLogin[64] = {'\0'};
    int timediff;

    snprintf(f_eAgent, sizeof(f_eAgent), "%s/eAgent", s->call->myClusterContext);
    snprintf(f_dAgent, sizeof(f_dAgent), "%s/dAgent", s->call->myClusterContext);
    snprintf(f_dynLogin, sizeof(f_dynLogin), "%s/DYNLOGIN", s->call->myClusterContext);

    agent_member_local(agentchan, sizeof(agentchan), s->call->callerid, s->call->myClusterContext);

    strcpy(agent, DBGet(s, f_eAgent, s->call->callerid));
    strlcpy(agentpasswd, sqlQueryBind1("SELECT passwd FROM agent WHERE pkey=?", agent), sizeof(agentpasswd));

    // check if they are logged in - if not just exit

    if (!strcmp(agent, ""))
    {
        agi_exec(s, "Playback", "agent-loggedoff");
        return;
    }

    agi_get_variable(s, "EPOCH"); //Asterisk system variable EPOCH
    strlcpy(epoch, s->res->data, sizeof(epoch));
    strlcpy(startepoch, DBGet(s, f_dynLogin, agent), sizeof(startepoch));
    //	agi_exec(s, "Authenticate",agentpasswd);
    //	if (atoi(s->res->result)==0) {
    for (i = 1; i < 7; i++)
    {
        snprintf(agentqueue, sizeof(agentqueue), "queue%i", i);
        snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM agent WHERE pkey=?", agentqueue);
        strlcpy(queue_pkey, sqlQueryBind1(myQuery, agent), sizeof(queue_pkey));
        if (agent_resolve_queue_name(s, queue_pkey, queuename, sizeof(queuename)) == 0)
        {
            snprintf(queuearg, sizeof(queuearg), "%s,%s", queuename, agentchan);
            agi_exec(s, "RemoveQueueMember", queuearg);
        }
    }
    if (strcmp(DBGet(s, f_dynLogin, agent), ""))
    {
        timediff = atoi(epoch) - atoi(startepoch);
    }
    snprintf(buffer, sizeof(buffer), "%s|%s|NONE|Agent/%s|AGENTLOGOFF|-|%i", epoch, s->call->uniqueid, agent, timediff);
    QLogWrite(buffer);
    DBDel(s, f_dynLogin, agent);
    DBDel(s, f_dAgent, agent);
    DBDel(s, f_eAgent, s->call->callerid);
    agi_exec(s, "Playback", "agent-loggedoff");
    //	}
}

void AgentPause(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char agent[8] = {'\0'};      // agent number
    char member[64] = {'\0'};
    char queuearg[256] = {'\0'}; // argument
    char f_eAgent[64] = {'\0'};

    snprintf(f_eAgent, sizeof(f_eAgent), "%s/eAgent", s->call->myClusterContext);
    strlcpy(agent, DBGet(s, f_eAgent, s->call->callerid), sizeof(agent));

    if (strcmp(agent, ""))
    {
        agent_member_local(member, sizeof(member), s->call->callerid, s->call->myClusterContext);
        snprintf(queuearg, sizeof(queuearg), ",%s", member);
        agi_exec(s, "PauseQueueMember", queuearg);
    }
    agi_exec(s, "Playback", "beep");
}

void AgentUnpause(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char agent[8] = {'\0'};      // agent number
    char member[64] = {'\0'};
    char queuearg[256] = {'\0'}; // argument
    char f_eAgent[64] = {'\0'};

    snprintf(f_eAgent, sizeof(f_eAgent), "%s/eAgent", s->call->myClusterContext);
    strlcpy(agent, DBGet(s, f_eAgent, s->call->callerid), sizeof(agent));

    if (strcmp(agent, ""))
    {
        agent_member_local(member, sizeof(member), s->call->callerid, s->call->myClusterContext);
        snprintf(queuearg, sizeof(queuearg), ",%s", member);
        agi_exec(s, "UnPauseQueueMember", queuearg);
    }
    agi_exec(s, "Playback", "beep");
}

// ChanSpy targets PJSIP/{shortuid} — channel names are never PJSIP/{pkey}.
// Resolve only within the calling tenant (PARM_CLST). No cross-cluster pkey
// fallback — that leaked spy onto other tenants sharing an extension number.
static int chanspy_resolve_endpoint(agi_session_t *s, const char *dialed_pkey,
                                   char *endpoint, size_t endpoint_sz)
{
    const char *suid;

    if (endpoint == NULL || endpoint_sz == 0) {
        return -1;
    }
    endpoint[0] = '\0';
    if (dialed_pkey == NULL || dialed_pkey[0] == '\0') {
        return -1;
    }

    sqlQueryBind2("SELECT shortuid FROM ipphone WHERE pkey=? AND cluster=?",
                  dialed_pkey, s->call->myCluster);
    suid = rescols[0];
    if (suid == NULL || suid[0] == '\0') {
        snprintf(vmsg, sizeof(vmsg),
                 "ChanSpy: pkey=%s not in cluster=%s (deny cross-tenant)",
                 dialed_pkey, s->call->myCluster);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return -1;
    }
    strlcpy(endpoint, suid, endpoint_sz);
    return 0;
}

void ChanSpyWhisper(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char ext[MAX_EXT_LEN] = {'\0'};
    char endpoint[MAX_EXT_LEN] = {'\0'};
    char options[64] = {'\0'};
    if (Authenticate(s, "SPYPASS") != 0) //** spy_pass is held in the tenant table (used to be in Globals)
    {
        return;
    }
 
    GetExt(ext, sizeof(ext), PARM_CMD);
    if (chanspy_resolve_endpoint(s, ext, endpoint, sizeof(endpoint)) != 0) {
        agi_exec(s, "Playback", "pbx-invalid");
        return;
    }
    snprintf(options, sizeof(options), "%s/%s,qw", SIPDRIVER, endpoint);
    agi_exec(s, "ChanSpy", options);
}

void ChanSpy(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char ext[MAX_EXT_LEN] = {'\0'};
    char endpoint[MAX_EXT_LEN] = {'\0'};
    char options[64] = {'\0'};
    if (Authenticate(s, "SPYPASS") != 0) //** spy_pass is held in the tenant table (used to be in Globals)
    {
        return;
    }
    GetExt(ext, sizeof(ext), PARM_CMD);
    if (chanspy_resolve_endpoint(s, ext, endpoint, sizeof(endpoint)) != 0) {
        agi_exec(s, "Playback", "pbx-invalid");
        return;
    }
    snprintf(options, sizeof(options), "%s/%s,q", SIPDRIVER, endpoint);
    agi_exec(s, "ChanSpy", options);
}


void OutRoute(agi_session_t *s)
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

    strlcpy(s->call->myClusterclid, g_cluster_cfg.clusterclid, sizeof(s->call->myClusterclid));

    /* cluster.dynamicfeatures is the Asterisk DYNAMIC_FEATURES string (replaces SET_DYNAMIC_FEATURES dialplan global). */
    if (g_cluster_cfg.dynamicfeatures[0] != '\0')
    {
        snprintf(dfbuf, sizeof(dfbuf), "__DYNAMIC_FEATURES=%s", g_cluster_cfg.dynamicfeatures);
        agi_exec(s, "Set", dfbuf);
    }

    
    /*
     * Cluster absolute timeout (tenant); 0 means barred.
     */
    if (g_cluster_cfg.abstimeout_sec == 0)
    {
        agi_exec(s, "Playtones", "busy");
        agi_exec(s, "Busy", "");
        return;
    }
    abstimeint = g_cluster_cfg.abstimeout_sec;

    /*
     * set timeout to the extenAbstimeout (if present)...
     * ...and check if the exten is barred (extenAbstimeout=0)
     *
     */
    if (s->call->caller_is_local)
    {
        sqlQueryBind2("SELECT abstimeout FROM ipphone WHERE shortuid=? AND cluster=?", s->call->extension, s->call->myCluster);
        strlcpy(extenAbstimeout, rescols[0], sizeof(extenAbstimeout));
        if (strcmp(extenAbstimeout, ""))
        {
            if (atoi(extenAbstimeout) == 0)
            {
                agi_exec(s, "Playtones", "busy");
                agi_exec(s, "Busy", "");
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
        snprintf(clusterGroup, sizeof(clusterGroup), "GROUP(%s)", s->call->myCluster);
        agi_set_variable(s, clusterGroup, s->call->myCluster);
        snprintf(clusterCount, sizeof(clusterCount), "GROUP_COUNT(%s)", s->call->myCluster);
        agi_get_variable(s, clusterCount); // clusterCount is the number of active outbound calls
        if (atoi(s->res->data) > atoi(g_cluster_cfg.chanmax_str))
        {
            agi_exec(s, "Playtones", "busy");
            agi_exec(s, "Busy", "");
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
        agi_exec(s, "Authenticate", eparm);
        if (atoi(s->res->result) != 0)
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
        agi_get_variable(s, PARM_KEY); // PARM_KEY is the path key - no change for PBX3
        last = atoi(s->res->data);
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
     * Fleet nodes: single Egress trunk to SBC — no multi-path failover (Phase A).
     */
    if (pbx3_fleet_mode())
    {
        strlcpy(path[0], "Egress", sizeof(path[0]));
        last = 0;
        strlcpy(active, sqlQueryBind1("SELECT active FROM trunks WHERE pkey=?", path[0]), sizeof(active));
        if (!strcmp(active, "YES") && strcmp(path[0], "None"))
        {
            OutVoip(s, path[0]);
        }
        return;
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
                agi_exec(s, "Set", setlast);
            }
            else if (strcmp(path[last], "None"))
            {
                OutVoip(s, path[last]);
            }
        }
    
        agi_get_variable(s, "DIALSTATUS"); // DIALSTATUS is the status of the call - answered, busy, cancelled
        if (!strcmp(s->res->data, "ANSWER"))
        {
            return;
        }
        if (!strcmp(s->res->data, "BUSY"))
        {
            if (!strncmp(g_cluster_cfg.playbusy, "YES", 3))
            {
                agi_exec(s, "Playtones", "busy");
                agi_exec(s, "Busy", "");
            }
            else
            {
                agi_exec(s, "Playback", "numb-dialled-busy");
                agi_exec(s, "Playback", "silence/1");
                agi_exec(s, "Playback", "please-try-again-later");
            }
            return;
        }

        if (!strcmp(s->res->data, "CANCEL"))
        {
            return;
        }

        if (!strncmp(g_cluster_cfg.playbeep, "YES", 3) && strcmp(path[last], "None"))
        {
            agi_exec(s, "Playback", "beep");
        }
        last++;
        if (last >= 3)
        {
            last = 0;
        }
    }   

    if (strcmp(alternate, "") && strcmp(alternate, s->call->extension))
    {
        agi_set_priority(s, 1);
        agi_set_extension(s, alternate);
        agi_set_context(s, s->call->myClusterContext);
        return;
    }

    if (!strncmp(g_cluster_cfg.playcongested, "YES", 3))
    {
        agi_exec(s, "Playtones", "congestion");
        agi_exec(s, "Congestion", "");
    }
    else
    {
        agi_exec(s, "Playback", "were-sorry");
        agi_exec(s, "Playback", "call-cannot-complete");
        agi_exec(s, "Playback", "please-hang-up-and-try-again");
    }
}


void OutTrunk(agi_session_t *s, char *key)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char active[4] = {'\0'};

    strlcpy(active, sqlQueryBind1("SELECT active FROM trunks WHERE pkey=?", PARM_KEY), sizeof(active));


    if (!strcmp(active, "YES"))
    {
        OutVoip(s, key);
        agi_get_variable(s, "DIALSTATUS"); // DIALSTATUS is the status of the call - answered, busy, cancelled
        if (!strcmp(s->res->data, "ANSWER"))
        {
        }
        else if (!strcmp(s->res->data, "BUSY"))
        {
            if (!strncmp(g_cluster_cfg.playbusy, "YES", 3))
            {
                agi_exec(s, "Playtones", "busy");
                agi_exec(s, "Busy", "");
            }
            else
            {
                agi_exec(s, "Playback", "numb-dialled-busy");
                agi_exec(s, "Playback", "silence/1");
                agi_exec(s, "Playback", "please-try-again-later");
            }
        }
        else
        {
            if (!strncmp(g_cluster_cfg.playcongested, "YES", 3))
            {
                agi_exec(s, "Playtones", "congestion");
                agi_exec(s, "Congestion", "");
            }
            else
            {
                agi_exec(s, "Playback", "were-sorry");
                agi_exec(s, "Playback", "call-cannot-complete");
                agi_exec(s, "Playback", "please-hang-up-and-try-again");
            }
        }
    }
}

/**
 * Tenant short dial (PrefixDial) — fleet only.
 * GenAst: exten => _81XXXX,1,agi(...,PrefixDial,81,{cluster},,,)  (remainder = dest ext_len)
 * DNID 811000 → remainder 1000 → Dial PJSIP/Egress/sip:1000@{target_fqdn}
 * CallerID num = phone_suid@calling_tenant_fqdn; name = "pkey human".
 * Rule 1: resolve target_fqdn from local dialalias row only (no live GK).
 */
static int prefixdial_digits_only(const char *s)
{
    size_t i;

    if (s == NULL || s[0] == '\0')
    {
        return 0;
    }
    for (i = 0; s[i] != '\0'; i++)
    {
        if (!isdigit((unsigned char)s[i]))
        {
            return 0;
        }
    }
    return 1;
}

/** Look up dest ext_len from local cluster (FQDN); 0 if unknown. */
static int prefixdial_dest_ext_len(const char *target_fqdn)
{
    const char *raw;
    int n;

    if (target_fqdn == NULL || target_fqdn[0] == '\0')
    {
        return 0;
    }
    raw = sqlQueryBind1(
        "SELECT ext_len FROM cluster WHERE lower(trim(fqdn)) = lower(trim(?)) LIMIT 1",
        target_fqdn);
    if (raw == NULL || rescols[0][0] == '\0')
    {
        return 0;
    }
    n = atoi(rescols[0]);
    if (n < 2 || n > 5)
    {
        return 0;
    }
    return n;
}

static void prefixdial_deny(agi_session_t *s)
{
    if (!strncmp(g_cluster_cfg.playcongested, "YES", 3))
    {
        agi_exec(s, "Playtones", "congestion");
        agi_exec(s, "Congestion", "");
    }
    else
    {
        agi_exec(s, "Playback", "were-sorry");
        agi_exec(s, "Playback", "call-cannot-complete");
        agi_exec(s, "Playback", "please-hang-up-and-try-again");
    }
}

void PrefixDial(agi_session_t *s)
{
    DebugFunctionTrace(__FUNCTION__);

    char prefix[MAX_PRESEL_LEN] = {'\0'};
    char dnid[MAX_EXT_LEN] = {'\0'};
    char remainder[MAX_EXT_LEN] = {'\0'};
    char active[8] = {'\0'};
    char target_fqdn[128] = {'\0'};
    char phone_suid[32] = {'\0'};
    char phone_pkey[MAX_EXT_LEN] = {'\0'};
    char phone_desc[128] = {'\0'};
    char dialString[MAX_DIALSTR_LEN] = {'\0'};
    char clidname[160] = {'\0'};
    char setclid[200] = {'\0'};
    const char *raw;

    if (!pbx3_fleet_mode())
    {
        DebugFunctionMsg(__FUNCTION__, "not fleet mode — deny");
        prefixdial_deny(s);
        return;
    }

    if (PARM_KEY == NULL || PARM_KEY[0] == '\0')
    {
        prefixdial_deny(s);
        return;
    }
    strlcpy(prefix, PARM_KEY, sizeof(prefix));
    if (!prefixdial_digits_only(prefix))
    {
        prefixdial_deny(s);
        return;
    }

    strlcpy(dnid, s->call->extension, sizeof(dnid));
    strlcpy(remainder, StripPreselect(prefix, dnid), sizeof(remainder));
    if (!prefixdial_digits_only(remainder))
    {
        DebugFunctionMsg(__FUNCTION__, "non-digit or empty remainder — deny");
        prefixdial_deny(s);
        return;
    }

    raw = sqlQueryBind2(
        "SELECT active, target_fqdn FROM dialalias WHERE pkey=? AND cluster=?",
        prefix,
        s->call->myCluster);
    if (raw == NULL || !strcmp(raw, "-1") || rescols[0][0] == '\0')
    {
        DebugFunctionMsg(__FUNCTION__, "dialalias row missing — deny");
        prefixdial_deny(s);
        return;
    }
    strlcpy(active, rescols[0], sizeof(active));
    strlcpy(target_fqdn, rescols[1], sizeof(target_fqdn));
    if (strcmp(active, "YES") || target_fqdn[0] == '\0')
    {
        DebugFunctionMsg(__FUNCTION__, "inactive or empty target_fqdn — deny");
        prefixdial_deny(s);
        return;
    }

    /* Wrong-length remainder when dest is local (GenAst already fixed-width; defense in depth). */
    {
        int dest_len = prefixdial_dest_ext_len(target_fqdn);
        size_t rem_len = strlen(remainder);
        if (dest_len > 0 && (int)rem_len != dest_len)
        {
            DebugFunctionMsg(__FUNCTION__, "wrong-length remainder — deny");
            prefixdial_deny(s);
            return;
        }
        if (dest_len == 0 && (rem_len < 2 || rem_len > 5))
        {
            DebugFunctionMsg(__FUNCTION__, "remainder length out of 2–5 — deny");
            prefixdial_deny(s);
            return;
        }
    }

    /* Resolve calling phone → shortuid + display fields (pkey then shortuid). */
    raw = sqlQueryBind2(
        "SELECT shortuid, pkey, description FROM ipphone WHERE cluster=? AND pkey=?",
        s->call->myCluster,
        s->call->callerid);
    if (raw == NULL || rescols[0][0] == '\0')
    {
        raw = sqlQueryBind2(
            "SELECT shortuid, pkey, description FROM ipphone WHERE cluster=? AND shortuid=?",
            s->call->myCluster,
            s->call->callerid);
    }
    if (raw != NULL && rescols[0][0] != '\0')
    {
        strlcpy(phone_suid, rescols[0], sizeof(phone_suid));
        strlcpy(phone_pkey, rescols[1], sizeof(phone_pkey));
        strlcpy(phone_desc, rescols[2], sizeof(phone_desc));
    }

    if (phone_pkey[0] != '\0')
    {
        /*
         * Site Group CLIP try-out: presentation = {caller routing_prefix}{ext}
         * so peer redial is PrefixDial digits (destination-owned prefix mesh).
         * Resolve own prefix from a co-located managed dialalias that targets us
         * (same SQLite as peers on this home). Fallback: bare extension (pre-cohort).
         * PAI / return AoR stays suid@fqdn (§3.9 Path 1).
         */
        char own_prefix[16] = {'\0'};
        char clip_num[48] = {'\0'};

        if (g_cluster_cfg.fqdn[0] != '\0')
        {
            raw = sqlQueryBind1(
                "SELECT pkey FROM dialalias WHERE source='cohort' AND target_fqdn=? "
                "AND pkey IS NOT NULL AND trim(pkey) != '' LIMIT 1",
                g_cluster_cfg.fqdn);
            if (raw != NULL && rescols[0][0] != '\0' && prefixdial_digits_only(rescols[0]))
            {
                strlcpy(own_prefix, rescols[0], sizeof(own_prefix));
            }
        }

        if (own_prefix[0] != '\0')
        {
            snprintf(clip_num, sizeof(clip_num), "%s%s", own_prefix, phone_pkey);
            snprintf(setclid, sizeof(setclid), "CALLERID(number)=%s", clip_num);
        }
        else
        {
            snprintf(setclid, sizeof(setclid), "CALLERID(number)=%s", phone_pkey);
        }
        agi_exec(s, "Set", setclid);
    }
    else if (phone_suid[0] != '\0')
    {
        snprintf(setclid, sizeof(setclid), "CALLERID(number)=%s", phone_suid);
        agi_exec(s, "Set", setclid);
    }
    if (phone_pkey[0] != '\0' || phone_desc[0] != '\0')
    {
        if (phone_desc[0] != '\0' && phone_pkey[0] != '\0')
        {
            snprintf(clidname, sizeof(clidname), "%s %s", phone_pkey, phone_desc);
        }
        else if (phone_desc[0] != '\0')
        {
            strlcpy(clidname, phone_desc, sizeof(clidname));
        }
        else
        {
            strlcpy(clidname, phone_pkey, sizeof(clidname));
        }
        snprintf(setclid, sizeof(setclid), "CALLERID(name)=\"%s\"", clidname);
        agi_exec(s, "Set", setclid);
    }

    /*
     * Network return AoR (§3.9) on the *outbound* PJSIP channel via Dial b()
     * (setting PJSIP_HEADER on Local never sticks). The SBC keeps this PAI
     * and rewrites From → sitedial for home identify. Presentation num is
     * routing_prefix+ext when Site Group mesh is present (else bare pkey).
     */
    if (phone_suid[0] != '\0' && g_cluster_cfg.fqdn[0] != '\0')
    {
        char aor[160] = {'\0'};

        snprintf(aor, sizeof(aor), "%s@%s", phone_suid, g_cluster_cfg.fqdn);
        agi_set_variable(s, "__PBX3_RETURN_AOR", aor);
    }

    /*
     * Via SbcSiteOut{calling-tenant} so From domain is caller tenant FQDN.
     * R-URI = ext@target tenant FQDN for SBC miss→dispatcher.
     */
    {
        char endpoint[80] = {'\0'};
        char opts[64] = {'\0'};

        snprintf(endpoint, sizeof(endpoint), "SbcSiteOut%s", s->call->myCluster);
        snprintf(dialString, sizeof(dialString), "%s/%s/sip:%s@%s",
                 SIPDRIVER, endpoint, remainder, target_fqdn);
        /* b() runs on the outbound PJSIP leg before INVITE */
        strlcpy(opts, "b(pbx3-pre-dial^s^1)", sizeof(opts));
        if (!strcmp(g_cluster_cfg.allowhashxfer, "enabled") && s->call->caller_is_local)
        {
            strlcat(opts, "T", sizeof(opts));
        }
        strlcat(dialString, ASTDLIM, sizeof(dialString));
        strlcat(dialString, ASTDLIM, sizeof(dialString));
        strlcat(dialString, opts, sizeof(dialString));
    }

    agi_set_variable(s, "__PBX3_SITE_DIAL", "YES");
    agi_exec(s, "Dial", dialString);
    agi_get_variable(s, "DIALSTATUS");
    if (!strcmp(s->res->data, "ANSWER"))
    {
        return;
    }
    if (!strcmp(s->res->data, "BUSY"))
    {
        if (!strncmp(g_cluster_cfg.playbusy, "YES", 3))
        {
            agi_exec(s, "Playtones", "busy");
            agi_exec(s, "Busy", "");
        }
        else
        {
            agi_exec(s, "Playback", "numb-dialled-busy");
            agi_exec(s, "Playback", "silence/1");
            agi_exec(s, "Playback", "please-try-again-later");
        }
        return;
    }
    prefixdial_deny(s);
}

void OutVoip(agi_session_t *s, char *key)
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

    /* trunks.description (not legacy "desc") — peername falls back to description if blank. */
    sqlQueryBind1("SELECT username,peername,callprogress,description,transform,match,technology FROM trunks WHERE pkey=?", key);
    strlcpy(username, rescols[0], sizeof(username));
    strlcpy(peername, rescols[1], sizeof(peername));
    strlcpy(callprogress, rescols[2], sizeof(callprogress));
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
        Mangle(s, preSel, transform, "", number, sizeof(number));
    }
    else
    {
        strlcpy(number, s->call->extension, sizeof(number));
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
        // if this is a sx to sx trunk (legacy sister-site trunk tech InterSARK) then leave the clip as the extension number even if there
        // are overrides.  This caters for inter site calls where the caller wants to send their extension number even
        // though they normally send a DDI on an outbound call.
        if (strcmp(technology, "SailToSail") && strcmp(technology, "InterSARK"))
    {
        outboundClip(s, PARM_KEY);
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
    if (s->call->rdnis_is_set)
    {
        if (!strcmp(g_cluster_cfg.cfwd_progress, "enabled"))
        {
            strlcat(dialString, "r", sizeof(dialString));
        }
        if (!strcmp(g_cluster_cfg.cfwd_answer, "enabled"))
        {
            agi_answer(s);
        }
    }

    agi_set_variable(s, "GROUP()", "OUTBOUND_GROUP");
    agi_get_variable(s, "GROUP_COUNT()"); // GROUP_COUNT() is the number of active outbound calls
    if (atoi(s->res->data) <= atoi(g_cluster_cfg.voipmax_str))
    {
        strlcpy(recRet, SetRecord(s, s->call->callerid, "Outbound"), sizeof(recRet));
        strlcat(dialString, recRet, sizeof(dialString));
        //  Abs timeout
        agi_exec(s, "Set", abstimeout);
        //  acounting
        // setOutbound_cdr_userfield();
        // Dial
        agi_exec(s, "Dial", dialString);
    }
}

void LepDial(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char vmbox[64] = {'\0'};
    char blindtransfer[MAX_EXT_LEN] = {'\0'};
    char cellphone[MAX_EXT_LEN] = {'\0'};
    char celltwindial[MAX_EXT_LEN] = {'\0'};
    char *pBtr = &blindtransfer[4];
    char vmflags[4] = {'\0'};
    char calledCluster[MAX_CLUSTER_LEN] = {'\0'};
    char extalert[128] = {'\0'};
    char transferer[MAX_EXT_LEN] = {'\0'};

    /* Phase G: dialplan gates Dial on non-empty PBX3_DIAL — clear so CFIM/VM early exits skip Dial. */
    agi_set_variable(s, "PBX3_DIAL", "");

    agi_get_variable(s, "BLINDTRANSFER");  //set in extensions.conf
    strlcpy(blindtransfer, s->res->data, sizeof(blindtransfer));

    sqlQueryBind2("SELECT dvrvmail,extalert,cluster FROM ipphone WHERE shortuid=? AND cluster=?", s->call->extension, s->call->myCluster);

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
    if (!strcmp(DBGet(s, "cfim", s->call->extension), s->call->extension))
    {
        if (!strcmp(vmbox, "None"))
        {
            if (strcmp(blindtransfer, ""))
            {
                if (strcmp(s->call->agi_dnid, s->call->extension))
                {
                    agi_exec(s, "Playback", "silence/1");
                    if (!strcmp(g_cluster_cfg.playtransfer, "YES"))
                    {
                        agi_exec(s, "Playback", "pls-hold-while-try");
                    }
                    strlcpy(transferer, pBtr, sizeof(transferer));
                    agi_set_priority(s, 1);
                    agi_set_extension(s, transferer);
                    agi_set_context(s, s->call->myClusterContext);
                    return;
                }
            }
            else
            {
                agi_exec(s, "Playtones", "congestion");
                agi_exec(s, "congestion", "");
                return;
            }
        }
        else
        {
            //            agi_exec(s, "Playback","silence/1");
            strlcat(vmflags, "u", sizeof(vmflags));
            strlcat(vmbox, vmflags, sizeof(vmbox));
            agi_exec(s, "Voicemail", vmbox);
            return;
        }
    }
    /*
     *  check and send PBX forwards
     */
    if (!CFCheck(s, "cfim", s->call->extension))
    {
        if (debug)
        {
            snprintf(vmsg, sizeof(vmsg), "Trace left CFCheck with extension=%s",s->call->extension); 
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }

        return;
    }

/**
 *  Distinctive ring / Alert-Info on the outbound PJSIP INVITE via Dial b().
 */
    if (strcmp(extalert, ""))
    {
        set_pbx3_alert_info(s, extalert);
    }

 /**
  *  set a pickup mark for directed call pickup
  */
    agi_set_variable(s, "__PICKUPMARK", s->call->extension);

/**
  *  Add a twin if we have one enabled
  */
    strlcpy(cellphone, DBGet(s, "srktwin", s->call->extension), sizeof(cellphone));
    if (strcmp(cellphone, ""))
    {
        strlcpy(celltwindial, "&Local/", sizeof(celltwindial));
        strlcat(celltwindial, cellphone, sizeof(celltwindial));
        strlcat(celltwindial, "@", sizeof(celltwindial));
        strlcat(celltwindial, s->call->myClusterContext, sizeof(celltwindial));
    }
    /* Phase G: PrepDial sets PBX3_DIAL and returns — dialplan Dial owns the bridge. */
    PrepDial(s, s->call->extension, "", celltwindial, vmbox);
    return;
}

/**
 *  Phase G — after dialplan Dial(${PBX3_DIAL}).
 *  Former LepDial post-bridge policy. Dialplan skips this AGI on ANSWER/CANCEL
 *  (dead-AGI cold-start avoidance); still no-op those statuses defensively.
 */
void PostDial(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char vmbox[64] = {'\0'};
    char blindtransfer[MAX_EXT_LEN] = {'\0'};
    char transferer[MAX_EXT_LEN] = {'\0'};
    char *pBtr = &blindtransfer[4];
    char calleridsave[MAX_EXT_LEN] = {'\0'};
    char dstatus[16] = {'\0'};
    char vmflags[4] = {'\0'};
    char calledCluster[MAX_CLUSTER_LEN] = {'\0'};
    char dialed[MAX_EXT_LEN] = {'\0'};

    if (g_parms.argc > 2 && g_parms.argv[2] != NULL && PARM_KEY[0] != '\0')
    {
        strlcpy(dialed, PARM_KEY, sizeof(dialed));
    }
    else
    {
        strlcpy(dialed, s->call->extension, sizeof(dialed));
    }

    agi_get_variable(s, "BLINDTRANSFER");
    strlcpy(blindtransfer, s->res->data, sizeof(blindtransfer));

    sqlQueryBind2("SELECT dvrvmail,cluster FROM ipphone WHERE shortuid=? AND cluster=?", dialed, s->call->myCluster);
    strlcpy(vmbox, rescols[0], sizeof(vmbox));
    strlcpy(calledCluster, rescols[1], sizeof(calledCluster));

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

    agi_get_variable(s, "DIALSTATUS");
    strcpy(dstatus, s->res->data);
    if (!strcmp(dstatus, "ANSWER") || !strcmp(dstatus, "CANCEL"))
    {
        return;
    }

    if (!CFCheck(s, "cfbs", dialed))
    {
        return;
    }

    if (!strcmp(dstatus, "NOANSWER"))
    {
        if (!strcmp(vmbox, "None"))
        {
            if (strcmp(blindtransfer, ""))
            {
                if (strcmp(s->call->agi_dnid, dialed))
                {
                    if (g_cluster_cfg.bounce_alert[0] != '\0')
                    {
                        set_pbx3_alert_info(s, g_cluster_cfg.bounce_alert);
                    }
                    strlcpy(calleridsave, s->call->callerid, sizeof(calleridsave));
                    strlcpy(s->call->callerid, "R", sizeof(s->call->callerid));
                    strlcat(s->call->callerid, calleridsave, sizeof(s->call->callerid));
                    agi_set_callerid(s, s->call->callerid);
                    strlcpy(transferer, pBtr, sizeof(transferer));
                    agi_set_priority(s, 1);
                    agi_set_extension(s, transferer);
                    agi_set_context(s, s->call->myClusterContext);
                    return;
                }
            }
        }
        else
        {
            strlcat(vmflags, "u", sizeof(vmflags));
            strlcat(vmbox, vmflags, sizeof(vmbox));
            agi_exec(s, "Voicemail", vmbox);
            return;
        }
    }
    if (!strcmp(vmbox, "None"))
    {
        if (strcmp(blindtransfer, ""))
        {
            if (strcmp(s->call->agi_dnid, dialed))
            {
                agi_exec(s, "Playback", "silence/1");
                if (!strcmp(g_cluster_cfg.playtransfer, "YES"))
                {
                    agi_exec(s, "Playback", "pls-hold-while-try");
                }

                if (g_cluster_cfg.bounce_alert[0] != '\0')
                {
                    set_pbx3_alert_info(s, g_cluster_cfg.bounce_alert);
                }
                strlcpy(transferer, pBtr, sizeof(transferer));
                agi_set_priority(s, 1);
                agi_set_extension(s, transferer);
                agi_set_context(s, s->call->myClusterContext);
                return;
            }
            else
            {
                if (g_cluster_cfg.blind_busy[0] != '\0')
                {
                    agi_set_priority(s, 1);
                    agi_set_extension(s, g_cluster_cfg.blind_busy);
                    agi_set_context(s, s->call->myClusterContext);
                    return;
                }
                else
                {
                    agi_exec(s, "Playtones", "busy");
                    agi_exec(s, "Busy", "");
                    return;
                }
            }
        }
        else
        {
            agi_exec(s, "Playtones", "busy");
            agi_exec(s, "Busy", "");
            return;
        }
    }
    else
    {
        strlcat(vmflags, "b", sizeof(vmflags));
        strlcat(vmbox, vmflags, sizeof(vmbox));
        agi_exec(s, "Voicemail", vmbox);
        return;
    }

    agi_exec(s, "Playtones", "busy");
    agi_exec(s, "Busy", "");
    return;
}

void PrepDial(agi_session_t *s, char *number, char *type, char *twin, char *vmbox)
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
    strlcat(setcdrcmduser, s->call->agi_dnid, sizeof(setcdrcmduser));
    agi_exec(s, "Set", setcdrcmduser);

/**
 *  begin to set up the dialstring.
 *  Fleet only: keep tenant domain in the Request-URI so SBC usrloc is
 *  domain-aware (Dial(PJSIP/shortuid) alone uses AOR contact).
 *  Singleton (no SBC) keeps Dial(PJSIP/shortuid) and uses the registered contact.
 *
 *  WebRTC via edge WSS (W1): SIP.js registers Contact @192.0.2.x;transport=wss
 *  (RFC dummy) while OpenSIPS holds the real WSS connection in location.
 *  Dial(PJSIP/shortuid) alone → "No route to destination" → VM. Always use
 *  sip:shortuid@tenant.fqdn in fleet mode so invite hits SBC lookup (same as desks).
 *  Direct instance :8089 WSS (no SBC REGISTER) remains singleton Dial without FQDN.
 *
 *  Site-dial receive (§3.9 / D): when __PBX3_RETURN_AOR is set, dial via
 *  endpoint SiteRing (send_pai=no) so dialplan PAI (return AoR) is not
 *  overwritten by CALLERID presentation (extension digits).
 *
 *  Slice B hairpin guard: SbcDomainRoute sets __PBX3_SITE_DIAL=YES on digit
 *  R-URI arrivals (usrloc miss → dispatcher → home). Lab already proved
 *  no Contact — do NOT Dial sip:user@tenant.fqdn again (loops). Local
 *  Dial(PJSIP/shortuid) → CHANUNAVAIL → PostDial → voicemail/CFBS.
 *  Carrier DID Ingress does not set SITE_DIAL; first FQDN Dial still runs.
 */
    {
        char ring_ep[80] = {'\0'};
        int site_return = 0;
        int site_dial_inbound = 0;

        agi_get_variable(s, "PBX3_SITE_DIAL");
        if (s->res->data[0] != '\0' && strcmp(s->res->data, "(null)") != 0 &&
            strcmp(s->res->data, "YES") == 0)
        {
            site_dial_inbound = 1;
        }

        agi_get_variable(s, "PBX3_RETURN_AOR");
        if (pbx3_fleet_mode() && s->res->data[0] != '\0' &&
            strcmp(s->res->data, "(null)") != 0)
        {
            site_return = 1;
            strlcpy(ring_ep, "SiteRing", sizeof(ring_ep));
        }
        else
        {
            strlcpy(ring_ep, number, sizeof(ring_ep));
        }

        strlcpy(dialString, SIPDRIVER, sizeof(dialString));
        strlcat(dialString, "/", sizeof(dialString));
        strlcat(dialString, ring_ep, sizeof(dialString));
        /* Fleet FQDN Dial only on first attempt (not Slice B / hairpin re-entry). */
        if (pbx3_fleet_mode() && g_cluster_cfg.fqdn[0] != '\0' && !site_dial_inbound)
        {
            strlcat(dialString, "/sip:", sizeof(dialString));
            strlcat(dialString, number, sizeof(dialString));
            strlcat(dialString, "@", sizeof(dialString));
            strlcat(dialString, g_cluster_cfg.fqdn, sizeof(dialString));
        }
        else if (site_dial_inbound && debug)
        {
            snprintf(vmsg, sizeof(vmsg),
                     "PrepDial SITE_DIAL inbound — local Dial only (no FQDN hairpin) for %s",
                     number);
            DebugFunctionMsg(__FUNCTION__, vmsg);
        }
        /* stash flag for option append below */
        if (site_return)
        {
            agi_set_variable(s, "PBX3_SITE_RING", "YES");
        }
        else
        {
            agi_set_variable(s, "PBX3_SITE_RING", "");
        }
    }

/**
 *  Set the ring timeout for everything but dials coming in off the 
 *  Queues subsystem - they set their own
 */

    if (strcmp(type, "queue"))
    {
        strlcpy(userRingDelay, DBGet(s, "ringdelay", number), sizeof(userRingDelay));
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
        if (s->call->caller_is_local)
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
 *      Queue dial — empty timeout (Queue owns ring time) but keep cktT so the
 *      agent leg can park (*5 / featuremap) and transfer after answer; Dial c
 *      sets HANGUPCAUSE answered-elsewhere when Queue cancels the ring.
 *      Pair with GenAst member=Local/…/n so Local is not optimized out of the
 *      Queue bridge.
 */
    {
        strlcat(dialString, ASTDLIM, sizeof(dialString));
        strlcat(dialString, ASTDLIM, sizeof(dialString));
        strlcat(dialString, "cktT", sizeof(dialString));
    }

/**
 *  stop queue dial-legs (which do come through here) from firing off "ghost" recordings
 */
    if (strcmp(type, "queue"))
    {
        strlcpy(recRet, SetRecord(s, number, "Inbound"), sizeof(recRet));
        strlcat(dialString, recRet, sizeof(dialString));
    }
/**
 *  If the dial definition has asked for MOH instead of ringback then set it here
 */
    agi_get_variable(s, "MOH"); //set ON/OFF in inbound routes from the dial definition
    if (!strcmp(s->res->data, "YES"))
    {
        strlcat(dialString, "m", sizeof(dialString));
    }
/**
 *  Tenant short dial (§3.9 / slice D) PAI and/or distinctive-ring Alert-Info:
 *  one Dial b() gosub on the outbound PJSIP channel before INVITE.
 */
    {
        int need_predial = 0;

        agi_get_variable(s, "PBX3_SITE_RING");
        if (!strcmp(s->res->data, "YES"))
        {
            need_predial = 1;
        }
        agi_get_variable(s, "PBX3_ALERT_INFO");
        if (s->res->data[0] != '\0' && strcmp(s->res->data, "(null)") != 0)
        {
            need_predial = 1;
        }
        if (need_predial && strstr(dialString, "pbx3-pre-dial") == NULL &&
            strstr(dialString, "pbx3-site-pai") == NULL)
        {
            if (strcmp(type, "queue") == 0 && strstr(dialString, ",,") == NULL)
            {
                strlcat(dialString, ASTDLIM, sizeof(dialString));
                strlcat(dialString, ASTDLIM, sizeof(dialString));
            }
            strlcat(dialString, "b(pbx3-pre-dial^s^1)", sizeof(dialString));
        }
    }
/**
 *  Phase E (queue) + Phase G (LepDial): decide only — set PBX3_DIAL for
 *  dialplan Dial(${PBX3_DIAL}). Short-run AGI; dialplan owns the bridge.
 */
    agi_set_variable(s, "PBX3_DIAL", dialString);
    return;
}

char * SetRecord(agi_session_t *s, char *key, char *compass)
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
        sqlQueryBind2("SELECT devicerec FROM Queue WHERE pkey=? AND cluster=?", s->call->agi_dnid, s->call->myCluster);
        strlcpy(devicerec, rescols[0], sizeof(devicerec));        
    }

    snprintf(vmsg, sizeof(vmsg), "devicerec is %s",devicerec);
    DebugFunctionMsg(__FUNCTION__, vmsg);
/**
 *  device recording is off - exit
 */
    if (!strcmp(devicerec, "None"))
    {
        return pdial;
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
        snprintf(filework, sizeof(filework), "%d-%s-%s-%s", (int)time(&now), s->call->myCluster, s->call->agi_dnid, s->call->callerid);
        strlcat(filename, filework, sizeof(filename));

        /**
         * FILE refs MUST BE CHANGED *DONE*
         */
        snprintf(soundFile, sizeof(soundFile), " /var/spool/asterisk/monitor/%s/%s.wav", s->call->myCluster, filename);
        agi_exec(s, "MixMonitor", soundFile);

    }
    return pdial;
}

/**
 *  Checks for a call forward and actions it.
 *  type - cfbs or cfim
 *  number - number to check
 */
char * CFCheck(agi_session_t *s, char *type, char *number)
{

    DebugFunctionTrace(__FUNCTION__);

    //    char cflist[MAX_CALL_FWD_CHAIN_LEN][MAX_FWD_NUM_LEN] = {{'\0'}};
    char cfnum[MAX_FWD_NUM_LEN] = {'\0'};
    //    int i, cnt=0;
    char rdnis_string[128] = {'\0'};
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

    if (strcmp(DBGet(s, type, number), ""))
/**
 *  We have a forward set...
 */
    {
        strlcpy(cfnum, DBGet(s, type, number), sizeof(cfnum));
/* Is this DND? */
        if (!strcmp(cfnum,s->call->agi_dnid)) 
        {
/**
 *  then go to voicemail
 */
            snprintf(vmbox, sizeof(vmbox), "%s@%s%s", s->call->agi_dnid, s->call->myCluster, vmflags);
            agi_exec(s, "Voicemail",vmbox);
			return NULL;
        }
/**
 *  Is this a local divert or are we heading out to the PSTN?
 *  Use forward target (cfnum), not AstDB key (number — shortuid or pkey).
 */
        if (strlen(cfnum) > 5)
/**
 *      Our forward is not local.  The default behaviour is to play a comfort message at this point
 *      to cover the uncertainty of a possible audio pause while the upstream is switching
 *      You can turn this behaviour off if you wish by setting the cluster control variable PLAYTRANSFER to false. 
 */
        {
            agi_exec(s, "Playback", "silence/1");
            if (!strcmp(g_cluster_cfg.playtransfer, "YES"))
            {
                agi_exec(s, "Playback", "pls-hold-while-try");
            }
        }
/**
 *      Forwarding to a local extension...
 */
        

/**
 *  Set the rdnis to the original CLI 
 */
    if (strcmp(s->call->rdnis,"unknown"))
    {
        snprintf(rdnis_string, sizeof(rdnis_string), "CALLERID(rdnis)=%s", s->call->callerid);
        agi_exec(s, "Set", rdnis_string);
    }
/**
 *  and branch...
 */
        agi_set_priority(s, 1);
        agi_set_extension(s, cfnum);
        agi_set_context(s, s->call->myClusterContext);
        return NULL;
    }
    return number;
}
/// @brief CFToggle - set a call forward in AstDB.  
/**
 *  e.g. *21*1104 - sets a CFIM or CFBS to extension 1104
 *       *21* cancels any forwards
*/
void CFToggle(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char toNum[32] = {'\0'};
    char *technology;
    char *sipId;

/**
 *  Get the SIP endpoint ID from the channel variable
*/
    technology = strtok(s->call->channel,"/");
    sipId = strtok(NULL,"-");

    GetExt(toNum, sizeof(toNum), PARM_CMD);

    DBPut(s, PARM_KEY, sipId, toNum);

    agi_exec(s, "Playback", "call-forwarding");
    if (strcmp(toNum, ""))
    {
        agi_exec(s, "Playback", "activated");
    }
    else
    {
        agi_exec(s, "Playback", "de-activated");
    }
}

void CFVMailSet(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char fromNum[32] = {'\0'};
    char *technology;
    char *sipId;

    strlcat(fromNum, s->call->callerid, sizeof(fromNum));

/**
 *  Get the SIP endpoint ID from the channel variable
*/
    technology = strtok(s->call->channel,"/");
    sipId = strtok(NULL,"-");

    // if the action is ON then activate otherwise de-activate
    agi_exec(s, "Playback", "call-forwarding");

    // 18 & 19 (ON/OFF) and 20 (toggle self) are the documented ones 

    if (!strncmp(PARM_CMD, "*18*", 3)) // toggle ON
    {
        DBPut(s, "cfim", sipId, fromNum);
        agi_exec(s, "Playback", "activated");
    }
    else // toggle OFF
    {
        DBPut(s, "cfim", sipId, "");
        agi_exec(s, "Playback", "de-activated");
    }
}

void CFVMailToggle(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char fromNum[32] = {'\0'};
    char toNum[32] = {'\0'};
    char *technology;           // Change name - CONFUSING !!!!!!!!!!!
    char *sipId;

    strlcat(fromNum, s->call->callerid, sizeof(fromNum));
/**
 *  Get the SIP endpoint ID from the channel variable
*/
    technology = strtok(s->call->channel,"/");
    sipId = strtok(NULL,"-");

    // if the property is empty in the DB then activate otherwise de-activate
    agi_exec(s, "Playback", "call-forwarding");
    if (!strcmp(DBGet(s, "cfim", sipId), ""))
    {
        DBPut(s, "cfim", sipId, fromNum);
        agi_exec(s, "Playback", "activated");
    }
    else
    {
        DBPut(s, "cfim", sipId, "");
        agi_exec(s, "Playback", "de-activated");
    }
}

void FollowMe(agi_session_t *s)
{
    (void)s;
    /**
 * NEEDS TO BE REWRITTEN - not sure we care
*/

/*
    DebugFunctionTrace(__FUNCTION__);

    char realFromNum[MAX_EXT_LEN] = {'\0'};

    //  get real fromnum
    strlcpy(realFromNum, myClusterId, sizeof(realFromNum));
    strcat(realFromNum, fromNum);
    agi_exec(s, "VMauthenticate", realFromNum);
    if (!atoi(s->res->result))
    {
        DBPut(s, "cfim", realFromNum, toNum);
        agi_exec(s, "Playback", "activated");
    }
*/
}

void CFOff(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char *technology;           // Change name - CONFUSING !!!!!!!!!!!
    char *sipId;
/**
 *  Get the SIP endpoint ID from the channel variable
*/
    technology = strtok(s->call->channel,"/");
    sipId = strtok(NULL,"-");

    DBPut(s, "cfim", sipId, "");
    DBPut(s, "cfbs", sipId, "");
    agi_exec(s, "Playback", "call-forwarding");
    agi_exec(s, "Playback", "de-activated");
}

void SetRingDelay(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char fromNum[32] = {'\0'};
    char ringDelay[4] = {'\0'};

    char *technology;           // Change name - CONFUSING !!!!!!!!!!!
    char *sipId;
    

    strlcpy(fromNum, s->call->callerid, sizeof(fromNum));
/**
 *  Get the SIP endpoint ID from the channel variable
*/
    technology = strtok(s->call->channel,"/");
    sipId = strtok(NULL,"-");

    strncpy(ringDelay,PARM_CMD + 4,sizeof(ringDelay));

    DBPut(s, "ringdelay", sipId, ringDelay);
    agi_exec(s, "Playback", "silence/1");
    agi_exec(s, "Playback", "activated");
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

/**
 * Normalize caller ID to digits-only for clid_block lookup (fail-open on empty/short).
 */
static void normalize_clid_digits(const char *in, char *out, size_t outlen)
{
    size_t i;
    size_t j = 0;

    if (outlen == 0)
    {
        return;
    }
    out[0] = '\0';
    if (in == NULL)
    {
        return;
    }
    for (i = 0; in[i] != '\0' && j + 1 < outlen; i++)
    {
        if (isdigit((unsigned char)in[i]))
        {
            out[j++] = in[i];
        }
    }
    out[j] = '\0';
}

/**
 * Tenant CLID block (Phase 1): reject inbound when active row matches digits-only CLID.
 * Fail open when cluster/CLID missing or lookup misses.
 */
static int clid_block_should_reject(const char *cluster, const char *callerid)
{
    char norm[64];

    if (cluster == NULL || cluster[0] == '\0')
    {
        return 0;
    }
    if (callerid == NULL || callerid[0] == '\0')
    {
        return 0;
    }
    normalize_clid_digits(callerid, norm, sizeof(norm));
    if (norm[0] == '\0' || strlen(norm) < 6)
    {
        return 0;
    }

    sqlQueryBind2(
        "SELECT action FROM clid_block WHERE cluster=? AND pkey=? AND active='YES' LIMIT 1",
        cluster,
        norm);
    if (rescols[0][0] == '\0')
    {
        return 0;
    }
    if (!strcasecmp(rescols[0], "hangup"))
    {
        return 1;
    }
    return 0;
}

void Ingress(agi_session_t *s)
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
        agi_set_variable(s, "GROUP(inbound)", "inbound");
        agi_get_variable(s, "GROUP_COUNT(inbound)"); // GROUP_COUNT(inbound) is the number of active inbound calls

        if (atoi(s->res->data) > atoi(g_cluster_cfg.maxin_str))
        {
            agi_exec(s, "Playtones", "busy");
            agi_exec(s, "Busy", "");
            return;
        }
    }

    // set the dialled number (DDI) in the CDR userfield
    strlcat(setcdrcmduser, PARM_KEY, sizeof(setcdrcmduser));
    agi_exec(s, "Set", setcdrcmduser);

    if (g_cluster_cfg.dynamicfeatures[0] != '\0')
    {
        char df_ingress[640];
        snprintf(df_ingress, sizeof(df_ingress), "__DYNAMIC_FEATURES=%s", g_cluster_cfg.dynamicfeatures);
        agi_exec(s, "Set", df_ingress);
    }

    sqlQueryBind1("SELECT technology,tag,inprefix,alertinfo,moh,swoclip,cluster FROM inroutes WHERE pkey=?", PARM_KEY);
    strlcpy(technology, rescols[0], sizeof(technology));
    strlcpy(tag, rescols[1], sizeof(tag));
    strlcpy(prefix, rescols[2], sizeof(prefix));
    strlcpy(alertinfo, rescols[3], sizeof(alertinfo));
    strlcpy(moh, rescols[4], sizeof(moh));
    strlcpy(swoclip, rescols[5], sizeof(swoclip));

    agi_set_variable(s, "__MOH", moh);

    // CLIP Processing

    //  DDI Prefix

    if (strcmp(prefix, ""))
    {
        if (!strcmp(prefix, "0 "))
        {
            strlcpy(prefix, "0", sizeof(prefix));
        }
        strlcat(prefix, s->call->callerid, sizeof(prefix));
        strlcpy(s->call->callerid, prefix, sizeof(s->call->callerid));
        snprintf(clidstrng, sizeof(clidstrng), "CALLERID(number)=%s", s->call->callerid);
        agi_exec(s, "Set", clidstrng);
    }

    //...check CLIP routing and recurse if set
    //... This needs to change to a cluster-centric treatment ////////////////////////////////////////////////

    if (strcmp(swoclip, "NO") && strcmp(s->call->callerid, ""))
    {
        if (!strcmp(technology, "PTT_CLID") || !strcmp(technology, "CLID"))
        {
            if (strcmp(s->call->callerid, PARM_KEY))
            {
                strlcpy(clicluster, sqlQueryBind1("SELECT cluster FROM inroutes WHERE pkey=?", s->call->callerid), sizeof(clicluster));
                if (!strcmp(s->call->myCluster, clicluster))
                {
                    agi_set_priority(s, 1);
                    agi_set_extension(s, s->call->callerid);
                    agi_set_context(s, "mainmenu");
                    return;
                }
            }
        }
    }

    // regular CLIP
    if (strcmp(tag, ""))
    {
        strlcpy(s->call->calleridname, tag, sizeof(s->call->calleridname));
        agi_exec(s, "Set", "CALLERID(pres)=allowed");
    }
    if (!strcmp(s->call->calleridname, "unknown"))
    {
        strlcpy(s->call->calleridname, "", sizeof(s->call->calleridname));
    }

    //	Alphatag
    if (strcmp(s->call->calleridname, ""))
    {
        snprintf(clidstrng, sizeof(clidstrng), "CALLERID(name)=\"%s\"", s->call->calleridname);
        agi_exec(s, "Set", clidstrng);
    }

    if (clid_block_should_reject(rescols[6], s->call->callerid))
    {
        agi_exec(s, "Hangup", "");
        return;
    }

    if (strcmp(g_cluster_cfg.lterm_str, "YES"))
    {
        agi_answer(s);
        // agi_exec(s, "Ringing","");
    }
    // cause a ring for voip lines if requested
    if (!strcmp(technology, "SIP"))
    {
        if (strcmp(g_cluster_cfg.ringdelay_str, "0"))
        {
            agi_exec(s, "Ringing", "");
            agi_exec(s, "Wait", g_cluster_cfg.ringdelay_str);
        }
    }
    if (!strcmp(technology, "IAX2"))
    {
        if (strcmp(g_cluster_cfg.ringdelay_str, "0"))
        {
            agi_exec(s, "Ringing", "");
            agi_exec(s, "Wait", g_cluster_cfg.ringdelay_str);
        }
    }

    // distinctive ring (if present) — outbound INVITE via Dial b(pbx3-pre-dial)
    if (strcmp(alertinfo, ""))
    {
        set_pbx3_alert_info(s, alertinfo);
    }

    CheckState(s, PARM_KEY);
}

/**
 * closed-like modes use closeroute when no profile line.
 */
static int mode_is_closed_like(const char *mode)
{
    if (mode == NULL || mode[0] == '\0')
    {
        return 0;
    }
    return (strcasecmp(mode, "closed") == 0);
}

/**
 * Profile line for mode, else legacy openroute/closeroute.
 */
static void ResolveInboundDest(
    agi_session_t *s,
    const char *mode,
    const char *route_profile,
    const char *open_r,
    const char *close_r,
    char *dest,
    size_t destlen)
{
    char effective_mode[32];
    char defmode[32];

    (void)s;
    dest[0] = '\0';
    strlcpy(effective_mode, (mode && mode[0]) ? mode : "open", sizeof(effective_mode));

    if (route_profile != NULL && route_profile[0] != '\0')
    {
        sqlQueryBind2(
            "SELECT destination FROM route_profile_line WHERE profile=? AND lower(mode)=lower(?) LIMIT 1",
            route_profile,
            effective_mode);
        if (rescols[0][0] != '\0')
        {
            strlcpy(dest, rescols[0], destlen);
            return;
        }
        /* Miss: try profile default_mode line. */
        sqlQueryBind1(
            "SELECT default_mode FROM route_profile WHERE shortuid=? LIMIT 1",
            route_profile);
        strlcpy(defmode, rescols[0][0] ? rescols[0] : "open", sizeof(defmode));
        if (strcasecmp(defmode, effective_mode) != 0)
        {
            sqlQueryBind2(
                "SELECT destination FROM route_profile_line WHERE profile=? AND lower(mode)=lower(?) LIMIT 1",
                route_profile,
                defmode);
            if (rescols[0][0] != '\0')
            {
                strlcpy(dest, rescols[0], destlen);
                return;
            }
        }
        /* Still miss: closed-like → closed line, else open line. */
        if (mode_is_closed_like(effective_mode))
        {
            sqlQueryBind1(
                "SELECT destination FROM route_profile_line WHERE profile=? AND lower(mode)='closed' LIMIT 1",
                route_profile);
        }
        else
        {
            sqlQueryBind1(
                "SELECT destination FROM route_profile_line WHERE profile=? AND lower(mode)='open' LIMIT 1",
                route_profile);
        }
        if (rescols[0][0] != '\0')
        {
            strlcpy(dest, rescols[0], destlen);
            return;
        }
    }

    /* Legacy dual-read columns. */
    if (mode_is_closed_like(effective_mode))
    {
        strlcpy(dest, close_r && close_r[0] ? close_r : "None", destlen);
    }
    else
    {
        strlcpy(dest, open_r && open_r[0] ? open_r : "None", destlen);
    }
}

/**
 * Map AstDB OCSTAT to a forced schedule mode (day-parts).
 * AUTO / empty / OPEN → no force (caller follows holiday/timer).
 * CLOSED or any mode token (lunch, night, …) → force that mode (Q5).
 */
static void ocstat_to_force_mode(const char *raw, char *out, size_t outlen)
{
    size_t i;

    if (outlen == 0)
    {
        return;
    }
    out[0] = '\0';
    if (raw == NULL || raw[0] == '\0')
    {
        return;
    }
    if (!strcasecmp(raw, "AUTO") || !strcasecmp(raw, "OPEN"))
    {
        return;
    }
    for (i = 0; raw[i] != '\0' && i + 1 < outlen; i++)
    {
        char c = raw[i];
        if (c >= 'A' && c <= 'Z')
        {
            c = (char)(c - 'A' + 'a');
        }
        out[i] = c;
    }
    out[i] = '\0';
}

void CheckState(agi_session_t *s, char *remotenum)
{

    DebugFunctionTrace(__FUNCTION__);

    char state[32] = {'\0'};
    char cluster[MAX_CLUSTER_LEN] = {'\0'};
    char route_profile[32] = {'\0'};
    char entry_dest[64] = {'\0'};
    char dest[64] = {'\0'};
    char mode[32] = {'\0'};
    char holiday_dest[64] = {'\0'};
    char force_mode[32] = {'\0'};

    sqlQueryBind1(
        "SELECT cluster,openroute,closeroute,route_profile,entry_dest FROM inroutes WHERE pkey=?",
        remotenum);
    strlcpy(cluster, rescols[0], sizeof(cluster));
    strlcpy(openroute, rescols[1], sizeof(openroute));
    strlcpy(closeroute, rescols[2], sizeof(closeroute));
    strlcpy(route_profile, rescols[3], sizeof(route_profile));
    strlcpy(entry_dest, rescols[4], sizeof(entry_dest));

    /* Prefer holiday_force_dest, else legacy routeoverride (dual-read). */
    if (g_cluster_cfg.holiday_force_dest[0] != '\0')
    {
        strlcpy(holiday_dest, g_cluster_cfg.holiday_force_dest, sizeof(holiday_dest));
    }
    else if (g_cluster_cfg.routeoverride[0] != '\0')
    {
        strlcpy(holiday_dest, g_cluster_cfg.routeoverride, sizeof(holiday_dest));
    }

    /*
     * Operator hard-force (Q5): master then tenant AstDB OCSTAT.
     * Values: AUTO/OPEN = no force; CLOSED or day-part mode = force that mode.
     * Wins over holiday dest.
     */
    strlcpy(state, DBGet(s, "STAT", "OCSTAT"), sizeof(state));
    ocstat_to_force_mode(state, force_mode, sizeof(force_mode));
    if (force_mode[0] == '\0')
    {
        strlcpy(state, DBGet(s, cluster, "OCSTAT"), sizeof(state));
        ocstat_to_force_mode(state, force_mode, sizeof(force_mode));
    }
    if (force_mode[0] == '\0' && s->call->myCluster[0] != '\0')
    {
        strlcpy(state, DBGet(s, s->call->myCluster, "OCSTAT"), sizeof(state));
        ocstat_to_force_mode(state, force_mode, sizeof(force_mode));
    }
    if (force_mode[0] != '\0')
    {
        ResolveInboundDest(s, force_mode, route_profile, openroute, closeroute, dest, sizeof(dest));
        agi_set_priority(s, 1);
        agi_set_extension(s, dest);
        agi_set_context(s, s->call->myClusterContext);
        return;
    }

    /* Fixed entry destination (always-same DID) — no schedule. */
    if (entry_dest[0] != '\0' && strcasecmp(entry_dest, "None") != 0)
    {
        agi_set_priority(s, 1);
        agi_set_extension(s, entry_dest);
        agi_set_context(s, s->call->myClusterContext);
        return;
    }

    /* Holiday dest override when not operator hard-forced. */
    if (holiday_dest[0] != '\0' && strcasecmp(holiday_dest, "None") != 0)
    {
        agi_set_priority(s, 1);
        agi_set_extension(s, holiday_dest);
        agi_set_context(s, s->call->myClusterContext);
        return;
    }

    strlcpy(mode, CheckTime(s, cluster), sizeof(mode));
    ResolveInboundDest(s, mode, route_profile, openroute, closeroute, dest, sizeof(dest));
    agi_set_priority(s, 1);
    agi_set_extension(s, dest);
    agi_set_context(s, s->call->myClusterContext);
}

char *CheckTime(agi_session_t *s, char *cluster)
{

    DebugFunctionTrace(__FUNCTION__);

    static char mode_ret[32];
    char force_mode[32] = {'\0'};
    char raw[32] = {'\0'};

    /*
     * Tenant force is applied in CheckState (above holiday). Here: sched_mode / oclo only.
     * Defensive: if a caller uses CheckTime alone, still honour OCSTAT force.
     */
    strlcpy(raw, DBGet(s, cluster, "OCSTAT"), sizeof(raw));
    ocstat_to_force_mode(raw, force_mode, sizeof(force_mode));
    if (force_mode[0] == '\0' && s->call->myCluster[0] != '\0')
    {
        strlcpy(raw, DBGet(s, s->call->myCluster, "OCSTAT"), sizeof(raw));
        ocstat_to_force_mode(raw, force_mode, sizeof(force_mode));
    }
    if (force_mode[0] != '\0')
    {
        strlcpy(mode_ret, force_mode, sizeof(mode_ret));
        return mode_ret;
    }

    /* Prefer sched_mode (day-parts), else legacy oclo OPEN/CLOSED. */
    if (g_cluster_cfg.sched_mode[0] != '\0')
    {
        strlcpy(mode_ret, g_cluster_cfg.sched_mode, sizeof(mode_ret));
        /* normalize OPEN/CLOSED upper from historic dual-write */
        if (!strcasecmp(mode_ret, "OPEN"))
        {
            strlcpy(mode_ret, "open", sizeof(mode_ret));
        }
        else if (!strcasecmp(mode_ret, "CLOSED"))
        {
            strlcpy(mode_ret, "closed", sizeof(mode_ret));
        }
        return mode_ret;
    }

    if (!strcasecmp(g_cluster_cfg.oclo, "OPEN"))
    {
        strlcpy(mode_ret, "open", sizeof(mode_ret));
        return mode_ret;
    }

    if (!strcasecmp(g_cluster_cfg.oclo, "CLOSED"))
    {
        strlcpy(mode_ret, "closed", sizeof(mode_ret));
        return mode_ret;
    }
    agi_exec(s, "NoOp", "NO MTIME - returning open");
    strlcpy(mode_ret, "open", sizeof(mode_ret));
    return mode_ret;
}

void IVR(agi_session_t *s, char *ivrname)
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

    /* Asterisk sound silence/N — seconds to wait for a digit after the greeting. */
    if (g_cluster_cfg.ivr_key_wait[0] != '\0')
    {
        strlcat(ivrsilence, g_cluster_cfg.ivr_key_wait, sizeof(ivrsilence));
    }
    else
    {
        strlcat(ivrsilence, "6", sizeof(ivrsilence));
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

        agi_exec(s, "Wait", "0.5");
        agi_answer(s);

        strlcpy(greetnum, sqlQueryBind1("SELECT greetnum FROM ivrmenu WHERE pkey=?", PARM_KEY), sizeof(greetnum));
        snprintf(msg, sizeof(msg), "%s%s/usergreeting%s", SOUNDIR, s->call->myCluster, greetnum);
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
            agi_stream_file(s, msg, optionStr, 0);
            if (strcmp(optionStr, "") && atoi(s->res->result) > 0)
            {
                snprintf(dtmf, sizeof(dtmf), "%c", atoi(s->res->result));
                IVRAction(s, PARM_KEY, dtmf);
                return;
            }
            if (strcmp(optionStr, ""))
            {
                agi_stream_file(s, ivrsilence, optionStr, 0);
                if (strcmp(optionStr, "") && atoi(s->res->result) > 0)
                {
                    snprintf(dtmf, sizeof(dtmf), "%c", atoi(s->res->result));
                    IVRAction(s, PARM_KEY, dtmf);
                    return;
                }
            }
        }
        //
        // here is the get_data command
        // it can listen for more digits - we set it to listen for up to 4 digits.

        else
        {
            agi_get_data(s, msg, ivrdigitwait, 4);
            if (atoi(s->res->result) > 0)
            {
                // check if it's an extension
                snprintf(dtmf, sizeof(dtmf), "%s", s->res->result);
                if (strlen(dtmf) > 1)
                {
                    agi_set_priority(s, 1);
                    agi_set_extension(s, dtmf);
                    agi_set_context(s, s->call->myClusterContext);
                    return;
                }
                IVRAction(s, PARM_KEY, dtmf);
                return;
            }
        }

        // If no key is pressed then perform the timeout action
        if (!strcmp(sqlQueryBind1("SELECT timeout FROM ivrmenu WHERE pkey=?", PARM_KEY), "Repeat Message"))
        {
            IVR(s, ivrname);
            return;
        }
        else
        {
            IVRAction(s, PARM_KEY, "");
            return;
        }
    }
}

void IVRAction(agi_session_t *s, char *menu, char *press)
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
        /* Alert-Info for distinctive ring on the outbound INVITE (Dial b()). */
        snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM ivrmenu WHERE pkey=?", dbAlert);
        strlcpy(alert, sqlQueryBind1(myQuery, menu), sizeof(alert));
        if (alert[0] != '\0')
        {
            set_pbx3_alert_info(s, alert);
        }
        snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM ivrmenu WHERE pkey=?", tag);
        strcpy(tagID, sqlQueryBind1(myQuery, menu));
        if (strcmp(tagID, ""))
        {
            strlcpy(s->call->calleridname, tagID, sizeof(s->call->calleridname));
        }
    }
    // CLIP Processing

    if (!strcmp(s->call->calleridname, "unknown"))
    {
        strlcpy(s->call->calleridname, "", sizeof(s->call->calleridname));
    }
    //	Alphatag
    if (strcmp(s->call->calleridname, ""))
    {
        snprintf(clidstrng, sizeof(clidstrng), "CALLERID(name)=\"%s\"", s->call->calleridname);
        agi_exec(s, "Set", clidstrng);
    }
    //  Route it
    if (strcmp(action, "None"))
    {
        agi_set_priority(s, 1);
        agi_set_extension(s, action);
        agi_set_context(s, s->call->myClusterContext);
        return;
    }
    // bad key press?
    return;
}

char * DBGet(agi_session_t *s, char *family, char *key)
{

    DebugFunctionTrace(__FUNCTION__);

    agi_database_get(s, family, key);
    return s->res->data;
}

void DBPut(agi_session_t *s, char *family, char *key, char *val)
{

    DebugFunctionTrace(__FUNCTION__);

    agi_database_put(s, family, key, val);
}

void DBDel(agi_session_t *s, char *family, char *key)
{

    DebugFunctionTrace(__FUNCTION__);

    agi_database_del(s, family, key);
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
void OutQmt(agi_session_t *s)
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
    agi_exec(s, "Set", abstimeout);
    //
    //    initial log entries for queuemetrics
    //
    snprintf(buffer, sizeof(buffer), "%d|%s|%s|NONE|ENTERQUEUE|-|%s", nowstart, s->call->uniqueid, PARM_PM2, PARM_KEY);
    QLogWrite(buffer);
    snprintf(buffer, sizeof(buffer), "%d|%s|%s|%s|CONNECT|0||", nowstart, s->call->uniqueid, PARM_PM2, PARM_PM3);
    QLogWrite(buffer);
    //
    // 	turn off HUP so we can run "deadagi" after the dial
    //
    if (signal(SIGHUP, sig_handler) == SIG_ERR)
    {
        snprintf(vmsg, sizeof(vmsg), "Cant catch SIGHUP!!");
        agi_verbose(s, vmsg, 1);
    }

    signal(SIGHUP, SIG_IGN);
    //
    //    Do the Dial
    //
    strlcpy(s->call->extension, PARM_KEY, sizeof(s->call->extension));
    OutVoip(s, PARM_PM1);
    if (atoi(s->res->result))
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
    agi_get_variable(s, "ANSWEREDTIME"); //Asterisk varaiable set when a queue ends
    answeredtime = atoi(s->res->data);

    if (answeredtime == 0)
    {
        snprintf(buffer, sizeof(buffer), "%d|%s|%s|NONE|ABANDON|1|1", nowend, s->call->uniqueid, PARM_PM2);
        QLogWrite(buffer);
        return;
    }

    waittime = (nowend - nowstart) - answeredtime;
    connecttime = nowend - answeredtime;
    snprintf(buffer, sizeof(buffer), "%d|%s|%s|%s|CONNECT|%d|", connecttime, s->call->uniqueid, PARM_PM2, PARM_PM3, waittime);
    QLogWrite(buffer);
    snprintf(buffer, sizeof(buffer), "%d|%s|%s|%s|%s|%d|%d|", nowend, s->call->uniqueid, PARM_PM2, PARM_PM3, whohungup, waittime, answeredtime);
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
void outboundClip(agi_session_t *s, char *key)
{
/*
 * N.B. Look at the new version of this code in 6.2
 *
 */
 
    DebugFunctionTrace(__FUNCTION__);

	char clidwork[MAX_EXT_LEN] = {'\0'};
	const char *sqlclid;

	// backstop CLID if all else fails - take the trunk's CLID (if it exists)
	sqlclid = sqlQueryBind1("SELECT callerid FROM trunks WHERE pkey=?", key);
	strlcpy(clidline, sqlclid ? sqlclid : "", sizeof(clidline));
	if (strcmp(clidline, "")) {
		strlcpy(clidwork, clidline, sizeof(clidwork));
		snprintf(vmsg, sizeof(vmsg), "trunks CLID  %s found for outbound call, using key %s", clidwork, key);
		agi_verbose(s, vmsg,1);
	}
	else {
		snprintf(vmsg, sizeof(vmsg), "No trunks CLID found for outbound call, using key %s", key);
		agi_verbose(s, vmsg,1);
	}


	// If there is a cluster CLID then it trumps the line 
	if (strcmp(s->call->myClusterclid, "")) {
		snprintf(vmsg, sizeof(vmsg), "Cluster CLID %s found", s->call->myClusterclid);
		agi_verbose(s, vmsg,1);
		strlcpy(clidwork, s->call->myClusterclid, sizeof(clidwork));
	}
	else {
		snprintf(vmsg, sizeof(vmsg), "no cluster CLID found for outbound call, using key %s", key);
		agi_verbose(s, vmsg,1);
	}


	//if there is an extension CLID or RDNIS CLID then it trumps the line and cluster CLID 
	if (s->call->caller_is_local) {
		sqlclid = sqlQueryBind1("SELECT callerid FROM IPphone WHERE pkey=?", s->call->callerid);
		strlcpy(clidphone, sqlclid ? sqlclid : "", sizeof(clidphone));
	}
	// if the RDNIS is local, set its CLID into clidphone
	else if (s->call->rdnis_is_local) {
		sqlclid = sqlQueryBind1("SELECT callerid FROM IPphone WHERE pkey=?", s->call->rdnis);
		strlcpy(clidphone, sqlclid ? sqlclid : "", sizeof(clidphone));
	}

	// Only take the extension clid if it is longer than 5 characters (i.e. - not an extension number)
	// we do not want to send an extension number CLID onto the PSTN
	if (strcmp(clidphone, "") && (strlen(clidphone) > 5)) {
		strlcpy(clidwork, clidphone, sizeof(clidwork));
		snprintf(vmsg, sizeof(vmsg), "Extension CLID %s found for outbound call, using key %s", clidwork, key);
		agi_verbose(s, vmsg,1);
	}
	else {
		snprintf(vmsg, sizeof(vmsg), "No PSTN extension CLID found for outbound call, using clid %s", clidphone);
		agi_verbose(s, vmsg,1);
	}

	snprintf(vmsg, sizeof(vmsg), "Phase 1 CLID is %s for outbound call, using key %s", clidwork, key);
	agi_verbose(s, vmsg,1);

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
		if (s->call->caller_is_local) {
			snprintf(vmsg, sizeof(vmsg), "Using CLID %s for outbound call, using key %s", clidwork, key);
			agi_verbose(s, vmsg,1);
			snprintf(clidstrng, sizeof(clidstrng), "CALLERID(number)=%s", clidwork);
			agi_exec(s, "Set", clidstrng);		
		
			// if the RDNIS is set (which covers local diversions) then reset it to the CLID to make it safe.  
			// It will look odd because the DIVERT header will be the same as the FROM header but it is necessary 
			// in the case of a local call which already has RDNIS set.  This will happen if the diversion has come from a phone
			// (rather than a pbx divert, e.g. *21*) and the phone has set the RDNIS to the original callerid.
		
			if (s->call->rdnis_is_set) {
				snprintf(vmsg, sizeof(vmsg), "Using CLID %s to override local RDNIS", clidwork);
				agi_verbose(s, vmsg,1);
				snprintf(clidstrng, sizeof(clidstrng), "CALLERID(RDNIS)=%s", clidwork);
				agi_exec(s, "Set", clidstrng);
			}
		}
		// if the inbound callerid is a real number then this is a hairpin call so set the RDNIS to our CLID to create a 
		// diversion header.
		
		else {
			snprintf(clidstrng, sizeof(clidstrng), "CALLERID(RDNIS)=%s", clidwork);
			agi_exec(s, "Set", clidstrng);
		}
	}   
	else {
		// we have no CLID to set so just log it
		snprintf(vmsg, sizeof(vmsg), "No override CLID found for outbound call, using key %s", key);
		agi_verbose(s, vmsg,1);
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
