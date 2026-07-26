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
char clidstrng[MAX_EXT_LEN] = {'\0'};     // CLI build string for sprintf
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

static const agi_cmd_entry_t agi_cmd_table[] = {
    { 1, "OutTrunk", cmd_OutTrunk },
    { 2, "OutRoute", OutRoute },
    { 3, "LepDial", LepDial },
    { 4, "Ingress", Ingress },
    { 5, "Dial", cmd_Dial },
    { 6, "IVR", cmd_IVR },
    { 7, "OutQmt", OutQmt },
    { 8, "PostDial", PostDial },
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
    /* ChanSpy*: still need MultiTenant work */
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
    AGITool_exec(s->agi, s->res, "Set", setcdrcmd);

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

    AGITool_get_variable(&agi, &res, "DEBUG");
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
        AGITool_answer(&agi, &res);
        AGITool_exec(&agi, &res, "Wait", "0.5");
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
    AGITool_exec(s->agi, s->res, "Set", setmohcmd);
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

char *Mangle(agi_session_t *s, char *preSel, char *transformList, char *data)
{

    DebugFunctionTrace(__FUNCTION__);

    char *transform, *left, *right;
    char transformArr[MAX_NUM_TRANSFORM][MAX_TRANSFORM_LEN];
    char operand[MAX_NUM_LEN] = {'\0'};
    char *pOperand = &operand[0];
    int i = 0, j = 0, k = 0, len = 0;

    if (!strcmp(data, "CLI"))
    {
        strlcpy(operand, s->call->callerid, sizeof(operand));
    }
    else
    {
        strlcpy(operand, s->call->extension, sizeof(operand));
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

    strlcpy(ext, GetExt(PARM_CMD), sizeof(ext));
    snprintf(newGreetFile, sizeof(newGreetFile), "%s%s/usergreeting%s.wav", SOUNDIR, s->call->myCluster, ext);

    // check password
    if (Authenticate(s, "SYSPASS") != 0) //** syspass is held in the tenant table (used to be in Globals)
    {
        return;
    }
    // message to record
    AGITool_exec(s->agi, s->res, "Playback", "pm-announcement-number");
    AGITool_exec(s->agi, s->res, "SayDigits", ext);
    AGITool_exec(s->agi, s->res, "Playback", "is-now-being-recorded");
    AGITool_exec(s->agi, s->res, "Playback", "silence/1");
    AGITool_exec(s->agi, s->res, "Playback", "press-pound-save-changes");

    // record message
    while (press == '2')
    {
        AGITool_record_file(s->agi, s->res, tmpGreetFile, "wav", "#", 120000, 10, 10, 0);
        AGITool_stream_file(s->agi, s->res, tmpGreetFile, "", 0);
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
        AGITool_exec(s->agi, s->res, "Playback", "your-msg-has-been-saved");
        AGITool_exec(s->agi, s->res, "Playback", "goodbye");
        return;
    }

    remove(tmpGreetFile);
    AGITool_exec(s->agi, s->res, "Playback", "cancelled");
}


int AuthenticatePassword(agi_session_t *s, const char *password_plain)
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
    AGITool_exec(s->agi, s->res, "Playback", "silence/1");
    AGITool_exec(s->agi, s->res, "Authenticate", authbuf);
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
        AGITool_stream_file(s->agi, s->res, "save-announce-press", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        AGITool_stream_file(s->agi, s->res, "digits/1", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        AGITool_stream_file(s->agi, s->res, "to-rerecord-announce", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        AGITool_stream_file(s->agi, s->res, "digits/2", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        AGITool_stream_file(s->agi, s->res, "to-cancel-this-msg", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        AGITool_stream_file(s->agi, s->res, "press", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        AGITool_stream_file(s->agi, s->res, "digits/3", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        AGITool_stream_file(s->agi, s->res, "silence/5", "123", 0);
        if (atoi(s->res->result))
            return atoi(s->res->result);
        count++;
    }

    return '3';
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

    snprintf(f_eAgent, sizeof(f_eAgent), "%s/eAgent", s->call->myClusterContext);
    snprintf(f_dAgent, sizeof(f_dAgent), "%s/dAgent", s->call->myClusterContext);
    snprintf(f_dynLogin, sizeof(f_dynLogin), "%s/DYNLOGIN", s->call->myClusterContext);

    strlcpy(agentchan, "Local/", sizeof(agentchan));
    strlcat(agentchan, s->call->callerid, sizeof(agentchan));
    strlcat(agentchan, "@", sizeof(agentchan));
    strlcat(agentchan, s->call->myClusterContext, sizeof(agentchan));
    strlcpy(statechan, "Local/", sizeof(statechan));
    strlcat(statechan, s->call->callerid, sizeof(statechan));

    AGITool_get_data(s->agi, s->res, "agent-user", 7000, 5);

    while (!finished)
    {
        if (atoi(s->res->result))
        {
            // check if it's a valid agent
            snprintf(agent, sizeof(agent), "%s", s->res->result);
            if (strcmp(sqlQueryBind1("SELECT pkey FROM agent WHERE pkey=?", agent), ""))
            {
                strlcpy(agentpasswd, sqlQueryBind1("SELECT passwd FROM agent WHERE pkey=?", agent), sizeof(agentpasswd));
                AGITool_exec(s->agi, s->res, "Authenticate", agentpasswd);
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
                        AGITool_get_variable(s->agi, s->res, "EPOCH"); //Asterisk system variable EPOCH
                        strlcpy(epoch, s->res->data, sizeof(epoch));
                        for (i = 1; i < 7; i++)
                        {
                            snprintf(agentqueue, sizeof(agentqueue), "queue%i", i);
                            snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM agent WHERE pkey=?", agentqueue);
                            strlcpy(queuename, sqlQueryBind1(myQuery, oldagent), sizeof(queuename));
                            if (strcmp(queuename, "None"))
                            {
                                //								snprintf (queuearg, sizeof(queuearg), "%s%sLocal/%s@queues",queuename,ASTDLIM,extenAgent);
                                snprintf(queuearg, sizeof(queuearg), "%s,%s", queuename, agentchan);
                                AGITool_exec(s->agi, s->res, "RemoveQueueMember", queuearg);
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
                        AGITool_exec(s->agi, s->res, "Wait", "1");
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
                            AGITool_exec(s->agi, s->res, "AddQueueMember", queuearg);
                        }
                    }
                    AGITool_get_variable(s->agi, s->res, "EPOCH"); //Asterisk system variable EPOCH
                    strlcpy(epoch, s->res->data, sizeof(epoch));
                    snprintf(buffer, sizeof(buffer), "%s|%s|NONE|Agent/%s|AGENTLOGIN|%s", epoch, s->call->uniqueid, agent, agentchan);
                    QLogWrite(buffer);
                    if (!strcmp(DBGet(s, "DYNLOGIN", agent), ""))
                    {
                        DBPut(s, f_dynLogin, agent, epoch);
                    }
                    DBPut(s, f_dAgent, agent, s->call->callerid);
                    DBPut(s, f_eAgent, s->call->callerid, agent);
                    AGITool_exec(s->agi, s->res, "Playback", "agent-loginok");
                    finished = TRUE;
                    continue;
                }
            }
        }
        AGITool_get_data(s->agi, s->res, "agent-incorrect", 7000, 5);
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

    snprintf(f_eAgent, sizeof(f_eAgent), "%s/eAgent", s->call->myClusterContext);
    snprintf(f_dAgent, sizeof(f_dAgent), "%s/dAgent", s->call->myClusterContext);
    snprintf(f_dynLogin, sizeof(f_dynLogin), "%s/DYNLOGIN", s->call->myClusterContext);

    strlcpy(agentchan, "Local/", sizeof(agentchan));
    strlcat(agentchan, s->call->callerid, sizeof(agentchan));
    strlcat(agentchan, "@", sizeof(agentchan));
    strlcat(agentchan, s->call->myClusterContext, sizeof(agentchan));
    strlcpy(statechan, "Local/", sizeof(statechan));
    strlcat(statechan, s->call->callerid, sizeof(statechan));

    strcpy(agent, DBGet(s, f_eAgent, s->call->callerid));
    strlcpy(agentpasswd, sqlQueryBind1("SELECT passwd FROM agent WHERE pkey=?", agent), sizeof(agentpasswd));

    // check if they are logged in - if not just exit

    if (!strcmp(agent, ""))
    {
        AGITool_exec(s->agi, s->res, "Playback", "agent-loggedoff");
        return;
    }

    AGITool_get_variable(s->agi, s->res, "EPOCH"); //Asterisk system variable EPOCH
    strlcpy(epoch, s->res->data, sizeof(epoch));
    strlcpy(startepoch, DBGet(s, f_dynLogin, agent), sizeof(startepoch));
    //	AGITool_exec(s->agi,s->res,"Authenticate",agentpasswd);
    //	if (atoi(s->res->result)==0) {
    for (i = 1; i < 7; i++)
    {
        snprintf(agentqueue, sizeof(agentqueue), "queue%i", i);
        snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM agent WHERE pkey=?", agentqueue);
        strlcpy(queuename, sqlQueryBind1(myQuery, agent), sizeof(queuename));
        if (strcmp(queuename, "None"))
        {
            snprintf(queuearg, sizeof(queuearg), "%s,%s", queuename, agentchan);
            AGITool_exec(s->agi, s->res, "RemoveQueueMember", queuearg);
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
    AGITool_exec(s->agi, s->res, "Playback", "agent-loggedoff");
    //	}
}

void AgentPause(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char agent[8] = {'\0'};      // agent number
    char queuearg[256] = {'\0'}; // argument
    char f_eAgent[64] = {'\0'};

    snprintf(f_eAgent, sizeof(f_eAgent), "%s/eAgent", s->call->myClusterContext);
    strlcpy(agent, DBGet(s, f_eAgent, s->call->callerid), sizeof(agent));

    if (strcmp(agent, ""))
    {
        snprintf(queuearg, sizeof(queuearg), ",Local/%s@%s", s->call->callerid, s->call->myClusterContext);
        AGITool_exec(s->agi, s->res, "PauseQueueMember", queuearg);
    }
    AGITool_exec(s->agi, s->res, "Playback", "beep");
}

void AgentUnpause(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char agent[8] = {'\0'};      // agent number
    char queuearg[256] = {'\0'}; // argument
    char f_eAgent[64] = {'\0'};

    snprintf(f_eAgent, sizeof(f_eAgent), "%s/eAgent", s->call->myClusterContext);
    strlcpy(agent, DBGet(s, f_eAgent, s->call->callerid), sizeof(agent));

    if (strcmp(agent, ""))
    {
        snprintf(queuearg, sizeof(queuearg), ",Local/%s@%s", s->call->callerid, s->call->myClusterContext);
        AGITool_exec(s->agi, s->res, "UnPauseQueueMember", queuearg);
    }
    AGITool_exec(s->agi, s->res, "Playback", "beep");
}

//ToDo needs to become extenSpy
void ChanSpyWhisper(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char ext[MAX_EXT_LEN] = {'\0'};
    char options[32] = {'\0'};
    if (Authenticate(s, "SPYPASS") != 0) //** spy_pass is held in the tenant table (used to be in Globals)
    {
        return;
    }
 
    strlcat(ext, GetExt(PARM_CMD), sizeof(ext));
    strlcpy(options, SIPDRIVER, sizeof(options));
    strlcat(options, ext, sizeof(options));
    strlcat(options, ",qw", sizeof(options));
    AGITool_exec(s->agi, s->res, "ChanSpy", options);
}

//ToDo needs to become extenSpy
void ChanSpy(agi_session_t *s)
{
    
    DebugFunctionTrace(__FUNCTION__);

    char ext[MAX_EXT_LEN] = {'\0'};
    char options[32] = {'\0'};
    if (Authenticate(s, "SPYPASS") != 0) //** spy_pass is held in the tenant table (used to be in Globals)
    {
        return;
    }
//    strlcpy(ext, myClusterId, sizeof(ext));
    strlcat(ext, GetExt(PARM_CMD), sizeof(ext));
    strlcpy(options, SIPDRIVER, sizeof(options));
    strlcat(options, "/", sizeof(options));
    strlcat(options, ext, sizeof(options));
    strlcat(options, ",q", sizeof(options));
    AGITool_exec(s->agi, s->res, "ChanSpy", options);
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
        AGITool_exec(s->agi, s->res, "Set", dfbuf);
    }

    
    /*
     * Cluster absolute timeout (tenant); 0 means barred.
     */
    if (g_cluster_cfg.abstimeout_sec == 0)
    {
        AGITool_exec(s->agi, s->res, "Playtones", "busy");
        AGITool_exec(s->agi, s->res, "Busy", "");
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
                AGITool_exec(s->agi, s->res, "Playtones", "busy");
                AGITool_exec(s->agi, s->res, "Busy", "");
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
        AGITool_set_variable(s->agi, s->res, clusterGroup, s->call->myCluster);
        snprintf(clusterCount, sizeof(clusterCount), "GROUP_COUNT(%s)", s->call->myCluster);
        AGITool_get_variable(s->agi, s->res, clusterCount); // clusterCount is the number of active outbound calls
        if (atoi(s->res->data) > atoi(g_cluster_cfg.chanmax_str))
        {
            AGITool_exec(s->agi, s->res, "Playtones", "busy");
            AGITool_exec(s->agi, s->res, "Busy", "");
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
        AGITool_exec(s->agi, s->res, "Authenticate", eparm);
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
        AGITool_get_variable(s->agi, s->res, PARM_KEY); // PARM_KEY is the path key - no change for PBX3
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
                AGITool_exec(s->agi, s->res, "Set", setlast);
            }
            else if (strcmp(path[last], "None"))
            {
                OutVoip(s, path[last]);
            }
        }
    
        AGITool_get_variable(s->agi, s->res, "DIALSTATUS"); // DIALSTATUS is the status of the call - answered, busy, cancelled
        if (!strcmp(s->res->data, "ANSWER"))
        {
            return;
        }
        if (!strcmp(s->res->data, "BUSY"))
        {
            if (!strncmp(g_cluster_cfg.playbusy, "YES", 3))
            {
                AGITool_exec(s->agi, s->res, "Playtones", "busy");
                AGITool_exec(s->agi, s->res, "Busy", "");
            }
            else
            {
                AGITool_exec(s->agi, s->res, "Playback", "numb-dialled-busy");
                AGITool_exec(s->agi, s->res, "Playback", "silence/1");
                AGITool_exec(s->agi, s->res, "Playback", "please-try-again-later");
            }
            return;
        }

        if (!strcmp(s->res->data, "CANCEL"))
        {
            return;
        }

        if (!strncmp(g_cluster_cfg.playbeep, "YES", 3) && strcmp(path[last], "None"))
        {
            AGITool_exec(s->agi, s->res, "Playback", "beep");
        }
        last++;
        if (last >= 3)
        {
            last = 0;
        }
    }   

    if (strcmp(alternate, "") && strcmp(alternate, s->call->extension))
    {
        AGITool_set_priority(s->agi, s->res, 1);
        AGITool_set_extension(s->agi, s->res, alternate);
        AGITool_set_context(s->agi, s->res, s->call->myClusterContext);
        return;
    }

    if (!strncmp(g_cluster_cfg.playcongested, "YES", 3))
    {
        AGITool_exec(s->agi, s->res, "Playtones", "congestion");
        AGITool_exec(s->agi, s->res, "Congestion", "");
    }
    else
    {
        AGITool_exec(s->agi, s->res, "Playback", "were-sorry");
        AGITool_exec(s->agi, s->res, "Playback", "call-cannot-complete");
        AGITool_exec(s->agi, s->res, "Playback", "please-hang-up-and-try-again");
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
        AGITool_get_variable(s->agi, s->res, "DIALSTATUS"); // DIALSTATUS is the status of the call - answered, busy, cancelled
        if (!strcmp(s->res->data, "ANSWER"))
        {
        }
        else if (!strcmp(s->res->data, "BUSY"))
        {
            if (!strncmp(g_cluster_cfg.playbusy, "YES", 3))
            {
                AGITool_exec(s->agi, s->res, "Playtones", "busy");
                AGITool_exec(s->agi, s->res, "Busy", "");
            }
            else
            {
                AGITool_exec(s->agi, s->res, "Playback", "numb-dialled-busy");
                AGITool_exec(s->agi, s->res, "Playback", "silence/1");
                AGITool_exec(s->agi, s->res, "Playback", "please-try-again-later");
            }
        }
        else
        {
            if (!strncmp(g_cluster_cfg.playcongested, "YES", 3))
            {
                AGITool_exec(s->agi, s->res, "Playtones", "congestion");
                AGITool_exec(s->agi, s->res, "Congestion", "");
            }
            else
            {
                AGITool_exec(s->agi, s->res, "Playback", "were-sorry");
                AGITool_exec(s->agi, s->res, "Playback", "call-cannot-complete");
                AGITool_exec(s->agi, s->res, "Playback", "please-hang-up-and-try-again");
            }
        }
    }
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
        strlcpy(number, Mangle(s, preSel, transform, ""), sizeof(number));
    }
    else
    {
        strcpy(number, s->call->extension);
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
            AGITool_answer(s->agi, s->res);
        }
    }

    AGITool_set_variable(s->agi, s->res, "GROUP()", "OUTBOUND_GROUP");
    AGITool_get_variable(s->agi, s->res, "GROUP_COUNT()"); // GROUP_COUNT() is the number of active outbound calls
    if (atoi(s->res->data) <= atoi(g_cluster_cfg.voipmax_str))
    {
        strlcpy(recRet, SetRecord(s, s->call->callerid, "Outbound"), sizeof(recRet));
        strlcat(dialString, recRet, sizeof(dialString));
        //  Abs timeout
        AGITool_exec(s->agi, s->res, "Set", abstimeout);
        //  acounting
        // setOutbound_cdr_userfield();
        // Dial
        AGITool_exec(s->agi, s->res, "Dial", dialString);
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
    AGITool_set_variable(s->agi, s->res, "PBX3_DIAL", "");

    AGITool_get_variable(s->agi, s->res, "BLINDTRANSFER");  //set in extensions.conf
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
                    AGITool_exec(s->agi, s->res, "Playback", "silence/1");
                    if (!strcmp(g_cluster_cfg.playtransfer, "YES"))
                    {
                        AGITool_exec(s->agi, s->res, "Playback", "pls-hold-while-try");
                    }
                    strlcpy(transferer, pBtr, sizeof(transferer));
                    AGITool_set_priority(s->agi, s->res, 1);
                    AGITool_set_extension(s->agi, s->res, transferer);
                    AGITool_set_context(s->agi, s->res, s->call->myClusterContext);
                    return;
                }
            }
            else
            {
                AGITool_exec(s->agi, s->res, "Playtones", "congestion");
                AGITool_exec(s->agi, s->res, "congestion", "");
                return;
            }
        }
        else
        {
            //            AGITool_exec(s->agi,s->res,"Playback","silence/1");
            strlcat(vmflags, "u", sizeof(vmflags));
            AGITool_exec(s->agi, s->res, "Voicemail", strcat(vmbox, vmflags));
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
 *  Add a SIP header if we have one (usually a hook directive or distinctive ring)
 */
    if (strcmp(extalert, ""))
    {
        AGITool_exec(s->agi, s->res, "SIPAddHeader", extalert);
    }

 /**
  *  set a pickup mark for directed call pickup
  */
    AGITool_set_variable(s->agi, s->res, "__PICKUPMARK", s->call->extension);

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

    AGITool_get_variable(s->agi, s->res, "BLINDTRANSFER");
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

    AGITool_get_variable(s->agi, s->res, "DIALSTATUS");
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
                        AGITool_exec(s->agi, s->res, "SIPAddHeader", g_cluster_cfg.bounce_alert);
                    }
                    strlcpy(calleridsave, s->call->callerid, sizeof(calleridsave));
                    strlcpy(s->call->callerid, "R", sizeof(s->call->callerid));
                    strlcat(s->call->callerid, calleridsave, sizeof(s->call->callerid));
                    AGITool_set_callerid(s->agi, s->res, s->call->callerid);
                    strlcpy(transferer, pBtr, sizeof(transferer));
                    AGITool_set_priority(s->agi, s->res, 1);
                    AGITool_set_extension(s->agi, s->res, transferer);
                    AGITool_set_context(s->agi, s->res, s->call->myClusterContext);
                    return;
                }
            }
        }
        else
        {
            strlcat(vmflags, "u", sizeof(vmflags));
            AGITool_exec(s->agi, s->res, "Voicemail", strcat(vmbox, vmflags));
            return;
        }
    }
    if (!strcmp(vmbox, "None"))
    {
        if (strcmp(blindtransfer, ""))
        {
            if (strcmp(s->call->agi_dnid, dialed))
            {
                AGITool_exec(s->agi, s->res, "Playback", "silence/1");
                if (!strcmp(g_cluster_cfg.playtransfer, "YES"))
                {
                    AGITool_exec(s->agi, s->res, "Playback", "pls-hold-while-try");
                }

                if (g_cluster_cfg.bounce_alert[0] != '\0')
                {
                    AGITool_exec(s->agi, s->res, "SIPAddHeader", g_cluster_cfg.bounce_alert);
                }
                strlcpy(transferer, pBtr, sizeof(transferer));
                AGITool_set_priority(s->agi, s->res, 1);
                AGITool_set_extension(s->agi, s->res, transferer);
                AGITool_set_context(s->agi, s->res, s->call->myClusterContext);
                return;
            }
            else
            {
                if (g_cluster_cfg.blind_busy[0] != '\0')
                {
                    AGITool_set_priority(s->agi, s->res, 1);
                    AGITool_set_extension(s->agi, s->res, g_cluster_cfg.blind_busy);
                    AGITool_set_context(s->agi, s->res, s->call->myClusterContext);
                    return;
                }
                else
                {
                    AGITool_exec(s->agi, s->res, "Playtones", "busy");
                    AGITool_exec(s->agi, s->res, "Busy", "");
                    return;
                }
            }
        }
        else
        {
            AGITool_exec(s->agi, s->res, "Playtones", "busy");
            AGITool_exec(s->agi, s->res, "Busy", "");
            return;
        }
    }
    else
    {
        strlcat(vmflags, "b", sizeof(vmflags));
        AGITool_exec(s->agi, s->res, "Voicemail", strcat(vmbox, vmflags));
        return;
    }

    AGITool_exec(s->agi, s->res, "Playtones", "busy");
    AGITool_exec(s->agi, s->res, "Busy", "");
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
    AGITool_exec(s->agi, s->res, "Set", setcdrcmduser);

