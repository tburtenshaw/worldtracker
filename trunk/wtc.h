/*
This should contain all the command line utilties, and the main program.
*/

void PrintIntro(char *programName);	//blurb
void PrintUsage(char *programName); 	//called if run without arguments
int HandleCLIOptions(int argc,char *argv[], OPTIONS *options);
void PrintOptions(OPTIONS *options);
