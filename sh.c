#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "sh.h"
#include <errno.h>
#include <glob.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <utmpx.h>

node *head = NULL; //for watchmail
node *headu = NULL; //for watchuser
pthread_mutex_t watchusert = PTHREAD_MUTEX_INITIALIZER;

int sh( int argc, char **argv, char **envp )
{
  char *prompt = calloc(PROMPTMAX, sizeof(char));
  char *commandline = calloc(MAX_CANON, sizeof(char));
  char *command, *arg, *commandpath, *p, *pwd, *owd;
  int uid, i, status, argsct, go = 1;
  struct passwd *password_entry;
  char *homedir;
  struct pathelement *pathlist;
  char input [64];

  uid = getuid();
  password_entry = getpwuid(uid);               /* get passwd info */
  homedir = password_entry->pw_dir;		/* Home directory to start
						  out with*/
     
  if ( (pwd = getcwd(NULL, PATH_MAX+1)) == NULL )
  {
    perror("getcwd");
    free(pwd);
    exit(2);
  }
  owd = calloc(strlen(pwd) + 1, sizeof(char));
  memcpy(owd, pwd, strlen(pwd));
  prompt[0] = ' '; prompt[1] = '\0';
  free(pwd); //malloc'd by getcwd
  /* Put PATH into a linked list */
  pathlist = get_path();

  while ( go )
  {
    /* print your prompt */
    //generate directory
    char *workingdir;
    //calloc the args array
    char **args = calloc(MAXARGS, sizeof(char*));
    workingdir =getcwd(NULL, 0);
    if(strcmp(prompt," ") == 0){ //ignore the prompt if it is the default space
      printf("[%s]",workingdir);
    }
    else{
    printf("%s [%s]",prompt,workingdir);
    }
    free(workingdir);//malloc'd by getcwd
    printf(">>");
    /* get command line and process */
    if (fgets(input, 64 , stdin) != NULL){
    if(strcmp(input,"\n") != 0){
    int len = strlen(input);
    input[len - 1] = '\0';
    char *copy = (char*)malloc((strlen(input)+1)*sizeof(char));
    strcpy(copy,input);
    //use strtok to generate the tokens
    char *argument = strtok(input, " "); //first should be the command, next are the args
    int i = 0;
    while(argument != NULL){ //fill up arguments with args
      args[i] = argument;
      argument = strtok(NULL, " ");  
      i++;
    }// now args is full of our arguments

    /* check for each built in command and implement */
      if((strcmp(args[0],"where")==0)|| //internal command
      (strcmp(args[0],"which")==0)||
      (strcmp(args[0],"watchmail")==0)||
      (strcmp(args[0],"watchuser")==0)||
      (strcmp(args[0],"pwd")==0)||
      (strcmp(args[0],"exit")==0)||
      (strcmp(args[0],"cd")==0)||
      (strcmp(args[0],"list")==0)||
      (strcmp(args[0],"pid")==0)||
      (strcmp(args[0],"prompt")==0) ||
      (strcmp(args[0],"kill")==0) ||
      (strcmp(args[0],"printenv")==0) ||
      (strcmp(args[0],"setenv")==0) ){
        printf("Executing built-in [%s]\n",args[0]);
      }
     /*  else  program to exec */
     if(strcmp(args[0],"where")==0){
       if(args[1] != NULL)
       for(int i = 0; i<MAXARGS; i ++){
         if(args[i] != NULL)
          where(args[i],pathlist);
        else
          break; 
       }
       else
       printf("Where: not enough arguments\n");
     }
     else if(strcmp(args[0],"which")==0){
       if(args[1] != NULL){
       for(int i = 1; i<MAXARGS; i ++){
         if(args[i] != NULL)
          which(args[i],pathlist); 
        else
          break; 
       }
       }
       else{
         printf("Which: not enough arguments\n");
       }
     }
       /* find it */
       /* do fork(), execve() and waitpid() */
     else if(strcmp(args[0],"pwd")==0){
       if(args[1] != NULL)
       printf("pwd: too many arguments \n");
       else
       printWorkingDir();
     } 
     else if(strcmp(args[0],"exit")==0){
       //free fields
       free(copy);
       free(prompt);
       free(commandline);
       free(owd);
       
      while(pathlist->next != NULL) {
        struct pathelement* next = pathlist->next;
        free(pathlist);
        pathlist = next;
      }
      free(pathlist);

      free(args);
      
       return 0;
     }
    else if(strcmp(args[0],"cd")==0){
      if(args[2] != NULL){
        printf("cd: too many arguments\n");
      }
      else{
       changeDir(args);
      }
    }
     else if(strcmp(args[0],"list")==0){
       list(args);
     }
     else if(strcmp(args[0],"watchmail")==0){
        watchmail(args);
     }
     else if(strcmp(args[0],"watchuser")==0){
        watchuser(args);
     }
     else if(strcmp(args[0],"pid")==0){
       if(args[1] != NULL)
       printf("pid: too many arguments\n");
       else
       printPid();
     }
     else if(strcmp(args[0],"prompt")==0){
       promptCmd(args, prompt);
     }
     else if(strcmp(args[0], "kill")==0){
       if(args[3]!=NULL)
       printf("kill: too many arguments\n");
       else
       killProc(args);
     }
      else if(strcmp(args[0], "printenv")==0){
       printenv(envp,args,0);
     }
      else if(strcmp(args[0], "setenv")==0){
        if(args[3] != NULL){
          printf("setenv: too many arguments\n");
        }
        else
       setEnviornment(envp,args,pathlist);
     }
     else{
        int PID = fork();
        int status;
        char * res;
        if(PID == 0){ //child
          int isWild = 0;
          glob_t globbuf;
          int gl_offs_count = 0;
          if(strstr(copy,"*") || strstr(copy,"?")){
            //there are wildcards
            isWild = 1;
            //find any flags
            int flag = 1;
            for(int i = 1; i<MAXARGS; i++){
              if(args[i] == NULL)
                break;
              if((strstr(args[i],"-"))){ //a wildcard
                flag++;
              }
            }
            globbuf.gl_offs = flag;

            for(int i = 1; i<MAXARGS; i++){
              if(args[i] == NULL)
                break;
              if((strstr(args[i],"*")) || (strstr(args[i],"?"))){ //a wildcard
                gl_offs_count++; //add up all the numbers of wildcards
              }
            }
            //go through again but this time globbing 
            int first = 0;
            for(int i = 1; i<MAXARGS; i++){
              if(args[i] == NULL)
                break;
              if((strstr(args[i],"*")) || (strstr(args[i],"?"))){ //a wildcard
                if(first)
                glob(args[i], GLOB_DOOFFS | GLOB_APPEND, NULL, &globbuf);
                else
                glob(args[i], GLOB_DOOFFS, NULL, &globbuf);
              }
            }
            globbuf.gl_pathv[0] = args[0]; //set pathv[0] to the command name
            
            //set the rest
            
            for(int i = 1; i<MAXARGS; i++){
              if(args[i] == NULL)
                break;
              if((strstr(args[i],"-"))){ //a wildcard
                globbuf.gl_pathv[i] = args[i];
              }
            }
          } 
          //if the arg contains ./ ../ or starts with a /
          if(strstr(args[0],"./") != NULL || strstr(args[0],"../") != NULL || args[0][0] == '/'){ 
            if (access(args[0], X_OK) == 0){ //if the path given is to an executable
              printf("Executing [%s]\n",args[0]);
              if(execve(args[0],args,envp)==-1){
                perror("exec");
              }
            }
            else{
              printf("Executable not found.\n");
            }
          }
          /*
          // If the command is in the path
          */
          else if(res = whichRet(args[0],pathlist)){
            printf("Executing [%s]\n",args[0]);

            if(isWild){
            
              if(execvp(args[0], &globbuf.gl_pathv[0])==-1){
                perror("exec");
              }
            }
            else{
              if(execve(res,args,envp)==-1){
                perror("exec");
              }
            }
            free(res);
          }
          /*
          // If the command is not in the path
          */
          else{
            printf("Command not found\n");
          } 
        }
        else{ //parent
          waitpid(-1, &status, 0);
        }
        //fprintf(stderr, "%s: Command not found.\n", args[0]);
    }
    free(copy);
  }
  else{
    //it was a newline so we do nothing
  }
  }
  else{
      printf("\n");
      clearerr(stdin);
      //control + D was pressed
  }
      free(args);
  }

  return 0;
} /* sh() */