/**
 *  begin to set up the dialstring.
 *  Fleet only: keep tenant domain in the Request-URI so SBC usrloc is
 *  domain-aware (Dial(PJSIP/shortuid) alone uses AOR contact @VIP → 404
 *  when multiple tenants share one instance/setid). Singleton (no SBC)
 *  keeps Dial(PJSIP/shortuid) and uses the registered contact directly.
 */
    strlcpy(dialString, SIPDRIVER, sizeof(dialString));
    strlcat(dialString, "/", sizeof(dialString));
    strlcat(dialString, number, sizeof(dialString));
    if (pbx3_fleet_mode() && g_cluster_cfg.fqdn[0] != '\0')
    {
        strlcat(dialString, "/sip:", sizeof(dialString));
        strlcat(dialString, number, sizeof(dialString));
        strlcat(dialString, "@", sizeof(dialString));
        strlcat(dialString, g_cluster_cfg.fqdn, sizeof(dialString));
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
        strlcpy(recRet, SetRecord(s, number, "Inbound"), sizeof(recRet));
        strlcat(dialString, recRet, sizeof(dialString));
    }
/**
 *  If the dial definition has asked for MOH instead of ringback then set it here
 */
    AGITool_get_variable(s->agi, s->res, "MOH"); //set ON/OFF in inbound routes from the dial definition
    if (!strcmp(s->res->data, "YES"))
    {
        strlcat(dialString, "m", sizeof(dialString));
    }
