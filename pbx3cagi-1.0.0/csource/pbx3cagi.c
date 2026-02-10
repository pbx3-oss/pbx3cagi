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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "pbx3cagi.h"
#include "cagi.h"
#include <bsd/string.h>
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

long debug = FALSE;
int abstimeint = 14400; // default (4 hours)						// debug on/off
int myargc;             // numargs
int switchdig;          // function number
int callee_is_local = FALSE;
int caller_is_local = FALSE;
int caller_is_local_ext = FALSE;
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

int main(int argc, char **argv)
{

    char numwork[32] = {'\0'};
    char chardig[4] = {'\0'};

    char *cmdTab[11] =
        {"OutTrunk", "OutRoute", "", "",
         "InCall", "Inbound", "Dial", "", "IVR", "", "OutQmt"};
    int i = 0;
    char relation[32] = {'\0'};

    myargv = argv; // agi srgs
    myargc = argc; // agi arg count

    AGITool_Init(&agi);

    AGITool_get_variable(&agi, &res, "DEBUG");
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


	DBQuery("master_xref", "pkey", callerid, "relation");	   	
	strcpy(relation, rescols[0]); 

	if (strcmp(relation,"")) {	
		if (!strcmp(relation,"IPphone")) {
			caller_is_local_ext = TRUE;
		}
		if (!strcmp(relation,"IPphone") || !strcmp(relation,"queue")) {
			caller_is_local = TRUE;	
		}			
	}

	DBQuery("master_xref", "pkey", extension, "relation");	   	
	strcpy(relation, rescols[0]); 

	if (strcmp(relation,"")) {		
		if (!strcmp(relation,"IPphone") || !strcmp(relation,"speed")) {
			callee_is_local = TRUE;
		}	
	}
	
	if (strcmp(rdnis, "unknown")) { 
		if (strcmp(DBQuery("IPphone", "pkey", rdnis, "pkey"),"")) {
			rdnis_is_local = TRUE;
		}
		rdnis_is_set = TRUE;
	}

    // set the cluster  	
	SetCluster();
	    
    strlcpy (myClusterId,DBQuery("cluster", "pkey", myCluster, "id"),sizeof(myClusterId));
    strlcpy (myClusterContext, myCluster, sizeof(myClusterContext));
    
    if (!strcmp(myCluster,"default")) {
      strlcpy (myClusterContext, "qrxvtmny", sizeof(myClusterContext));
    }

    AGITool_get_variable(&agi, &res, "ABSTIMEOUT");
    if (strcmp(res.data, ""))
    {
        abstimeint = atoi(res.data);
    }

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
        snprintf(vmsg, sizeof(vmsg), "PARM_KEY is %s", PARM_KEY);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        snprintf(vmsg, sizeof(vmsg), "PARM_PM1 is %s", PARM_PM1);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        snprintf(vmsg, sizeof(vmsg), "PARM_PM2 is %s", PARM_PM2);
        DebugFunctionMsg(__FUNCTION__, vmsg);
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
        OutTrunk();
        break;
    case 2:
        OutRoute();
        break;
/**
    case 3:
        OutCos(); // no longer used
        break;
*/
/*
    case 4:             // no longer used
        OutCluster(); 
        break;
*/
    case 5:
        InCall();
        break;
    case 6:
        Inbound();
        break;

    case 7:
        Dial(PARM_KEY,PARM_PM1,"","");
        break;

/**    case 8:
        Alias();    //moved to queues
        break;
*/
    case 9:
        IVR();      //IVR menus
        break;
/**
    case 10:
        hangUp();   //not used
        break;
*/
    case 11:
        OutQmt();   //ACD Queues, ring groups and pages
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
    case 28:
        CFToggle();
        break;
    case 29:
        CFToggle();
        break;

// Master timers (whole system)
    case 30:
        SetTimer();
        break;
    case 31:
        SetTimer();
        break;
    case 32:
        SetTimer();
        break;
/**
 * No longer used

    case 33:
        SetTimer();
        break;
    case 34:
        SetTimer();
        break;
    case 35:
        SetTimer();
        break;
 */   

    case 38:
        CFToggle();
        break;
    case 39:
        CFToggle();
        break;
/*        
    case 40:    // - no longer supported 
        Page();
        break;
  Old PVU codes no longer used
    case 41:
        CFVMailSet();
        break;
    case 42:
        CFVMailSet();
        break;
+/ 
        //
        //  ALL *5X* codes have been moved into the main dialplan.
        //  They are all direct asterisk calls so there was nothing
        //  to be gained from loading the AGI to deal with them
        //
        //    DO NOT USE *5X* Codes without checking extensions.conf
        //

    case 60:
        RecGreet();
        break;
        /* *61* has moved into extensions.conf
                case 61:
                    PlayGreet();
                    break;
        */
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


void SetCluster()
{
    //
    // Guarantees a cluster will be set.
    // In some cases this may be "default" if we can't locate a real cluster.
    //
    // Downstream trunks should be started in the cluster context to which they belong
    // - e.g context=someclustername
    //

    DebugFunctionTrace(__FUNCTION__);

    char setcdrcmd[64] = "CHANNEL(accountcode)=";

    //
    // two cases can change an already set accountcode (cluster) :-
    // - an alias (where it has been invoked from an external caller) or
    // - a call-divert from an external number to an external number via *21* or a SIP divert
    //
    // I don't think these can happen in the new regime
    //

    if (!strcmp(context, "mainmenu"))
    {
        strlcpy(myCluster, PARM_PM1, sizeof(myCluster));
        if (strcmp(myCluster, ""))
        {
            strlcat(setcdrcmd, myCluster, sizeof(setcdrcmd));
        }
        else
        {
            strlcat(setcdrcmd, "default", sizeof(setcdrcmd));
            strlcpy(myCluster, "default", sizeof(myCluster));
        }
        AGITool_exec(&agi, &res, "Set", setcdrcmd);

        snprintf(vmsg, sizeof(vmsg), "Phase Mainmenu Assigned a cluster of %s with PARM_CMD %s and PARM_PM1 %s", myCluster, PARM_KEY, PARM_PM1);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return;
    }

    if (strcmp(context, "internal"))
    {
        if (!strcmp(context, "qrxvtmny"))
        {
            strlcat(setcdrcmd, "default", sizeof(setcdrcmd));
            strlcpy(myCluster, "default", sizeof(myCluster));
        }
        else
        {
            strlcat(setcdrcmd, context, sizeof(setcdrcmd));
            strlcpy(myCluster, context, sizeof(myCluster));
        }
        AGITool_exec(&agi, &res, "Set", setcdrcmd);
        snprintf(vmsg, sizeof(vmsg), "Phase internal Assigned a cluster of %s with PARM_CMD %s and PARM_KEY %s", myCluster, PARM_CMD, PARM_KEY);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return;
    }

    if (strcmp(myCluster, ""))
    {

        snprintf(vmsg, sizeof(vmsg), " Assigned no cluster with PARM_CMD %s (Already set to %s)", PARM_CMD, myCluster);
        DebugFunctionMsg(__FUNCTION__, vmsg);

        return;
    }

    //
    // If we get here we have old V6 downstream trunks using "internal" context
    //

    snprintf(vmsg, sizeof(vmsg), "Old internal context detected for channel %s)", channel);
    AGITool_verbose(&agi, &res, vmsg, 1);

    // Can we do a cluster lookup in trunks?
    if (switchdig == 1 || switchdig == 4 || switchdig == 6)
    {
        strlcpy(myCluster, DBQuery("lineIO", "pkey", PARM_KEY, "cluster"), sizeof(myCluster));
        if (strcmp(myCluster, ""))
        {
            strlcat(setcdrcmd, myCluster, sizeof(setcdrcmd));
        }
        else
        {
            strlcat(setcdrcmd, "default", sizeof(setcdrcmd));
            strlcpy(myCluster, "default", sizeof(myCluster));
        }
        AGITool_exec(&agi, &res, "Set", setcdrcmd);

        snprintf(vmsg, sizeof(vmsg), "Phase Trunkname Assigned a cluster of %s with PARM_KEY %s", myCluster, PARM_KEY);
        DebugFunctionMsg(__FUNCTION__, vmsg);

        return;
    }

    // OK - it's from outside so we need to look at the channel	to identify the DDI or trunk

    strlcpy(myCluster, DBQuery("lineIO", "pkey", chanId, "cluster"), sizeof(myCluster));
    if (strcmp(myCluster, ""))
    {
        strlcat(setcdrcmd, myCluster, sizeof(setcdrcmd));
    }
    else
    {
        strlcat(setcdrcmd, "default", sizeof(setcdrcmd));
    }
    AGITool_exec(&agi, &res, "Set", setcdrcmd);
    snprintf(vmsg, sizeof(vmsg), "Phase Channel Assigned a cluster of %s with PARM_KEY %s", myCluster, PARM_KEY);
    AGITool_verbose(&agi, &res, vmsg, 1);
    return;
}

void setMoh()
{

    DebugFunctionTrace(__FUNCTION__);

    char setmohcmd[64] = "CHANNEL(musicclass)=moh-";
    char mohfolder[64] = "/usr/share/asterisk/moh-";
    char cmd[1024];
    int status, exitcode;

    strlcat(mohfolder, myCluster, sizeof(mohfolder));

    if (strcmp(DBQuery("cluster", "pkey", myCluster, "usemohcustom"), "YES"))
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
    for (i = 0; i < (strlen(tmp) - 4); i++)
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
            for (k = 0; k < strlen(operand); k++)
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
    if (Authenticate("SYSPASS") != 0)
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
        // AGITool_exec(&agi,&res,"Playback","beep");
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

/*
 * This code is no longer used in V7.
void PlayGreet() {

    DebugFunctionTrace(__FUNCTION__);

    char greetFile[MAX_FILENAME_LEN] = SOUNDIR"/usergreeting";
    char greetFile2[MAX_FILENAME_LEN] = {'\0'};
    char ext[MAX_EXT_LEN] = {'\0'};
    FILE *greeting = NULL;

    strlcpy (ext, GetExt(PARM_CMD),sizeof(ext));
    strlcat(greetFile, ext, sizeof(greetFile));
    strlcpy(greetFile2, greetFile, sizeof(greetFile2));
    strlcat(greetFile, ".wav", sizeof(greetFile));
    greeting = fopen(greetFile, "r");
    if (greeting == NULL) {
        AGITool_exec(&agi,&res,"Playback","invalid");
    }
    else {
        AGITool_exec(&agi,&res,"Playback",greetFile2);
    }
    fclose(greeting);
}

*/

int Authenticate(char *password)
{

    DebugFunctionTrace(__FUNCTION__);

    char syspass[32] = {'\0'};

    // get password from DB

    // pSyspass = DBQuery("globals", "pkey", "global", password);
    //  added to replace line above  16/6/10
    AGITool_get_variable(&agi, &res, password);
    strlcpy(syspass, res.data, sizeof(syspass));
    if (strcmp(syspass, ""))
    {
        AGITool_exec(&agi, &res, "Playback", "silence/1");
        AGITool_exec(&agi, &res, "Authenticate", syspass);
        return atoi(res.result);
    }
    else
    {
        snprintf(vmsg, sizeof(vmsg), "Unable to find password in the database.");
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return -1;
    }
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
            if (strcmp(DBQuery("agent", "pkey", agent, "pkey"), ""))
            {
                strlcpy(agentpasswd, DBQuery("agent", "pkey", agent, "passwd"), sizeof(agentpasswd));
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
                        AGITool_get_variable(&agi, &res, "EPOCH");
                        strlcpy(epoch, res.data, sizeof(epoch));
                        for (i = 1; i < 7; i++)
                        {
                            snprintf(agentqueue, sizeof(agentqueue), "queue%i", i);
                            strlcpy(queuename, DBQuery("agent", "pkey", oldagent, agentqueue), sizeof(queuename));
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
                        strlcpy(queuename, DBQuery("agent", "pkey", agent, agentqueue), sizeof(queuename));
                        if (strcmp(queuename, "None"))
                        {
                            snprintf(queuearg, sizeof(queuearg), "%s,%s,,,Agent/%s",
                                     queuename, agentchan, agent);
                            AGITool_exec(&agi, &res, "AddQueueMember", queuearg);
                        }
                    }
                    AGITool_get_variable(&agi, &res, "EPOCH");
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
    strlcpy(agentpasswd, DBQuery("agent", "pkey", agent, "passwd"), sizeof(agentpasswd));

    // check if they are logged in - if not just exit

    if (!strcmp(agent, ""))
    {
        AGITool_exec(&agi, &res, "Playback", "agent-loggedoff");
        return;
    }

    AGITool_get_variable(&agi, &res, "EPOCH");
    strlcpy(epoch, res.data, sizeof(epoch));
    strlcpy(startepoch, DBGet(f_dynLogin, agent), sizeof(startepoch));
    //	AGITool_exec(&agi,&res,"Authenticate",agentpasswd);
    //	if (atoi(res.result)==0) {
    for (i = 1; i < 7; i++)
    {
        snprintf(agentqueue, sizeof(agentqueue), "queue%i", i);
        strlcpy(queuename, DBQuery("agent", "pkey", agent, agentqueue), sizeof(queuename));
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

    if (Authenticate("SPYPASS") != 0)
    {
        return;
    }
    AGITool_exec(&agi, &res, "ChanSpy", "Agent");
}

void ChanSpyWhisper()
{

    DebugFunctionTrace(__FUNCTION__);

    char ext[MAX_EXT_LEN] = {'\0'};
    char options[32] = {'\0'};
    if (Authenticate("SPYPASS") != 0)
    {
        return;
    }
 //   strlcpy(ext, myClusterId, sizeof(ext));
    strlcat(ext, GetExt(PARM_CMD), sizeof(ext));
    strlcpy(options, SIPDRIVER, sizeof(options));
    strlcat(options, ext, sizeof(options));
    strlcat(options, ",qw", sizeof(options));
    AGITool_exec(&agi, &res, "ChanSpy", options);
}

void ChanSpy()
{

    DebugFunctionTrace(__FUNCTION__);

    char ext[MAX_EXT_LEN] = {'\0'};
    char options[32] = {'\0'};
    if (Authenticate("SPYPASS") != 0)
    {
        return;
    }
//    strlcpy(ext, myClusterId, sizeof(ext));
    strlcat(ext, GetExt(PARM_CMD), sizeof(ext));
    strlcpy(options, SIPDRIVER, sizeof(options));
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
    char clusterChanmax[8] = {'\0'};
    char clusterGroup[64] = {'\0'};
    char clusterCount[64] = {'\0'};
    char clusterAbstimeout[16] = {'\0'};
    char extenAbstimeout[16] = {'\0'};
    //    char rgrpCluster[MAX_CLUSTER_LEN] = {'\0'};
    char carriertype[12] = {'\0'};
    char carrier[12] = {'\0'};
    char beep[4] = {'\0'};
    char busy[4] = {'\0'};
    char congested[4] = {'\0'};
    char set_dynamic_features[4] = "NO";
    int i;
    int last;

    DBQuery("cluster", "pkey", myCluster, "chanmax,abstimeout,clusterclid");
    strlcpy(clusterChanmax, rescols[0], sizeof(clusterChanmax));
    strlcpy(clusterAbstimeout, rescols[1], sizeof(clusterAbstimeout));
    strlcpy(myClusterclid, rescols[2], sizeof(myClusterclid));

    // Set dynamic features if necessary
    AGITool_get_variable(&agi, &res, "SET_DYNAMIC_FEATURES");
    strlcpy(set_dynamic_features, res.data, sizeof(set_dynamic_features));
    if (!strcmp(set_dynamic_features, "YES"))
    {
        AGITool_exec(&agi, &res, "Set", "__DYNAMIC_FEATURES=clear#outpause#outresume");
    }

    /*
     * set timeout to the clusterAbstimeout (if present)...
     * ...and check if the cluster is barred (clusterAbstimeout=0)
     *
     */
    if (strcmp(clusterAbstimeout, ""))
    {
        if (atoi(clusterAbstimeout) == 0)
        {
            AGITool_exec(&agi, &res, "Playtones", "busy");
            AGITool_exec(&agi, &res, "Busy", "");
            return;
        }
        abstimeint = atoi(clusterAbstimeout);
    }

    /*
     * set timeout to the extenAbstimeout (if present)...
     * ...and check if the exten is barred (extenAbstimeout=0)
     *
     */
    if (caller_is_local_ext)
    {
        snprintf(myQuery, sizeof(myQuery), "SELECT abstimeout from ipphone WHERE pkey='%s' AND cluster='%s'", extension, myCluster);
        sqlQuery(myQuery);
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
    if (strcmp(clusterChanmax, ""))
    {
        snprintf(clusterGroup, sizeof(clusterGroup), "GROUP(%s)", myCluster);
        AGITool_set_variable(&agi, &res, clusterGroup, myCluster);
        snprintf(clusterCount, sizeof(clusterCount), "GROUP_COUNT(%s)", myCluster);
        AGITool_get_variable(&agi, &res, clusterCount);
        if (atoi(res.data) > atoi(clusterChanmax))
        {
            AGITool_exec(&agi, &res, "Playtones", "busy");
            AGITool_exec(&agi, &res, "Busy", "");
            return;
        }
    }

    DBQuery("Route", "pkey", PARM_KEY, "auth,path1,path2,path3,path4,alternate,strategy");

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

    AGITool_get_variable(&agi, &res, "PLAYBEEP");
    strlcpy(beep, res.data, sizeof(beep));
    AGITool_get_variable(&agi, &res, "PLAYBUSY");
    strlcpy(busy, res.data, sizeof(busy));
    AGITool_get_variable(&agi, &res, "PLAYCONGESTED");
    strlcpy(congested, res.data, sizeof(congested));
    /*
     * set the first path (if we are balancing)
     */
    last = 0;
    if (!strncmp(strategy, "balance", 7))
    {
        AGITool_get_variable(&agi, &res, PARM_KEY);
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
        DBQuery("lineIO", "pkey", path[last], "carrier,active");
        strlcpy(carrier, rescols[0], sizeof(carrier));
        strlcpy(active, rescols[1], sizeof(active));

        DBQuery("Carrier", "pkey", carrier, "carriertype,technology");
        strlcpy(carriertype, rescols[0], sizeof(carriertype));
        strlcpy(technology, rescols[1], sizeof(technology));

        if (!strcmp(active, "YES"))
        {
            if (!strncmp(strategy, "balance", 7))
            {
                snprintf(setlast, sizeof(setlast), "GLOBAL(%s)=%d", PARM_KEY, last);
                AGITool_exec(&agi, &res, "Set", setlast);
            }
            else if (strcmp(path[last], "None"))
            {
                OutVoip(path[last]);
            }
        }
    
        AGITool_get_variable(&agi, &res, "DIALSTATUS");
        if (!strcmp(res.data, "ANSWER"))
        {
            return;
        }
        if (!strcmp(res.data, "BUSY"))
        {
            if (!strncmp(busy, "YES", 3))
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

        if (!strncmp(beep, "YES", 3) && strcmp(path[last], "None"))
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

    if (!strncmp(congested, "YES", 3))
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


void OutTrunk()
{

    DebugFunctionTrace(__FUNCTION__);

    char busy[4] = {'\0'};
    char congested[4] = {'\0'};
    char active[4] = {'\0'};
    char technology[64] = {'\0'};

    AGITool_get_variable(&agi, &res, "PLAYBUSY");
    strlcpy(busy, res.data, sizeof(busy));
    AGITool_get_variable(&agi, &res, "PLAYCONGESTED");
    strlcpy(congested, res.data, sizeof(congested));

    DBQuery("lineIO", "pkey", PARM_KEY, "active,technology");
    strlcpy(active, rescols[0], sizeof(active));
    strlcpy(technology, rescols[1], sizeof(technology));

    if (!strcmp(active, "YES"))
    {
        OutVoip();
        AGITool_get_variable(&agi, &res, "DIALSTATUS");
        if (!strcmp(res.data, "ANSWER"))
        {
        }
        else if (!strcmp(res.data, "BUSY"))
        {
            if (!strncmp(busy, "YES", 3))
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
            if (!strncmp(congested, "YES", 3))
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

void OutVoip()
{

    DebugFunctionTrace(__FUNCTION__);

    char dialString[MAX_DIALSTR_LEN] = {'\0'};
    char carrier[128] = {'\0'};
    char carriertype[32] = {'\0'};
    char technology[128] = {'\0'};
    char username[128] = {'\0'};
    char voipnum[128] = {'\0'};
    char peername[128] = {'\0'};
    char desc[128] = {'\0'};
    char callprogress[4] = {'\0'};
    char transform[MAX_TRANSFORM_LIST_LEN] = {'\0'};
    char number[MAX_EXT_LEN] = {'\0'};
    char preSel[MAX_PRESEL_LEN] = {'\0'};
    char voipmax[8] = {'\0'};
    char allowhashxfer[16] = {'\0'};
    char recRet[8] = {'\0'};
    char cfwdprogress[4] = {'\0'};
    char cfwdanswer[4] = {'\0'};

    DBQuery("lineIO", "pkey", PARM_KEY, "carrier,username,remotenum,peername,callprogress,desc,transform,match");
    strlcpy(carrier, rescols[0], sizeof(carrier));
    strlcpy(username, rescols[1], sizeof(username));
    strlcpy(voipnum, rescols[2], sizeof(voipnum));
    strlcpy(peername, rescols[3], sizeof(peername));
    strlcpy(callprogress, rescols[4], sizeof(callprogress));
    strlcpy(desc, rescols[5], sizeof(desc));
    strlcpy(transform, rescols[6], sizeof(transform));
    strlcpy(preSel, rescols[7], sizeof(preSel));

    DBQuery("carrier", "pkey", carrier, "carriertype,technology");
    strlcpy(carriertype, rescols[0], sizeof(carriertype));
    strlcpy(technology, rescols[1], sizeof(technology));

    if (!strcmp(peername, ""))
    {
        strlcpy(peername, DBQuery("lineIO", "pkey", PARM_KEY, "desc"), sizeof(peername));
    }

    AGITool_get_variable(&agi, &res, "VOIPMAX");
    strlcpy(voipmax, res.data, sizeof(voipmax));
    AGITool_get_variable(&agi, &res, "ALLOWHASHXFER");
    strlcpy(allowhashxfer, res.data, sizeof(allowhashxfer));

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
        if (strcmp(carrier, "SailToSail") && strcmp(carrier, "InterSARK"))
    {
        outboundClip(PARM_KEY);
    }
    // ENDCLIP

    strlcat(dialString, ASTDLIM, sizeof(dialString));
    strlcat(dialString, ASTDLIM, sizeof(dialString));

    if (!strcmp(allowhashxfer, "enabled"))
    {
        strlcat(dialString, "T", sizeof(dialString));
    }
    // early media
    if (!strcmp(callprogress, "YES"))
    {
        strlcat(dialString, "r", sizeof(dialString));
    }
    DBQuery("globals", "pkey", "global", "CFWDPROGRESS,CFWDANSWER");
    strlcpy(cfwdprogress, rescols[0], sizeof(cfwdprogress));
    strlcpy(cfwdanswer, rescols[1], sizeof(cfwdanswer));
    if (rdnis_is_set)
    {
        if (!strcmp(cfwdprogress, "enabled"))
        {
            strlcat(dialString, "r", sizeof(dialString));
        }
        if (!strcmp(cfwdanswer, "enabled"))
        {
            AGITool_answer(&agi, &res);
        }
    }

    AGITool_set_variable(&agi, &res, "GROUP()", "OUTBOUND_GROUP");
    AGITool_get_variable(&agi, &res, "GROUP_COUNT()");
    if (atoi(res.data) <= atoi(voipmax))
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

void InCall()
{

    DebugFunctionTrace(__FUNCTION__);

    char vmbox[64] = {'\0'};
    char blindtransfer[MAX_EXT_LEN] = {'\0'};
    char blindbusy[MAX_EXT_LEN] = {'\0'};
    char transferer[MAX_EXT_LEN] = {'\0'};
    char cellphone[MAX_EXT_LEN] = {'\0'};
    char celltwindial[MAX_EXT_LEN] = {'\0'};
    char *pBtr = &blindtransfer[4];
    char calleridsave[MAX_EXT_LEN] = {'\0'};
    char agi_dnid[MAX_EXT_LEN] = {'\0'};
    char dstatus[16] = {'\0'};
    char vmflags[4] = {'\0'};
    char voiceinstr[8] = {'\0'};
    char calledCluster[MAX_CLUSTER_LEN] = {'\0'};
    char includeClusters[8192] = {'\0'};
    char extalert[128] = {'\0'};
    char bouncealert[128] = {'\0'};
    char transfer[4] = {'\0'};

    strlcpy(agi_dnid, AGITool_ListGetVal(agi.agi_vars, "agi_dnid"), sizeof(agi_dnid));
    AGITool_get_variable(&agi, &res, "BLINDTRANSFER");
    strlcpy(blindtransfer, res.data, sizeof(blindtransfer));

    //    DBQuery("IPphone", "pkey", extension, "dvrvmail,extalert,cluster");
    snprintf(myQuery, sizeof(myQuery), "SELECT dvrvmail,extalert,cluster from ipphone WHERE id='%s' AND cluster='%s'", extension, myCluster);
    sqlQuery(myQuery);

    strlcpy(vmbox, rescols[0], sizeof(vmbox));
    strlcpy(extalert, rescols[1], sizeof(extalert));
    strlcpy(calledCluster, rescols[2], sizeof(calledCluster));

    if (strcmp(vmbox, "None"))
    {
        strlcat(vmbox, "@", sizeof(vmbox));
        strlcat(vmbox, calledCluster, sizeof(vmbox));
    }

    AGITool_get_variable(&agi, &res, "VOICEINSTR");
    strlcpy(voiceinstr, res.data, sizeof(voiceinstr));
    strlcpy(vmflags, ASTDLIM, sizeof(vmflags));
    if (!strcmp(voiceinstr, "NO"))
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
                    AGITool_get_variable(&agi, &res, "PLAYTRANSFER");
                    strlcpy(transfer, res.data, sizeof(transfer));
                    if (!strcmp(transfer, "YES"))
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
        return;
    }

    if (strcmp(extalert, ""))
    {
        AGITool_exec(&agi, &res, "SIPAddHeader", extalert);
    }

    //***************************************
    AGITool_set_variable(&agi, &res, "__PICKUPMARK", extension);
    // deal with cellphone twinning
    strlcpy(cellphone, DBGet("srktwin", extension), sizeof(cellphone));
    if (strcmp(cellphone, ""))
    {
        strlcpy(celltwindial, "&Local/", sizeof(celltwindial));
        strlcat(celltwindial, cellphone, sizeof(celltwindial));
        strlcat(celltwindial, "@", sizeof(celltwindial));
        strlcat(celltwindial, myClusterContext, sizeof(celltwindial));
    }
    Dial(extension, "", celltwindial, vmbox);
    //***************************************

    /*
     * read any stuff hanging around in the pipe
     */
    //	AGITool_Init(&agi);

    AGITool_get_variable(&agi, &res, "DIALSTATUS");
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
                    AGITool_get_variable(&agi, &res, "BOUNCEALERT");
                    strlcpy(bouncealert, res.data, sizeof(bouncealert));
                    if (strcmp(bouncealert, ""))
                    {
                        AGITool_exec(&agi, &res, "SIPAddHeader", bouncealert);
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
                AGITool_get_variable(&agi, &res, "PLAYTRANSFER");
                strlcpy(transfer, res.data, sizeof(transfer));
                if (!strcmp(transfer, "YES"))
                {
                    AGITool_exec(&agi, &res, "Playback", "pls-hold-while-try");
                };

                AGITool_get_variable(&agi, &res, "BOUNCEALERT");
                strlcpy(bouncealert, res.data, sizeof(bouncealert));
                if (strcmp(bouncealert, ""))
                {
                    AGITool_exec(&agi, &res, "SIPAddHeader", bouncealert);
                }
                strlcpy(transferer, pBtr, sizeof(transferer));
                AGITool_set_priority(&agi, &res, 1);
                AGITool_set_extension(&agi, &res, transferer);
                AGITool_set_context(&agi, &res, myClusterContext);
                return;
            }
            else
            {
                AGITool_get_variable(&agi, &res, "BLINDBUSY");
                strcpy(blindbusy, res.data);
                if (strcmp(blindbusy, ""))
                {
                    AGITool_set_priority(&agi, &res, 1);
                    AGITool_set_extension(&agi, &res, blindbusy);
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

void Dial(char *number, char *type, char *twin, char *vmbox)
{

    DebugFunctionTrace(__FUNCTION__);

    char dialString[MAX_DIALSTR_LEN] = {'\0'};
    char intRingDelay[MAX_RING_LEN] = {'\0'};
    char userRingDelay[MAX_RING_LEN] = {'\0'};
    char recRet[8] = {'\0'};
    char agi_type[32] = {'\0'};

    strlcpy(dialString, "PJSIP", sizeof(dialString));
    strlcat(dialString, "/", sizeof(dialString));
    strlcat(dialString, number, sizeof(dialString));

    if (strcmp(type, "queue"))
    {
        strlcpy(userRingDelay, DBGet("ringdelay", number), sizeof(userRingDelay));
        if (!strcmp(vmbox, "None"))
        {
            strlcpy(intRingDelay, "", sizeof(intRingDelay));
        }
        // this is for followme calls which get sent through the local loop -
        // it stops local phones from coming off the delay and dropping to vmail
        else if (!strcmp(agi_type, "Local"))
        {
            strlcpy(intRingDelay, "", sizeof(intRingDelay));
        }
        else if (!strcmp(userRingDelay, ""))
        {
            AGITool_get_variable(&agi, &res, "INTRINGDELAY");
            strlcpy(intRingDelay, res.data, sizeof(intRingDelay));
        }
        else if (!strcmp(userRingDelay, "0"))
        {
            strlcpy(intRingDelay, "", sizeof(intRingDelay));
        }

        else
        {
            strlcpy(intRingDelay, userRingDelay, sizeof(intRingDelay));
        }
        // deal with cellphone twinning
        if (strcmp(twin, ""))
        {
            strlcat(dialString, twin, sizeof(dialString));
        }

        strlcat(dialString, ASTDLIM, sizeof(dialString));
        strlcat(dialString, intRingDelay, sizeof(dialString));
        strlcat(dialString, ASTDLIM, sizeof(dialString));
        // Don't allow external callers to drive a # transfer
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
    {
        strlcat(dialString, ASTDLIM, sizeof(dialString));
        strlcat(dialString, ASTDLIM, sizeof(dialString));
    }

    //
    // stop queue dial-legs (which do come through here) from firing off "ghost" recordings
    //
    if (strcmp(type, "queue"))
    {
        strlcpy(recRet, SetRecord(number, "Inbound"), sizeof(recRet));
        strlcat(dialString, recRet, sizeof(dialString));
    }
    AGITool_get_variable(&agi, &res, "MOH");
    if (!strcmp(res.data, "YES"))
    {
        strlcat(dialString, "m", sizeof(dialString));
    }

    AGITool_exec(&agi, &res, "Dial", dialString);
}

char *SetRecord(char *key, char *compass)
{

    DebugFunctionTrace(__FUNCTION__);

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

    strlcpy(dialChar, "W", sizeof(dialChar));
    if (!strcmp(compass, "Inbound"))
    {
        strlcpy(dialChar, "w", sizeof(dialChar));
    }

    AGITool_get_variable(&agi, &res, "CALLRECORD1");
    strlcpy(callRecord, res.data, sizeof(callRecord));

    if (!strcmp(key, "Qexec"))
    {
        strlcpy(filename, key, sizeof(filename));
        record = 1;
    }
    else
    {
/**
 * 
 * This SELECT will fail in the new regime.   Needs fixing
 * 
*/
        snprintf(myQuery, sizeof(myQuery), "SELECT devicerec from ipphone WHERE pkey='%s' AND cluster='%s'", key, myCluster);

        sqlQuery(myQuery);
        strlcpy(devicerec, rescols[0], sizeof(devicerec));
        if (!strcmp(devicerec, ""))
        {
            strlcpy(devicerec, DBQuery("speed", "pkey", key, "devicerec"), sizeof(devicerec));
        }
        if (!strcmp(devicerec, ""))
        {
            strlcpy(devicerec, "default", sizeof(devicerec));
        }
        if (!strcmp(devicerec, "default"))
        {
            if (!strcmp(callRecord, "OTR"))
            {
                strlcpy(dialString, dialChar, sizeof(dialString));
            }
            if (!strcmp(callRecord, compass) || !strcmp(callRecord, "Both"))
            {
                record = 1;
            }
            if (!strcmp(callRecord, "OTRR"))
            {
                record = 2;
            }
        }
        else
        {
            if (!strcmp(devicerec, "OTR"))
            {
                strlcpy(dialString, dialChar, sizeof(dialString));
            }
            if (!strcmp(devicerec, compass) || !strcmp(devicerec, "Both"))
            {
                record = 1;
            }
            if (!strcmp(devicerec, "OTRR"))
            {
                record = 2;
            }
        }
    }

    if (record)
    {
        snprintf(filework, sizeof(filework), "%d-%s-%s-%s", (int)time(&now), myCluster, extension, callerid);
        strlcat(filename, filework, sizeof(filename));

        // modifiers
        /*
                    // #1 Outbound calls always use Mixmonitor
                    // commented out 14/12/2013
                    // Some customers want to pause/unpause outbound call recording
                    // so we use monitor for outbound when recording is turned on
                    //
                    if (!strcmp(compass, "Outbound")) {
                       record = 2;
                    }
         */
        // #2 Internal calls always use Mixmonitor
        if (callee_is_local && caller_is_local)
        {
            record = 2;
        }
        // #3 if MIXMONITOR=YES then use mixmonitor
        if (!strcmp(DBQuery("globals", "pkey", "global", "MIXMONITOR"), "YES"))
        {
            record = 2;
        }

        if (record == 1)
        {
            /*
             * this appears not to do anything useful
                        AGITool_get_variable(&agi,&res,"CHANNEL");
                        strlcpy(recChannel, res.data, sizeof(recChannel));
                        snprintf(recChanname, sizeof(recChanname), "__channame=%s", recChannel);
                        AGITool_exec(&agi,&res,"Set",recChanname);
            */
            snprintf(soundFile, sizeof(soundFile), "wav,%s,mb", filename);
            AGITool_exec(&agi, &res, "Monitor", soundFile);
        }
        else
        {
            /**
             * FILE refs MUST BE CHANGED TO REFLECT rename
             */
            snprintf(soundFile, sizeof(soundFile), " /var/spool/asterisk/monitor/%s.wav%s%s\"/bin/sh /opt/pbx3/scripts/selmix mixmon /var/spool/asterisk/monitor/%s.wav\"", filename, ASTDLIM, ASTDLIM, filename);
            AGITool_exec(&agi, &res, "MixMonitor", soundFile);
            snprintf(execFile, sizeof(execFile), "/bin/touch /opt/pbx3/mixmon/var/spool/asterisk/monitor/%s.wav", filename);
            system(execFile);
        }
    }
    return pdial;
}

void Page()
{

    DebugFunctionTrace(__FUNCTION__);

    char dialStr[2048] = {'\0'};
    char localip[20] = {'\0'};
    char sipHeader[64] = {'\0'};
    char speedKey[32] = {'\0'};
    char ext[MAX_EXT_LEN] = {'\0'};

    strlcpy(ext, GetExt(PARM_CMD), sizeof(ext));
    strlcat(speedKey, ext, sizeof(speedKey));

    AGITool_get_variable(&agi, &res, "LOCALIP");
    strlcpy(localip, res.data, sizeof(localip));

    snprintf(sipHeader, sizeof(sipHeader), "Call-Info:<sip:%s>;answer-after=0", localip);
    // Page all extensions
    if (!strcmp(ext, ""))
    {
        strlcpy(dialStr, DBQuery("page", "pkey", "pageall", "pagegroup"), sizeof(dialStr));
        AGITool_exec(&agi, &res, "SIPAddHeader", sipHeader);
        AGITool_exec(&agi, &res, "Page", dialStr);
    }
    // Page single extension
    else if (!strcmp(ext, DBQuery("IPphone", "pkey", ext, "pkey")))
    {
        strlcpy(dialStr, SIPDRIVER, sizeof(dialStr));
        strlcat(dialStr, "/", sizeof(dialStr));
        strlcat(dialStr, ext, sizeof(dialStr));
        AGITool_exec(&agi, &res, "SIPAddHeader", sipHeader);
        AGITool_exec(&agi, &res, "Page", dialStr);
    }
    // Page a group of extensions
    else if (!strcmp(ext, DBQuery("speed", "pkey", speedKey, "pkey")))
    {
        strlcpy(dialStr, DBQuery("speed", "pkey", speedKey, "pagegroup"), sizeof(dialStr));
        AGITool_exec(&agi, &res, "SIPAddHeader", sipHeader);
        AGITool_exec(&agi, &res, "Page", dialStr);
    }
}


char *CFCheck(char *type, char *number)
{

    DebugFunctionTrace(__FUNCTION__);

    //    char cflist[MAX_CALL_FWD_CHAIN_LEN][MAX_FWD_NUM_LEN] = {{'\0'}};
    char cfnum[MAX_FWD_NUM_LEN] = {'\0'};
    //    int i, cnt=0;
    char rdnis_string[32] = "CALLERID(rdnis)=";
    char vmflags[4] = {'\0'};
    char vmbox[MAX_EXT_LEN] = {'\0'};
    char voiceinstr[8] = {'\0'};
    char speedKey[32] = {'\0'};
    char transfer[4] = {'\0'};

    AGITool_get_variable(&agi, &res, "VOICEINSTR");
    strlcpy(voiceinstr, res.data, sizeof(voiceinstr));
    strlcpy(vmflags, ASTDLIM, sizeof(vmflags));

    if (!strcmp(voiceinstr, "NO"))
    {
        strlcat(vmflags, "su", sizeof(vmflags));
    }
    else
    {
        strlcat(vmflags, "u", sizeof(vmflags));
    }

    if (strcmp(DBGet(type, number), ""))
    {
        strlcpy(cfnum, DBGet(type, number), sizeof(cfnum));
/* Is this DND? */
        if (!strcmp(cfnum,extension)) {
            AGITool_exec(&agi,&res,"Voicemail",strcat(vmbox,vmflags));
			return NULL;
        }

        strlcpy(cfnum, DBGet(type, number), sizeof(cfnum));
        if (!callee_is_local)
        {
            AGITool_exec(&agi, &res, "Playback", "silence/1");
            AGITool_get_variable(&agi, &res, "PLAYTRANSFER");
            strlcpy(transfer, res.data, sizeof(transfer));
            if (!strcmp(transfer, "YES"))
            {
                AGITool_exec(&agi, &res, "Playback", "pls-hold-while-try");
            }
        }
        strcat(rdnis_string, extension);
        AGITool_exec(&agi, &res, "Set", rdnis_string);
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

    char operator[16] = { '\0' };
    char sysop[16] = {'\0'};
    char type[32] = {'\0'};
    char fromNum[32] = {'\0'};
    char toNum[32] = {'\0'};
    char *technology;
    char *sipId;
/**
 *  Get the type (CFBS/CFIM) from the cfTab
*/
    strlcpy(type, *(cfTab + switchdig), sizeof(type));
/**
 *  Get the SIP endpoint ID from the channel variable
*/
    technology = strtok(channel,"/");
    sipId = strtok(NULL,"-");

    strlcpy(toNum, GetExt(PARM_CMD), sizeof(toNum));

    DBPut(type, sipId, toNum);

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
        DBPut(*(cfTab + switchdig), sipId, fromNum);
        AGITool_exec(&agi, &res, "Playback", "activated");
    }
    else // toggle OFF
    {
        DBPut(*(cfTab + switchdig), sipId, "");
        AGITool_exec(&agi, &res, "Playback", "de-activated");
    }
}

void CFVMailToggle()
{

    DebugFunctionTrace(__FUNCTION__);

    char fromNum[32] = {'\0'};
    char toNum[32] = {'\0'};
    char *technology;
    char *sipId;

    strlcat(fromNum, callerid, sizeof(fromNum));
/**
 *  Get the SIP endpoint ID from the channel variable
*/
    technology = strtok(channel,"/");
    sipId = strtok(NULL,"-");

    // if the property is empty in the DB then activate otherwise de-activate
    AGITool_exec(&agi, &res, "Playback", "call-forwarding");
    if (!strcmp(DBGet(*(cfTab + switchdig), sipId), ""))
    {
        DBPut(*(cfTab + switchdig), sipId, fromNum);
        AGITool_exec(&agi, &res, "Playback", "activated");
    }
    else
    {
        DBPut(*(cfTab + switchdig), sipId, "");
        AGITool_exec(&agi, &res, "Playback", "de-activated");
    }
}

void FollowMe()
{
/**
 * NEEDS TO BE REWRITTEN
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

    char *technology;
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

    char *technology;
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
        for (i = 0; i < strlen(number); i++)
        {
            number[i] = number[i + strlen(preSel)];
        }
    }
    return number;
}

void SetTimer()
{

    DebugFunctionTrace(__FUNCTION__);

    if (Authenticate("SYSPASS") != 0)
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

void Inbound()
{

    DebugFunctionTrace(__FUNCTION__);

    char technology[32] = {'\0'};
    //    char faxDetect[8] = {'\0'};
    char ringDelay[8] = {'\0'};
    //    char faxDetectVal[8] = {'\0'};
    char tag[64] = {'\0'};
    char swoclip[8] = {'\0'};
    char carrier[32] = {'\0'};
    char prefix[32] = {'\0'};
    char moh[16] = {'\0'};
    char maxin[4] = {'\0'};
    char lterm[4] = {'\0'};
    char alertinfo[128] = {'\0'};
    char transformclip[MAX_TRANSFORM_LEN] = {'\0'};
    char clicluster[MAX_CLUSTER_LEN] = {'\0'};
    char setcdrcmduser[64] = "CDR(userfield)=";
    char set_dynamic_features[4] = "NO";

    /*
     *  check if we are at max inbound channels
     */
    AGITool_get_variable(&agi, &res, "MAXIN");
    strcpy(maxin, res.data);
    if (strcmp(maxin, ""))
    {
        AGITool_set_variable(&agi, &res, "GROUP(inbound)", "inbound");
        AGITool_get_variable(&agi, &res, "GROUP_COUNT(inbound)");

        if (atoi(res.data) > atoi(maxin))
        {
            AGITool_exec(&agi, &res, "Playtones", "busy");
            AGITool_exec(&agi, &res, "Busy", "");
            return;
        }
    }
    /*
     * we are about to bridge so turn off HUP
     */
    //	AGITool_exec(&agi,&res,"Set","AGISIGHUP=no");

    // set the dialled number (DDI) in the CDR userfield
    strlcat(setcdrcmduser, PARM_KEY, sizeof(setcdrcmduser));
    AGITool_exec(&agi, &res, "Set", setcdrcmduser);

    //  take FAX delay if necessary - removed in V7
    // AGITool_get_variable(&agi,&res,"FAXDETECT");
    // strcpy(faxDetectVal, res.data);

    // Set dynamic features if necessary
    AGITool_get_variable(&agi, &res, "SET_DYNAMIC_FEATURES");
    strlcpy(set_dynamic_features, res.data, sizeof(set_dynamic_features));

    if (!strcmp(set_dynamic_features, "YES"))
    {
        AGITool_exec(&agi, &res, "Set", "__DYNAMIC_FEATURES=automon#clear#pause#resume");
    }

    DBQuery("lineIO", "pkey", PARM_KEY, "technology,tag,inprefix,alertinfo,transformclip,moh,swoclip");
    strlcpy(technology, rescols[0], sizeof(technology));
    strlcpy(tag, rescols[1], sizeof(tag));
    strlcpy(prefix, rescols[2], sizeof(prefix));
    strlcpy(alertinfo, rescols[3], sizeof(alertinfo));
    strlcpy(transformclip, rescols[4], sizeof(transformclip));
    strlcpy(moh, rescols[5], sizeof(moh));
    strlcpy(swoclip, rescols[6], sizeof(swoclip));

    /*  FAX detet removed in V7
        strcpy(faxDetect, DBQuery("lineIO", "pkey", PARM_KEY, "faxdetect"));
        if (!strcmp(faxDetect, "YES")) {
           AGITool_answer(&agi,&res);
    //	   strcat(faxSilence, faxDetectVal);
           AGITool_exec(&agi,&res,"Wait",faxDetectVal);
        }
    */
    // Should we use remotenum  or carrier because analogue can be DAHDI, Zap or number

    AGITool_get_variable(&agi, &res, "RINGDELAY");
    strlcpy(ringDelay, res.data, sizeof(ringDelay));

    AGITool_get_variable(&agi, &res, "LTERM");
    strlcpy(lterm, res.data, sizeof(lterm));

    AGITool_set_variable(&agi, &res, "__MOH", moh);

    // CLIP Processing

    //  CLI Prefix

    strcpy(prefix, DBQuery("lineIO", "pkey", PARM_KEY, "inprefix"));
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

    if (strcmp(swoclip, "NO") && strcmp(callerid, ""))
    {
        strlcpy(carrier, DBQuery("lineIO", "pkey", callerid, "carrier"), sizeof(carrier));
        if (!strcmp(carrier, "PTT_CLID") || !strcmp(carrier, "CLID"))
        {
            if (strcmp(callerid, PARM_KEY))
            {
                strlcpy(clicluster, DBQuery("lineIO", "pkey", callerid, "cluster"), sizeof(clicluster));
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

    if (strcmp(lterm, "YES"))
    {
        AGITool_answer(&agi, &res);
        // AGITool_exec(&agi,&res,"Ringing","");
    }
    // cause a ring for voip lines if requested
    if (!strcmp(technology, "SIP"))
    {
        if (strcmp(ringDelay, "0"))
        {
            AGITool_exec(&agi, &res, "Ringing", "");
            AGITool_exec(&agi, &res, "Wait", ringDelay);
        }
    }
    if (!strcmp(technology, "IAX2"))
    {
        if (strcmp(ringDelay, "0"))
        {
            AGITool_exec(&agi, &res, "Ringing", "");
            AGITool_exec(&agi, &res, "Wait", ringDelay);
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
    char rc_dest[32] = {'\0'};

    DBQuery("lineIO", "pkey", remotenum, "cluster,routeclassopen,routeclassclosed,openroute,closeroute");
    strlcpy(cluster, rescols[0], sizeof(cluster));
    strlcpy(routeclassopen, rescols[1], sizeof(routeclassopen));
    strlcpy(routeclassclosed, rescols[2], sizeof(routeclassclosed));
    strlcpy(openroute, rescols[3], sizeof(openroute));
    strlcpy(closeroute, rescols[4], sizeof(closeroute));
    /*
        if (!strcmp(cluster, "")) {
            strcpy(cluster, "default");
        }
    */
    strlcpy(state, DBGet("STAT", "OCSTAT"), sizeof(state));
    //    AGITool_set_variable(&agi,&res,"REMOTENUM",remotenum);
    if (!strcmp(state, "OPEN"))
    {
        //   Open(remotenum);
        strlcpy(rc_char, routeclassopen, sizeof(rc_char));
        strlcpy(rc_dest, openroute, sizeof(rc_dest));
        routeClass(remotenum, rc_char, rc_dest, myCluster);
    }
    else if (!strcmp(state, "CLOSED"))
    {
        //     Closed(remotenum);
        strlcpy(rc_char, routeclassclosed, sizeof(rc_char));
        strlcpy(rc_dest, closeroute, sizeof(rc_dest));
        routeClass(remotenum, rc_char, rc_dest, myCluster);
    }
    else
    {
        strlcpy(state, CheckTime(cluster), sizeof(state));
        if (!strcmp(state, "CLOSED"))
        {
            //   Closed(remotenum);
            strlcpy(rc_char, routeclassclosed, sizeof(rc_char));
            strlcpy(rc_dest, closeroute, sizeof(rc_dest));
            routeClass(remotenum, rc_char, rc_dest, myCluster);
        }
        else
        {
            //    Open(remotenum);
            strlcpy(rc_char, routeclassopen, sizeof(rc_char));
            strlcpy(rc_dest, openroute, sizeof(rc_dest));
            routeClass(remotenum, rc_char, rc_dest, myCluster);
        }
    }
}

char *GetState(char *cluster)
{

    DebugFunctionTrace(__FUNCTION__);

    char state[8] = {'\0'};

    strcpy(state, DBGet("STAT", "OCSTAT"));
    if (!strcmp(state, "OPEN"))
    {
        return "OPEN";
    }
    else if (!strcmp(state, "CLOSED"))
    {
        return "CLOSED";
    }
    else
    {
        return CheckTime(cluster);
    }
}

char *CheckTime(char *cluster)
{

    DebugFunctionTrace(__FUNCTION__);

    char clustermaster[8] = {'\0'};
    char clusterstate[8] = {'\0'};
    char clusterdboclo[8] = {'\0'};

    DBQuery("Cluster", "pkey", myCluster, "masteroclo,oclo,routeclassoverride,routeoverride");
    strlcpy(clustermaster, rescols[0], sizeof(clustermaster));
    strlcpy(clusterstate, rescols[1], sizeof(clusterstate));
    strlcpy(routeclassoverride, rescols[2], sizeof(routeclassoverride));
    strlcpy(routeoverride, rescols[3], sizeof(routeoverride));

    if (strcmp(routeoverride, ""))
    {
        strlcpy(closeroute, routeoverride, sizeof(closeroute));
        strlcpy(routeclassclosed, routeclassoverride, sizeof(routeclassclosed));
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
    strlcpy(clusterstate, DBQuery("Cluster", "pkey", myCluster, "oclo"), sizeof(clusterstate));
    if (!strcmp(clusterstate, "OPEN"))
    {
        return "OPEN";
    }

    if (!strcmp(clusterstate, "CLOSED"))
    {
        return "CLOSED";
    }
    AGITool_exec(&agi, &res, "NoOp", "NO MTIME - returning OPEN");
    return "OPEN";
}

void routeClass(char *gdest, char *rc_char, char *rc_dest, char *cluster)
{
    //
    // Routes the call according to the value in rc_char (routeclass)
    // rc-dest holds the qualifier (exten, ivrname, queuename, callgroup etc)
    //

    DebugFunctionTrace(__FUNCTION__);

    int routeclass;
    char disaPass[16] = {'\0'};
    char oper[MAX_EXT_LEN] = {'\0'};
    //    char options[16] = {'\0'};
    //    char timeout[64] = {'\0'};
    //    char devicerec[16] = {'\0'};
    //    char queue[32] = {'\0'};
    //    char voiceinstr[8] = {'\0'};
    //    char greetnum[8] = {'\0'};
    //    char greetFile[MAX_FILENAME_LEN] = "usergreeting";
    //    char vmflags[4] = {'\0'};

    routeclass = atoi(rc_char);
    switch (routeclass)
    {
    case 0: // Take no action
            // not really implemented - acknowledges "None" choice
            // in an ivr menu but "None" entries should never come here
        break;
    case 1: // dialable number (extension or callgroup)
        AGITool_set_priority(&agi, &res, 1);
        AGITool_set_extension(&agi, &res, rc_dest);
        AGITool_set_context(&agi, &res, myClusterContext);
        break;
    case 2: // IVR name
        IVR(rc_dest);
        break;
        // no longer supported
    case 3:
        //                IVR("");    // The Default IVR
        break;
    case 4: // Queue Name
        AGITool_set_priority(&agi, &res, 1);
        AGITool_set_extension(&agi, &res, rc_dest);
        AGITool_set_context(&agi, &res, myClusterContext);
        break;
    case 5: // DISA
        // Using the priv_sibling context is a bit of a hack -
        // It should have its own context really, if for no other
        // reason than clarity
        strcpy(disaPass, DBQuery("lineIO", "pkey", gdest, "disapass"));
        if (!strcmp(disaPass, ""))
        {
            break;
        }
        AGITool_exec(&agi, &res, "Playback", "silence/1");
        AGITool_exec(&agi, &res, "Authenticate", disaPass);
        if (atoi(res.result) == 0)
        {
            snprintf(eparm, sizeof(eparm), "no-password%spriv_sibling", ASTDLIM);
            AGITool_exec(&agi, &res, "DISA", eparm);
        }
        break;
/*   Removed in 7
    case 6:          // CALLBACK
        DialBack();
        break;
*/
    case 8: // Sibling Trunk
        OutTrunk(rc_dest);
        break;
    case 9: // VoIP Trunk
        OutTrunk(rc_dest);
        break;
    case 10: // Custom App
        AGITool_set_priority(&agi, &res, 1);
        AGITool_set_extension(&agi, &res, "s");
        AGITool_set_context(&agi, &res, rc_dest);
        break;
/**
    case 11: // Group (TDM Trunk)
        OutGroup(rc_dest);
        break;
*/
    case 20: // Retrieve Voicemail
        AGITool_set_priority(&agi, &res, 1);
        AGITool_set_extension(&agi, &res, "*51*");
        AGITool_set_context(&agi, &res, "internal-presets");
        break;
        /*
          This case is not used

                   case 21:          // Leave Voicemail
                        strlcpy(oper, DBQuery("Cluster", "pkey", cluster, "operator"), sizeof(oper));
                        if (!strcmp(oper, "System Operator")) {
                            AGITool_get_variable(&agi,&res,"SYSOP");
                            strlcpy(oper, res.data, sizeof(oper));
                            if (!strcmp(oper, "")) {
                              snprintf (vmsg,sizeof(vmsg),"OPERATOR IS NOT SET IN GLOBALS!! - CAN'T LEAVE VOICEMAILL!");
                              AGITool_verbose(&agi,&res,vmsg,1);
                              return;
                            }
                        }
                        AGITool_get_variable(&agi,&res,"VOICEINSTR");
                        strlcpy(voiceinstr, res.data, sizeof(voiceinstr));
                        strlcpy(vmflags, ASTDLIM, sizeof(vmflags));
                        if (!strcmp(voiceinstr, "NO")) {
                          strlcat(vmflags, "su", sizeof(vmflags));
                                }
                                else {
                          strlcat(vmflags, "u",sizeof(vmflags));
                                }
                        AGITool_exec(&agi,&res,"Voicemail",strcat(oper,vmflags));
                        break;
        */
    case 100: // Operator
        strcpy(oper, DBQuery("Cluster", "pkey", cluster, "operator"));
        if (!strcmp(oper, "System Operator"))
        {
            AGITool_get_variable(&agi, &res, "SYSOP");
            strcpy(oper, res.data);
            if (!strcmp(oper, ""))
            {
                snprintf(vmsg, sizeof(vmsg), "OPERATOR IS NOT SET IN GLOBALS!! - CAN'T ROUTE CALL!");
                AGITool_verbose(&agi, &res, vmsg, 1);
                return;
            }
        }
        AGITool_set_priority(&agi, &res, 1);
        AGITool_set_extension(&agi, &res, oper);
        AGITool_set_context(&agi, &res, "myClusterContext");
        break;
    case 101: // Hangup
        AGITool_exec(&agi, &res, "Hangup", "");
        break;
    }
    return;
}

void IVR()
{

    DebugFunctionTrace(__FUNCTION__);

    char msg[MAX_FILENAME_LEN] = {'\0'};
    char greetnum[8] = {'\0'};
    char option[4] = {'\0'};
    char optionStr[15] = {'\0'};
    char dtmf[8] = {'\0'};
    char ivrwork[8] = {'\0'};
    char ivrsilence[16] = "silence/";
    char ivrkeywait[4] = "6";
    int ivrdigitwait = 6000;
    int i;

    strlcat(ivrsilence, "6", sizeof(ivrsilence));

    DBQuery("globals", "pkey", "global", "IVRKEYWAIT,IVRDIGITWAIT");
    strlcpy(ivrkeywait, rescols[0], sizeof(ivrkeywait));
    strlcpy(ivrwork, rescols[1], sizeof(ivrwork));

    if (strcmp(ivrkeywait, ""))
    {
        strlcat(ivrsilence, ivrkeywait, sizeof(ivrsilence));
    }

    /* suggested fix for above
        strcpy (ivrwork,DBQuery("globals", "pkey", "global", "IVRKEYWAIT"));
        if (strcmp(ivrwork, "")) {
            strcpy(ivrkeywait,ivrwork);
        }
        strcat(ivrsilence,ivrkeywait);
    */

    if (strcmp(ivrwork, ""))
    {
        ivrdigitwait = atoi(ivrwork);
    }

    if (strcmp(PARM_KEY, ""))
    {
        // get the number of options for the menu and construct the option string
        DBQuery("ivrmenu", "pkey", PARM_KEY, "option0,option1,option2,option3,option4,option5,option6,option7,option8,option9,option10,option11");
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

        strlcpy(greetnum, DBQuery("ivrmenu", "pkey", PARM_KEY, "greetnum"), sizeof(greetnum));
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

        if (strcmp(DBQuery("ivrmenu", "pkey", PARM_KEY, "listenforext"), "YES"))
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
                    AGITool_set_context(&agi, &res, "myClusterContext");
                    return;
                }
                IVRAction(PARM_KEY, dtmf);
                return;
            }
        }

        // If no key is pressed then perform the timeout action
        if (!strcmp(DBQuery("ivrmenu", "pkey", PARM_KEY, "timeout"), "Repeat Message"))
        {
            IVR();
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
    char dbRouteclass[32] = "routeclass";
    char action[128] = {'\0'};
    char alert[128] = {'\0'};
    char routeclass[8] = {'\0'};
    //    char cluster[MAX_CLUSTER_LEN] = {'\0'};
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
        strcat(dbRouteclass, "10");
        strcat(tag, "10");
    }
    else if (!strcmp(press, "#"))
    {
        strcat(dbOption, "11");
        strcat(dbAlert, "11");
        strcat(dbRouteclass, "11");
        strcat(tag, "11");
    }
    else
    {
        strlcat(dbOption, press, sizeof(dbOption));
        strlcat(tag, press, sizeof(tag));
        strlcat(dbAlert, press, sizeof(dbAlert));
        strlcat(dbRouteclass, press, sizeof(dbRouteclass));
    }

    strlcpy(action, DBQuery("ivrmenu", "pkey", menu, dbOption), sizeof(action));

    //  handle case of timeout being requested
    if (!strcmp(dbOption, "timeout"))
    {
        strlcpy(routeclass, DBQuery("ivrmenu", "pkey", menu, "timeoutrouteclass"), sizeof(routeclass));
    }
    else
    { // handle ordinary case
        strlcpy(routeclass, DBQuery("ivrmenu", "pkey", menu, dbRouteclass), sizeof(routeclass));
        // set Alert-info if present
        strlcpy(alert, DBQuery("ivrmenu", "pkey", menu, dbAlert), sizeof(alert));
        if (strcmp(alert, ""))
        {
            AGITool_exec(&agi, &res, "SIPAddHeader", alert);
        }
        strcpy(tagID, DBQuery("ivrmenu", "pkey", menu, tag));
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
        routeClass("", routeclass, action, myCluster);
    }
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

char *DBQuery(char *table, char *wherecol, char *whereval, char *column)
{

    if (debug)
    {
        snprintf(vmsg, sizeof(vmsg), "Trace Entered DBQuery with table=%s, wherecol=%s, whereval=%s, col=%s",
                 table, wherecol, whereval, column);
        DebugFunctionMsg(__FUNCTION__, vmsg);
    }

    char query[256] = {'\0'};
    char *pQ = &query[0];
    char *pVal = &rescols[0][0];
    char whereStmnt[64] = {'\0'};
    int retval, i;

    sqlite3 *handle;
    sqlite3_stmt *stmt;

    retval = sqlite3_open(SQLITEDB, &handle);
    if (retval)
    {
        snprintf(vmsg, sizeof(vmsg), "Database connection failed, retval is %i", retval);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return "-1";
    }

    snprintf(whereStmnt, sizeof(whereStmnt), "%s=\'%s\'", wherecol, whereval);
    snprintf(query, sizeof(query), "SELECT %s FROM %s WHERE %s", column, table, whereStmnt);

    if (debug)
    {
        snprintf(vmsg, sizeof(vmsg), "Executing query %s", query);
        DebugFunctionMsg(__FUNCTION__, vmsg);
    }

    for (i = 0; i < 3; i++)
    {
        retval = sqlite3_prepare_v2(handle, pQ, -1, &stmt, 0);
        if (retval == SQLITE_OK)
        {
            break;
        }
        if (retval == SQLITE_LOCKED || retval == SQLITE_BUSY)
        {
            snprintf(vmsg, sizeof(vmsg), "Database LOCK! retry in .5s");
            DebugFunctionMsg(__FUNCTION__, vmsg);
            AGITool_exec(&agi, &res, "Wait", "0.5");
        }
        else
        {
            break;
        }
    }

    if (retval)
    {
        snprintf(vmsg, sizeof(vmsg), "Database Prepare failed, retval is %i, query is %s", retval, query);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        sqlite3_close(handle);
        return pVal;
    }

    retval = sqlite3_step(stmt);

    if (debug)
    {
        snprintf(vmsg, sizeof(vmsg), "Query returned %i columns", sqlite3_column_count(stmt));
        DebugFunctionMsg(__FUNCTION__, vmsg);
    }

    if (sqlite3_column_count(stmt) > 0)
    {
        for (i = 0; i < sqlite3_column_count(stmt); i++)
        {
            if (sqlite3_column_text(stmt, i))
            {
                strlcpy(rescols[i], (const char *)sqlite3_column_text(stmt, i), sizeof(rescols[i]));
            }
            else
            {
                strcpy(rescols[i], "");
            }
            if (debug)
            {
                snprintf(vmsg, sizeof(vmsg), "DBQcol %i returned %s", i, rescols[i]);
                DebugFunctionMsg(__FUNCTION__, vmsg);
            }
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(handle);
    return pVal;
}

char *sqlQuery(char *query)
{
    //
    //
    //	How to call this function
    //
    //	snprintf (myQuery,sizeof(myQuery),"SELECT id from cluster WHERE pkey='%s'",myCluster);
    //  sqlQuery(myQuery);
    //
    //  Useful for more complex SQL
    //

    if (debug)
    {
        snprintf(vmsg, sizeof(vmsg), "Trace Entered sqlQuery with query=%s", myQuery);
        DebugFunctionMsg(__FUNCTION__, vmsg);
    }

    char *pQ = &query[0];
    char *pVal = &rescols[0][0];
    int retval, i;

    sqlite3 *handle;
    sqlite3_stmt *stmt;

    retval = sqlite3_open(SQLITEDB, &handle);
    if (retval)
    {
        snprintf(vmsg, sizeof(vmsg), "Database connection failed, retval is %i", retval);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        return "-1";
    }

    if (debug)
    {
        snprintf(vmsg, sizeof(vmsg), "Executing query %s", query);
        DebugFunctionMsg(__FUNCTION__, vmsg);
    }

    for (i = 0; i < 3; i++)
    {
        retval = sqlite3_prepare_v2(handle, pQ, -1, &stmt, 0);
        if (retval == SQLITE_OK)
        {
            break;
        }
        if (retval == SQLITE_LOCKED || retval == SQLITE_BUSY)
        {
            snprintf(vmsg, sizeof(vmsg), "Database LOCK! retry in .5s");
            DebugFunctionMsg(__FUNCTION__, vmsg);
            AGITool_exec(&agi, &res, "Wait", "0.5");
        }
        else
        {
            break;
        }
    }

    if (retval)
    {
        snprintf(vmsg, sizeof(vmsg), "Database Prepare failed, retval is %i, query is %s", retval, query);
        DebugFunctionMsg(__FUNCTION__, vmsg);
        sqlite3_close(handle);
        return pVal;
    }

    retval = sqlite3_step(stmt);

    if (debug)
    {
        snprintf(vmsg, sizeof(vmsg), "Query returned %i columns", sqlite3_column_count(stmt));
        DebugFunctionMsg(__FUNCTION__, vmsg);
    }

    if (sqlite3_column_count(stmt) > 0)
    {
        for (i = 0; i < sqlite3_column_count(stmt); i++)
        {
            if (sqlite3_column_text(stmt, i))
            {
                strlcpy(rescols[i], (const char *)sqlite3_column_text(stmt, i), sizeof(rescols[i]));
            }
            else
            {
                strcpy(rescols[i], "");
            }
            if (debug)
            {
                snprintf(vmsg, sizeof(vmsg), "DBQcol %i returned %s", i, rescols[i]);
                consoleMsg(vmsg, 9);
            }
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(handle);
    return pVal;
}

/***********************************************************************
   Code for Queuemetrics outbound call statistics. This ia a straight
   replacement for the queuemetrics perl AGI. It opens and writes directly to the
   Asterisk queue log.

   The code accepts 4 parameters and it is called from a custom app like this

   Ast 1.6/1.8+ - probabaly won't work with Ast 1.4

EXAMPLE CALL
    exten => _X.,1,AGI(sxhpe,OutQmt,number,,Qname,agent)

PARAMS
          PARM_KEY 	number -> number to Dial
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
    //    AGITool_exec(&agi,&res,"Dial",PARM_PM1);
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
    AGITool_get_variable(&agi, &res, "ANSWEREDTIME");
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

    DebugFunctionTrace(__FUNCTION__);

    char clidwork[MAX_EXT_LEN] = {'\0'};
    char shortrdnis[5] = {'\0'};

    strlcpy(clidline, DBQuery("lineIO", "pkey", key, "callerid"), sizeof(clidline));

    /*
     * Changed to retrieve on chanID instead of CLID 16/10/19.
     * callerid is misleading in V7 because it can be duplicated across tenants
     * chanId is the true SIP endpoint ID
     */
    if (caller_is_local)
    {
        snprintf(myQuery, sizeof(myQuery), "SELECT callerid from ipphone WHERE pkey='%s' AND cluster='%s'", callerid, myCluster);
        sqlQuery(myQuery);
        strlcpy(clidphone, rescols[0], sizeof(clidphone));
    }
    /*
       ToDo - this RDNIS code is wrong in the new regime.   The RDNIS is a full SIP number which doesn't
       exist in the DB.   This is anyway a corner case, it will insert the cporrect CLID when someone calls a phone that
       is forwarded so that the correct CLID is sent.
     */
    else if (rdnis_is_local)
    {

        if (!strcmp(myCluster, "default"))
        {
            snprintf(myQuery, sizeof(myQuery), "SELECT callerid from ipphone WHERE pkey='%s' AND cluster='%s'", rdnis, myCluster);
        }
        else
        {
            char *skip = rdnis;
            skip += 2;
            strlcpy(shortrdnis, skip, sizeof(shortrdnis));
            snprintf(myQuery, sizeof(myQuery), "SELECT callerid from ipphone WHERE pkey='%s' AND cluster='%s'", shortrdnis, myCluster);
        }
        sqlQuery(myQuery);
        strlcpy(clidphone, rescols[0], sizeof(clidphone));
    }

    if (strcmp(clidline, ""))
    {
        if (caller_is_local)
        {
            strlcpy(clidwork, clidline, sizeof(clidwork));
        }
        else if (!strcmp(DBQuery("globals", "pkey", "global", "CFWDEXTRNRULE"), "enabled"))
        {
            strlcpy(clidwork, clidline, sizeof(clidwork));
        }
    }

    if (strcmp(myClusterclid, ""))
    {
        if (caller_is_local)
        {
            strlcpy(clidwork, myClusterclid, sizeof(clidwork));
        }
    }

    if (strcmp(clidphone, ""))
    {
        strlcpy(clidwork, clidphone, sizeof(clidwork));
    }

    if (strcmp(clidwork, ""))
    {
        snprintf(clidstrng, sizeof(clidstrng), "CALLERID(number)=%s", clidwork);
        AGITool_exec(&agi, &res, "Set", clidstrng);
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
