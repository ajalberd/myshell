
#include "get_path.h"

typedef struct node
{ 
    pthread_t thread;
    char *data; 
    struct node *next; 
}node;
node *head; //global head of LL, probably not the best place for it.
node *headu; //for the watchuser
int pid;
int sh( int argc, char **argv, char **envp);
void *which(char *command, struct pathelement *pathlist);
char* whichRet(char *command, struct pathelement *pathlist);
char *where(char *command, struct pathelement *pathlist);
void printWorkingDir();
void list ( char **args );
void printenv(char **envp, char **args,int forcePrintAll);
void setEnviornment(char **envp, char**args, struct pathelement *pathlist);
void changeDir(char **args);
void nullify(char **args);
void printPid();
void promptCmd(char **args, char* prompt);
void killProc(char **args);
void watchmailthread(char **args);
void watchmail(char **args);
void addnode(pthread_t thread, char *data);
void watchuser(char **args);
void watchuserthread(char **args);

#define PROMPTMAX 32
#define MAXARGS 10