/**
 *  Phase E (queue) + Phase G (LepDial): decide only — set PBX3_DIAL for
 *  dialplan Dial(${PBX3_DIAL}). Short-run AGI; dialplan owns the bridge.
 */
    AGITool_set_variable(s->agi, s->res, "PBX3_DIAL", dialString);
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
        snprintf(filework, sizeof(filework), "%d-%s-%s-%s", (int)time(&now), s->call->myCluster, s->call->agi_dnid, s->call->callerid);
        strlcat(filename, filework, sizeof(filename));

        /**
         * FILE refs MUST BE CHANGED *DONE*
         */
        snprintf(soundFile, sizeof(soundFile), " /var/spool/asterisk/monitor/%s/%s.wav", s->call->myCluster, filename);
        AGITool_exec(s->agi, s->res, "MixMonitor", soundFile);

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
            sprintf(vmbox,"%s@%s%s",s->call->agi_dnid,s->call->myCluster,vmflags);
            AGITool_exec(s->agi,s->res,"Voicemail",vmbox);
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
            AGITool_exec(s->agi, s->res, "Playback", "silence/1");
            if (!strcmp(g_cluster_cfg.playtransfer, "YES"))
            {
                AGITool_exec(s->agi, s->res, "Playback", "pls-hold-while-try");
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
        strcat(rdnis_string, s->call->callerid);
        AGITool_exec(s->agi, s->res, "Set", rdnis_string);
    }
