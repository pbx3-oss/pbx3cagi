// Written By: David Cornewell (david.cornewell at gmail.com)
// 
// This is a sample program written to show the basics of using CAGI.  
//

#include <stdio.h>
#include <stdarg.h>

#include "cagi.h"

int main(int argc, char *argv[])
{
	AGI_TOOLS agi;
	AGI_CMD_RESULT res;
	char dest[100];

	AGITool_Init(&agi);

	AGITool_verbose(&agi, &res, AGITool_ListGetVal(agi.agi_vars, "agi_request"), 0);

	AGITool_text2wav(&agi,&res,"Begin Program","1234567890#*",0);
	AGITool_set_variable(&agi, &res, "cagivar", "Hello");
	AGITool_get_variable2(&agi, &res, "cagivar", dest, sizeof(dest));
	AGITool_text2wav(&agi,&res,"The variable equals ","1234567890#*",0);
	AGITool_text2wav(&agi,&res,dest,"1234567890#*",0);


	AGITool_text2wav(&agi,&res,"Begin recording. Hit a key to stop.","1234567890#*",0);

	AGITool_record_file(&agi,&res, "/tmp/testrec", "gsm", "1234567890#*", -1, 10, 10, 0);

	AGITool_text2wav(&agi,&res,"You said.","1234567890#*",0);
	AGITool_stream_file(&agi,&res, "/tmp/testrec", "1234567890#*", 0);

	AGITool_text2wav(&agi,&res,"Goodbye","1234567890#*",0);

	AGITool_Destroy(&agi);

	return 0;
}