void *which(char *command, struct pathelement *p)
{
  char cmd[64];
  while (p) {         // WHERE
    sprintf(cmd, "%s/%s", p->element, command);
    if (access(cmd, X_OK) == 0){
      printf("%s\n", cmd);
      break;
    }
    p = p->next;
  }
} /* which() */

char* whichRet(char *command, struct pathelement *p)
{
  char cmd[256];
  while (p) {         // WHERE
    sprintf(cmd, "%s/%s", p->element, command);
    if (access(cmd, X_OK) == 0){
      char *ret = (char*)malloc((strlen(cmd)+1)*sizeof(char));
      strcpy(ret,cmd);
      return ret;
    }
     
    p = p->next;
  }
  return NULL;

} /* which() */

char *where(char *command, struct pathelement *p )
{
  char cmd[64];
  while (p) {         // WHERE
    char str[128];
    strcpy(str,"%s/");
    strcat(str,command);
    sprintf(cmd, str, p->element);
    if (access(cmd, X_OK) == 0)
      printf("%s\n", cmd);
    p = p->next;
  }
} /* where() */

void printWorkingDir(){
  char *ptr;

  ptr = getcwd(NULL, 0);

  printf("[%s]\n", ptr); 

  free(ptr);  // otherwise, there is memory leak...
}