/**
 *  and branch...
 */
        AGITool_set_priority(s->agi, s->res, 1);
        AGITool_set_extension(s->agi, s->res, cfnum);
        AGITool_set_context(s->agi, s->res, s->call->myClusterContext);
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

    strlcpy(toNum, GetExt(PARM_CMD), sizeof(toNum));

    DBPut(s, PARM_KEY, sipId, toNum);

    AGITool_exec(s->agi, s->res, "Playback", "call-forwarding");
    if (strcmp(toNum, ""))
    {
        AGITool_exec(s->agi, s->res, "Playback", "activated");
    }
    else
    {
        AGITool_exec(s->agi, s->res, "Playback", "de-activated");
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
    AGITool_exec(s->agi, s->res, "Playback", "call-forwarding");

    // 18 & 19 (ON/OFF) and 20 (toggle self) are the documented ones 

    if (!strncmp(PARM_CMD, "*18*", 3)) // toggle ON
    {
        DBPut(s, "cfim", sipId, fromNum);
        AGITool_exec(s->agi, s->res, "Playback", "activated");
    }
    else // toggle OFF
    {
        DBPut(s, "cfim", sipId, "");
        AGITool_exec(s->agi, s->res, "Playback", "de-activated");
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
    AGITool_exec(s->agi, s->res, "Playback", "call-forwarding");
    if (!strcmp(DBGet(s, "cfim", sipId), ""))
    {
        DBPut(s, "cfim", sipId, fromNum);
        AGITool_exec(s->agi, s->res, "Playback", "activated");
    }
    else
    {
        DBPut(s, "cfim", sipId, "");
        AGITool_exec(s->agi, s->res, "Playback", "de-activated");
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
    AGITool_exec(s->agi, s->res, "VMauthenticate", realFromNum);
    if (!atoi(s->res->result))
    {
        DBPut(s, "cfim", realFromNum, toNum);
        AGITool_exec(s->agi, s->res, "Playback", "activated");
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
    AGITool_exec(s->agi, s->res, "Playback", "call-forwarding");
    AGITool_exec(s->agi, s->res, "Playback", "de-activated");
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
    AGITool_exec(s->agi, s->res, "Playback", "silence/1");
    AGITool_exec(s->agi, s->res, "Playback", "activated");
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
        AGITool_set_variable(s->agi, s->res, "GROUP(inbound)", "inbound");
        AGITool_get_variable(s->agi, s->res, "GROUP_COUNT(inbound)"); // GROUP_COUNT(inbound) is the number of active inbound calls

        if (atoi(s->res->data) > atoi(g_cluster_cfg.maxin_str))
        {
            AGITool_exec(s->agi, s->res, "Playtones", "busy");
            AGITool_exec(s->agi, s->res, "Busy", "");
            return;
        }
    }

    // set the dialled number (DDI) in the CDR userfield
    strlcat(setcdrcmduser, PARM_KEY, sizeof(setcdrcmduser));
    AGITool_exec(s->agi, s->res, "Set", setcdrcmduser);

    if (g_cluster_cfg.dynamicfeatures[0] != '\0')
    {
        char df_ingress[640];
        snprintf(df_ingress, sizeof(df_ingress), "__DYNAMIC_FEATURES=%s", g_cluster_cfg.dynamicfeatures);
        AGITool_exec(s->agi, s->res, "Set", df_ingress);
    }

    sqlQueryBind1("SELECT technology,tag,inprefix,alertinfo,moh,swoclip FROM inroutes WHERE pkey=?", PARM_KEY);
    strlcpy(technology, rescols[0], sizeof(technology));
    strlcpy(tag, rescols[1], sizeof(tag));
    strlcpy(prefix, rescols[2], sizeof(prefix));
    strlcpy(alertinfo, rescols[3], sizeof(alertinfo));
    strlcpy(moh, rescols[4], sizeof(moh));
    strlcpy(swoclip, rescols[5], sizeof(swoclip));

    AGITool_set_variable(s->agi, s->res, "__MOH", moh);

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
        AGITool_exec(s->agi, s->res, "Set", clidstrng);
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
                    AGITool_set_priority(s->agi, s->res, 1);
                    AGITool_set_extension(s->agi, s->res, s->call->callerid);
                    AGITool_set_context(s->agi, s->res, "mainmenu");
                    return;
                }
            }
        }
    }

    // regular CLIP
    if (strcmp(tag, ""))
    {
        strlcpy(s->call->calleridname, tag, sizeof(s->call->calleridname));
        AGITool_exec(s->agi, s->res, "SetCallerPres", "allowed");
    }
    if (!strcmp(s->call->calleridname, "unknown"))
    {
        strlcpy(s->call->calleridname, "", sizeof(s->call->calleridname));
    }

    //	Alphatag
    if (strcmp(s->call->calleridname, ""))
    {
        snprintf(clidstrng, sizeof(clidstrng), "CALLERID(name)=%s", s->call->calleridname);
        AGITool_exec(s->agi, s->res, "Set", clidstrng);
    }

    if (strcmp(g_cluster_cfg.lterm_str, "YES"))
    {
        AGITool_answer(s->agi, s->res);
        // AGITool_exec(s->agi,s->res,"Ringing","");
    }
    // cause a ring for voip lines if requested
    if (!strcmp(technology, "SIP"))
    {
        if (strcmp(g_cluster_cfg.ringdelay_str, "0"))
        {
            AGITool_exec(s->agi, s->res, "Ringing", "");
            AGITool_exec(s->agi, s->res, "Wait", g_cluster_cfg.ringdelay_str);
        }
    }
    if (!strcmp(technology, "IAX2"))
    {
        if (strcmp(g_cluster_cfg.ringdelay_str, "0"))
        {
            AGITool_exec(s->agi, s->res, "Ringing", "");
            AGITool_exec(s->agi, s->res, "Wait", g_cluster_cfg.ringdelay_str);
        }
    }

    // distinctive ring (if present)
    if (strcmp(alertinfo, ""))
    {
        AGITool_exec(s->agi, s->res, "SIPAddHeader", alertinfo);
    }

    CheckState(s, PARM_KEY);
}

