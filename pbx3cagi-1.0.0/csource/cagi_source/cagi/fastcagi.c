// 
//   Written by: David Cornewell (david.cornewell@gmail.com)
//         Date: 09-28-2006
// 
//   Purpose: 	This is a simple program that is meant to get the (fast)AGI application being requested and run it.
//					It is made to be run using xinetd since it makes things so easy.
//
//   License:	LGPL. 
//
//   MODS: 
//        $Id: fastcagi.c,v 1.2 2006/10/06 20:44:10 dcornewell Exp $
//        $Log: fastcagi.c,v $
//        Revision 1.2  2006/10/06 20:44:10  dcornewell
//        Added support for argv to be passed to the AGI app being called. it parses
//        ?this=that&this2=that2 and makes it this that this2 that2.  also updated
//        the reading of first 2 lines to get network_script.  using read rather than
//        fgets so we don't loose the buffer when calling exec().
//
//        Revision 1.1  2006/10/04 18:50:46  dcornewell
//        Adding fastcagi to project for serving AGI apps using fast agi on xinetd.
//        Also put in doc for v.5 release.
//

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

extern int errno;

static int strim(char *src)
{
	int i=0;
	if (src) {
		i=strlen(src)-1;
		while (i>0 && isspace(src[i])) {
			src[i]=0;
			i--;
		}
	}
	return i;
}


static char **AstParseScriptName(char *cmd)
{
	char **agi_argv=NULL, *ptr=strchr(cmd, '?');
	int i, argc=1, cur_argc;

	if (ptr) {
		ptr[0]=0;
		ptr++;	

		for (i=0; ptr[i]; i++) { 
			if (ptr[i]=='&' || ptr[i]=='=') argc++;
		}
		
		agi_argv=(char**)calloc(argc+3, (sizeof(char*)));
		if (agi_argv) {
			agi_argv[0] = cmd;
			cur_argc=1;
			if (ptr[0]) {
				agi_argv[cur_argc] = &ptr[0];
				cur_argc++;
				
				for (i=0; ptr[i]; i++) { 
					if (ptr[i]=='&' || ptr[i]=='=') {
						ptr[i]=0;
						i++;
						agi_argv[cur_argc] = &ptr[i];
						cur_argc++;
					}
				}
			}
		}
	}
	
	return agi_argv;
}


/*
static void AstCleanExecCmd(char *cmd)
{
	char *args = strchr(cmd,'?');
	int i;
	
	if (args) {
		for (i=0; args[i]; i++) { 
			if (args[i]=='&' || args[i]=='=' || args[i]=='?') {
				args[i]=' ';
			}
		}
	}
	return;
}
*/

int main(int argc, char *argv[])
{
	char buffer[1024], *scriptname, path[1024], **agi_argv, *agi_argv_buf[2];
	char *logfilename="/var/log/fastcagi.log", *agidir="/var/lib/asterisk/agi-bin/";
	int i;
	FILE *logfp=NULL;
	
	for (i=1;i<argc;i++) {
		if (argv[i][0]== '-' && argv[i][1]== '-') {
			if (!strncmp(&argv[i][2], "agidir",6)) {
					if (argv[++i]) agidir = argv[i];
			} else if (!strncmp(&argv[i][2], "logfile",7)) {
					if (argv[++i]) logfilename = argv[i];
			}
		}		
	}

	//
	// We'll change the progname as needed. If parse_agi_request is true, 
	// we'll point agi_argv somewhere else entirely.
	//
	agi_argv_buf[0] = "";
	agi_argv_buf[1] = NULL;
	agi_argv = agi_argv_buf;
		
	if (strlen(logfilename)>0) {
		logfp=fopen(logfilename, "a");
	}

	if (logfp) {
		fprintf(logfp, "Starting fastcagi.\n");
	}
		
	/* Get these values and use them to spawn the AGI app. Any agi capable app should work.
    *   agi_network - value is yes if this is a fastagi
    *   agi_network_script - name of the script to execute
 	 */

//	fgets(buffer,sizeof(buffer),stdin);
	read(0,buffer,17);
	if (!strncmp(buffer, "agi_network: yes", 16)) {
		memset(buffer,0,sizeof(buffer));
//		fgets(buffer,sizeof(buffer),stdin);
		read(0,buffer,18);
		if (!strncmp(buffer, "agi_network_script", 18)) {
			while (buffer[strlen(buffer)-1]!='\n' && strlen(buffer)<sizeof(buffer)-1) {
				read(0,&buffer[strlen(buffer)],1);
				
			}
			scriptname=strchr(buffer, ':');
			if (scriptname) {
				// we are where we need to be, run the script.
				scriptname+=2;
				strim(scriptname);
				
				if (strchr(scriptname, '?')) {
					agi_argv = AstParseScriptName(scriptname);
					if (logfp) {
						fprintf(logfp, "Parsed agi_network_script: ");
						for (i=0; agi_argv[i]; i++) {
							fprintf(logfp, "%s, ", agi_argv[i]);
						}
						fprintf(logfp, "\n");
					}
				}
//				AstCleanExecCmd(path);

				snprintf(path,sizeof(path),"%s/%s", agidir, scriptname);
				if (logfp) {
					fprintf(logfp, "execv'ing %s.\n", path);
					fclose(logfp); // won't need this. exec will leave us.
				}

				chdir(agidir);
				execv(path, agi_argv);

				//
				// Something went wrong
				//
				if (strlen(logfilename)>0) {
					logfp=fopen(logfilename, "a");
					if (logfp) {
						fprintf(logfp, "execv'ing failed err(%d): %s.\n", errno, strerror(errno));
						fclose(logfp);
					}
				}
				
			}
		} else if (logfp) {
			fprintf(logfp, "Could not find agi_network_script. Buffer: %s\n", buffer);
		}
	} else if (logfp) {
		fprintf(logfp, "Could not find agi_network. Buffer: %s\n", buffer);
	}
 	
	return 0;
}
