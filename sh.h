
#include "get_path.h"

int pid;
int sh( int argc, char **argv, char **envp);
char *which(char *command, struct pathelement *pathlist);
char *where(char *command, struct pathelement *pathlist);
void printWorkingDir();
void list ( char *dir );
void printenv(char **envp);
void changeDir(char **args);
void nullify(char **args);
void printPid();

#define PROMPTMAX 32
#define MAXARGS 10