void CheckState(agi_session_t *s, char *remotenum)
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

    strlcpy(state, DBGet(s, "STAT", "OCSTAT"), sizeof(state));
/**
 *  If the master timer is closed we simply take the closed route...
 */
    if (!strcmp(state, "CLOSED"))
    {
        //   PBX is in hard CLOSED state - use closed route;
        AGITool_set_priority(s->agi, s->res, 1);
        AGITool_set_extension(s->agi, s->res, closeroute);
        AGITool_set_context(s->agi, s->res, s->call->myClusterContext);
    }
    else
/**
 *      If we get to here, the master timer is set to AUTO (usual state), or OPEN.
 *      So, now we can check the timers for the cluster and branch accordingly
 *      First check for hard closed...
 */
    {
        strlcpy(state, CheckTime(s, cluster), sizeof(state));
        if (!strcmp(state, "CLOSED"))
        {
            //   Closed(remotenum);
            AGITool_set_priority(s->agi, s->res, 1);
            AGITool_set_extension(s->agi, s->res, closeroute);
            AGITool_set_context(s->agi, s->res, s->call->myClusterContext);
        }
        else
/**
 *      So, the state must be OPEN/AUTO
 */
        {
            //    Open(remotenum);
            AGITool_set_priority(s->agi, s->res, 1);
            AGITool_set_extension(s->agi, s->res, openroute);
            AGITool_set_context(s->agi, s->res, s->call->myClusterContext);
        }
    }
}