void changeDir(char**args ){
         if(args[1] == NULL){
         if(chdir(getenv("HOME")) != 0){
            printf("Error locating home directory.\n");
         }
       }
       else if(strcmp(args[1],"-") == 0){
         if(chdir("..") != 0)
            printf("Error changing Directory.\n");
       }
       else{
         if(chdir(args[1])!= 0){
           printf("Argument is not a directory or does not exist.\n");
         }
       }
}

void list(char **args){
  struct dirent *file;
  int i = 1;
  while(1){
    DIR* directory;
    char* cwd;
    if(i == 1 && args[i] == NULL){
    
    directory = opendir(cwd = getcwd(NULL, 0));
    
    }
    else if(args[i] != NULL){
      directory = opendir(args[i]);
   
    }
    else{
      
      break;
    }
    if(directory != NULL){
      if(args[i] != NULL){ //allow default list behavior
      printf("\n%s:\n",args[i]);
      }
      while((file = readdir(directory)) != NULL){
        if(*file->d_name != '.'){ //ignore current dir and last dir along with any hidden files
                printf("%s\n",file->d_name); //print the file/dir name
        }
        
      }
      
    }
    else{ //error when opening the dir earlier
      
      printf("error opening %s\n", args[i]);
    }

    if(args[i] == NULL) //we malloced cwd
      free(cwd);
    if(closedir(directory) == -1){
      perror("closedir");
    }
    i++;
}
}

void printPid(){
  printf("%d\n",getpid());
}

void killProc(char** args){
  if(args[1]!= NULL && args[2]!= NULL){ //we have a flag
    if(strstr(args[1],"-")){ //if 1 is the one with the flag
      
      if(kill(atoi(args[2]),atoi(args[1]+1))==-1){
        perror("Kill error");
      }
    }
    else{// 2 is the flag
      if(kill(atoi(args[1]),atoi(args[2]+1))==-1){
        perror("Kill error");
      }
    }
  }
  else if(args[1] != NULL){ //no sig given
    if(kill(atoi(args[1]),SIGTERM)==-1){
      perror("Kill error");
    }

  }
  else{
    printf("kill: not enough arguments.\n");
  }
}

void promptCmd(char** args, char* prompt){
  if(args[1] != NULL){
    //prompt is given
    strcpy(prompt,args[1]);
  }
  else{
    char input[64];
    printf("input prompt prefix:");
    while(1){
    if (fgets(input, 64 , stdin) != NULL && strcmp(input,"\n") != 0)
    {
      int len = strlen(input);
      input[len - 1] = '\0';
      strcpy(prompt,input);
      break;
    }
    }
}
}

void printenv(char **envp, char **args,int forcePrintAll){
  if(args[1] != NULL && forcePrintAll == 0){
    char* res = getenv(args[1]);
    if(res != NULL)
    printf("%s\n",res);
    else
    printf("Does not exist.\n");
    
  }
  else{
  while (*envp){
    printf("%s\n", *envp++);
  }
  }
}

void setEnviornment(char **envp, char**args,struct pathelement *pathlist){
  if(args[1] == NULL){ //given no args
    printenv(envp,args,1);
  }
  else if(args[1] != NULL && args[2] == NULL){
    
    if(setenv(args[1],"",1)==-1){
      perror("Set Enviornment error");
      return;
    }
    if(strcmp(args[1],"HOME")==0){
      while(pathlist->next != NULL) {
        struct pathelement* next = pathlist->next;
        free(pathlist);
        pathlist = next;
      }
      free(pathlist);

      pathlist = get_path();
    }
  }
  else if(args[1] != NULL && args[2] != NULL){
    if(setenv(args[1],args[2],1)==-1){
      perror("Set Enviornment error");
      return;
    }
  }
  else{
    printf("setenv: Too many arguments.\n");
  }
}

