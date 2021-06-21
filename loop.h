#ifndef LOOP_H
#define LOOP_H

#define MAX_LEN 1025
#define MAX_ARGS 10
#define MAX_CMDS 20
#define MAX_WORDS 512

/* check return of memory allocation calls */
#define CK_ALLOC(_ALLOC) {\
   if (NULL == (_ALLOC)) {\
      perror("wf: ");\
      exit(EXIT_FAILURE);\
   }\
}

/* respresents one "stage" in the pipeline, that has the array for execvp */
typedef struct
{
   char *file;
   char *args[MAX_ARGS + 2]; /* need room for first arg and NULL in execvp */
   int redirection;
   char *inFile;
   char *outFile;
} pipeStage;

void mainLoop();
int readLine(char *line);
void flushLine();
void removeNewline(char line[MAX_LEN]);
int allSpaces(char *str, int len);
int parseCommandLine(
   unsigned *numCmds,
   char *cmdLine,
   char *allCommands[MAX_CMDS]);
int makeStages(
   char *commands[MAX_CMDS],
   unsigned numCmds,
   pipeStage stages[MAX_CMDS]);
int parseCommand(char *command, pipeStage *stage);
int redirectSymbols(char *symbol);
void splitWords(char *command, char *words[MAX_WORDS]);
void redirectionCheck(
   char *words[MAX_WORDS],
   int *curIdx,
   pipeStage *stage,
   int *ret);
int commandCheck(
   char *words[MAX_WORDS],
   int *curIdx,
   int *argCount,
   int *argIdx,
   pipeStage *stage);
void executeCommands(int numCmds, pipeStage stages[MAX_CMDS]);
void copyAndPipe(int numCmd, int numCmds, int curFds[2], int *prevFd);
void childProcess(
   int numCmd,
   int numCmds,
   int curFds[2],
   int prevFd,
   pipeStage *stage);
void redirect(pipeStage *stage);
void openAndDup(char *fileName, int flags, int filedes2);
void closeAndDupChildren(int numCmd, int curFds[2], int prevFd);
void executeCommand(pipeStage *stage);
void parentProcess(int numCmd, int numCmds, int curFds[2], int prevFd);
void closeParent(int numCmd, int curFds[2], int prevFd);
void resetStage(pipeStage *stage);
void waitForChildren(int numChildren, int *status);
void dupCheck(int fd1, int fd2);
void makePipe(int fd[2]);
void closeFd(int fd);
void errorAndExit(char* message);

#endif