char * CheckTime(agi_session_t *s, char *cluster)
{

    DebugFunctionTrace(__FUNCTION__);

    char clusterdboclo[8] = {'\0'};

    if (strcmp(g_cluster_cfg.routeoverride, ""))
    {
        strlcpy(closeroute, g_cluster_cfg.routeoverride, sizeof(closeroute));
        return "CLOSED";
    }

    strlcpy(clusterdboclo, DBGet(s, cluster, "OCSTAT"), sizeof(clusterdboclo));
    if (!strcmp(clusterdboclo, "CLOSED"))
    {
        return "CLOSED";
    }

    if (!strcmp(DBGet(s, s->call->myCluster, "OCSTAT"), "CLOSED"))
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
    AGITool_exec(s->agi, s->res, "NoOp", "NO MTIME - returning OPEN");
    return "OPEN";
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

        AGITool_exec(s->agi, s->res, "Wait", "0.5");
        AGITool_answer(s->agi, s->res);

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
            AGITool_stream_file(s->agi, s->res, msg, optionStr, 0);
            if (strcmp(optionStr, "") && atoi(s->res->result))
            {
                snprintf(dtmf, sizeof(dtmf), "%c", atoi(s->res->result));
                IVRAction(s, PARM_KEY, dtmf);
                return;
            }
            if (strcmp(optionStr, ""))
            {
                AGITool_stream_file(s->agi, s->res, ivrsilence, optionStr, 0);
                if (strcmp(optionStr, "") && atoi(s->res->result))
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
            AGITool_get_data(s->agi, s->res, msg, ivrdigitwait, 4);
            if (atoi(s->res->result))
            {
                // check if it's an extension
                snprintf(dtmf, sizeof(dtmf), "%s", s->res->result);
                if (strlen(dtmf) > 1)
                {
                    AGITool_set_priority(s->agi, s->res, 1);
                    AGITool_set_extension(s->agi, s->res, dtmf);
                    AGITool_set_context(s->agi, s->res, s->call->myClusterContext);
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
        // set Alert-info if present
        snprintf(myQuery, sizeof(myQuery), "SELECT %s FROM ivrmenu WHERE pkey=?", dbAlert);
        strlcpy(alert, sqlQueryBind1(myQuery, menu), sizeof(alert));
        if (strcmp(alert, ""))
        {
            AGITool_exec(s->agi, s->res, "SIPAddHeader", alert);
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
        snprintf(clidstrng, sizeof(clidstrng), "CALLERID(name)=%s", s->call->calleridname);
        AGITool_exec(s->agi, s->res, "Set", clidstrng);
    }
    //  Route it
    if (strcmp(action, "None"))
    {
        AGITool_set_priority(s->agi, s->res, 1);
        AGITool_set_extension(s->agi, s->res, action);
        AGITool_set_context(s->agi, s->res, s->call->myClusterContext);
        return;
    }
    // bad key press?
    return;
}

char * DBGet(agi_session_t *s, char *family, char *key)
{

    DebugFunctionTrace(__FUNCTION__);

    AGITool_database_get(s->agi, s->res, family, key);
    return s->res->data;
}

void DBPut(agi_session_t *s, char *family, char *key, char *val)
{

    DebugFunctionTrace(__FUNCTION__);

    AGITool_database_put(s->agi, s->res, family, key, val);
}

void DBDel(agi_session_t *s, char *family, char *key)
{

    DebugFunctionTrace(__FUNCTION__);

    AGITool_database_del(s->agi, s->res, family, key);
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
    AGITool_exec(s->agi, s->res, "Set", abstimeout);
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
        AGITool_verbose(s->agi, s->res, vmsg, 1);
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
    AGITool_get_variable(s->agi, s->res, "ANSWEREDTIME"); //Asterisk varaiable set when a queue ends
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

	// backstop CLID if all else fails - take the trunk's CLID (if it exists)
	strcpy(clidline, sqlQueryBind1("SELECT callerid FROM trunks WHERE pkey=?", key));
	if (strcmp(clidline, "")) {
		strcpy(clidwork, clidline);
		sprintf (vmsg,"trunks CLID  %s found for outbound call, using key %s", clidwork, key);
		AGITool_verbose(s->agi,s->res,vmsg,1);
	}
	else {
		sprintf (vmsg,"No trunks CLID found for outbound call, using key %s", key);
		AGITool_verbose(s->agi,s->res,vmsg,1);
	}


	// If there is a cluster CLID then it trumps the line 
	if (strcmp(s->call->myClusterclid, "")) {
		sprintf (vmsg,"Cluster CLID %s found", s->call->myClusterclid);
		AGITool_verbose(s->agi,s->res,vmsg,1);
		strcpy(clidwork, s->call->myClusterclid);
	}
	else {
		sprintf (vmsg,"no cluster CLID found for outbound call, using key %s", key);
		AGITool_verbose(s->agi,s->res,vmsg,1);
	}


	//if there is an extension CLID or RDNIS CLID then it trumps the line and cluster CLID 
	if (s->call->caller_is_local) {
		strcpy(clidphone, sqlQueryBind1("SELECT callerid FROM IPphone WHERE pkey=?", s->call->callerid));		
	}
	// if the RDNIS is local, set its CLID into clidphone
	else if (s->call->rdnis_is_local) {
		strcpy(clidphone, sqlQueryBind1("SELECT callerid FROM IPphone WHERE pkey=?", s->call->rdnis));
	}

	// Only take the extension clid if it is longer than 5 characters (i.e. - not an extension number)
	// we do not want to send an extension number CLID onto the PSTN
	if (strcmp(clidphone, "") && (strlen(clidphone) > 5)) {
		strcpy(clidwork, clidphone);
		sprintf (vmsg,"Extension CLID %s found for outbound call, using key %s", clidwork, key);
		AGITool_verbose(s->agi,s->res,vmsg,1);
	}
	else {
		sprintf (vmsg,"No PSTN extension CLID found for outbound call, using clid %s", clidphone);
		AGITool_verbose(s->agi,s->res,vmsg,1);
	}

	sprintf (vmsg,"Phase 1 CLID is %s for outbound call, using key %s", clidwork, key);
	AGITool_verbose(s->agi,s->res,vmsg,1);

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
			sprintf (vmsg,"Using CLID %s for outbound call, using key %s", clidwork, key);
			AGITool_verbose(s->agi,s->res,vmsg,1);
			sprintf(clidstrng,"CALLERID(number)=%s",clidwork);
			AGITool_exec(s->agi,s->res,"Set", clidstrng);		
		
			// if the RDNIS is set (which covers local diversions) then reset it to the CLID to make it safe.  
			// It will look odd because the DIVERT header will be the same as the FROM header but it is necessary 
			// in the case of a local call which already has RDNIS set.  This will happen if the diversion has come from a phone
			// (rather than a pbx divert, e.g. *21*) and the phone has set the RDNIS to the original callerid.
		
			if (s->call->rdnis_is_set) {
				sprintf (vmsg,"Using CLID %s to override local RDNIS", clidwork);
				AGITool_verbose(s->agi,s->res,vmsg,1);
				sprintf(clidstrng,"CALLERID(RDNIS)=%s",clidwork);
				AGITool_exec(s->agi,s->res,"Set", clidstrng);
			}
		}
		// if the inbound callerid is a real number then this is a hairpin call so set the RDNIS to our CLID to create a 
		// diversion header.
		
		else {
			sprintf(clidstrng,"CALLERID(RDNIS)=%s",clidwork);
			AGITool_exec(s->agi,s->res,"Set", clidstrng);
		}
	}   
	else {
		// we have no CLID to set so just log it
		sprintf (vmsg,"No override CLID found for outbound call, using key %s", key);
		AGITool_verbose(s->agi,s->res,vmsg,1);
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