void addnode(pthread_t thread, char *data) {
    node *tmp = malloc(sizeof(node));
    tmp->thread = thread;
    tmp->data = data;
    node *headref = head;
    tmp->next = NULL;
    if(!head){
      head = tmp;
      return;
    }
    while(headref->next != NULL){
      headref = headref->next;
    }
    headref->next = tmp;
}
void watchmail(char **args){
  pthread_t thread;
  if(args[2]!= NULL){if(!strcmp(args[2], "off")){ //it can take an optional second argument of "off" to turn off of watching of mails for that file.
    node *tmp = head;
    node *prev;
    if(tmp != NULL && !strcmp(tmp->data, args[1])){
      pthread_cancel(tmp->thread);
      head = tmp->next;
      free(tmp);
      return;
    }
    while(tmp->next != NULL){
      if(!strcmp(tmp->data, args[1])){ //kill the thread
        printf("Cancelling thread %d\n", tmp->thread); //delete node...
        pthread_cancel(tmp->thread);
        //Cannot free since it might break the list. Free all on exit.
      }
      prev = tmp;
      tmp = tmp->next;
    }
    prev->next = tmp->next;
  }}
  else{ //actually watch the file with pthread_create(3) 
    if(pthread_create(&thread, NULL, watchmailthread, args)) { //on success, returns 0
      fprintf(stderr, "Error creating thread\n");
    }
    //printf("THE THREAD IS: %d\n", thread);
    addnode(thread, args[1]); //append the thread ID to the linked list.
  }
}
void watchmailthread(char **args){
  struct stat sb;
  struct stat sb1;
  if(stat(args[1], &sb)){
    printf("Error getting file information\n");
  }
  int size = sb.st_size;
  while(1){ //A sleep for 1 second should be in the loop
    sleep(1);
    if(!stat(args[1], &sb1)){
      if(sb1.st_size != size){
        struct timeval tv;
        time_t timenow;
        struct tm *nowtm;
        char tmbuf[64], buf[64];
        gettimeofday(&tv, NULL);
        timenow = tv.tv_sec;
        nowtm = localtime(&timenow);
        strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%d %H:%M:%S", nowtm);
        printf("\a\n BEEP You've Got Mail in %s at %s \n", args[1], tmbuf);
        size = sb1.st_size;
      }
    }
  }
}
void watchuser(char **args){
//The first time watchuser is ran, a (new) thread should be created/started via pthread_create(3)
  pthread_t thread;
  pthread_mutex_lock(&watchusert);
  if(args[2] == NULL){ //then add a node
    node *tmp = malloc(sizeof(node));
    tmp->thread = thread; //doesn't matter
    tmp->data = args[1]; //the user to watch
    node *headref = headu;
    tmp->next = NULL;
    if(!headu){
      headu = tmp;
      return;
    }
    while(headref->next != NULL){
      headref = headref->next;
    }
    headref->next = tmp;
  }
  else if(args[2]!= NULL){if(!strcmp(args[2], "off")){ //it can take an optional second argument of "off" to turn off of watching of mails for that file.
    node *tmp = headu;
    node *prev;
    if(tmp != NULL && !strcmp(tmp->data, args[1])){
      pthread_cancel(tmp->thread);
      head = tmp->next;
      free(tmp);
      return;
    }
    while(tmp->next != NULL){
      if(!strcmp(tmp->data, args[1])){ //kill the thread
        printf("Cancelling thread %d\n", tmp->thread); //delete node...
        pthread_cancel(tmp->thread);
        //Cannot free since it might break the list. Free all on exit.
      }
      prev = tmp;
      tmp = tmp->next;
    }
    prev->next = tmp->next;
  }}
  pthread_mutex_unlock(&watchusert);
  if(!thread){ 
    if(pthread_create(&thread, NULL, watchuserthread, args)) { //on success, returns 0
      fprintf(stderr, "Error creating thread\n");
    }
    printf("THE THREAD IS: %d\n", thread);
  }
}
//The thread should get the list of users from a global linked list which the calling function 
//(of the main thread) will modify by either inserting new users or turning off existing watched users.
void watchuserthread(char **args){
  while(1){
    struct utmpx *up;
    node *tmp = headu;
    while(tmp){
      tmp = tmp->next;
    }
    setutxent();			/* start at beginning */
    while (up = getutxent())	/* get an entry */
    {
      if ( up->ut_type == USER_PROCESS && !strcmp(tmp->data, up->ut_user)){	/* only care about users */
        printf("%s has logged on %s from %s\n", up->ut_user, up->ut_line, up ->ut_host);
      }
    }
    sleep(20);
  }
}