#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include "loop.h"

#define INPUT_CHAR '<'
#define OUTPUT_CHAR '>'
#define READ_FLAGS O_RDONLY
#define WRITE_FLAGS O_WRONLY | O_CREAT | O_TRUNC

/* Description: the main loop of the shell
 */
void mainLoop()
{
   unsigned numCmds = 0;
   char cmdLine[MAX_LEN] = ""; 
   char *allCommands[MAX_CMDS] = { NULL };
   pipeStage stages[MAX_CMDS];

   /* main loop of printing prompt and running commands */
   while (1)
   {
      fprintf(stdout, "~ ");

      /* get the entire command we are running for this loop of the shell */
      if (readLine(cmdLine))
         continue;

      /* parse the command to get all necessary information */
      if (parseCommandLine(&numCmds, cmdLine, allCommands))
         continue;

      /* create the stages and check for invalid pipes */
      if (makeStages(allCommands, numCmds, stages))
         continue;

      /* create all the stages for the pipeline of the current command */
      executeCommands(numCmds, stages);
   }
}

/* Description: reads in a line and sees if it is valid, and exits if necessary
 *
 * Parameters:
 *    line: pointer to store the current command line to
 */
int readLine(char *line)
{
   int ret = EXIT_SUCCESS;
   const char delim[] = " ", exitStr[] = "exit";
   char *test, linecopy[MAX_LEN] = "";

   /* instant EOF means exit */ 
   if (NULL == fgets(line, MAX_LEN, stdin)) {
      fprintf(stdout, "exit\n");
      exit(EXIT_SUCCESS);
   }

   /* line too long */
   if (strlen(line) == 1024 && strchr(line, '\n') == 0) {
      fprintf(stderr, "cshell: Command line too long\n");
      flushLine();
      ret = EXIT_FAILURE;
   }

   /* get rid of the newline */
   removeNewline(line);

   /* test if "exit" was typed */
   strcpy(linecopy, line);
   test = strtok(linecopy, delim);

   /* empty line */
   if (test == NULL)
      ret = EXIT_FAILURE;

   /* exit typed */
   else if (strcmp(exitStr, test) == 0)
      exit(EXIT_SUCCESS);

   return ret;
}

/* Description: flushes the line if too many characters were entered
 */
void flushLine()
{
   int curChar;

   while ((curChar = getchar()) != '\n' && curChar != EOF);
}

/* Description: gets rid of the newline from the command
 *
 * Parameters:
 *    line: the current command line
 */
void removeNewline(char line[MAX_LEN])
{
   int i;

   for (i = 0; i < MAX_LEN; i++) {
      if (line[i] == '\n') {
         line[i] = '\0';
         return;
      }
   }
}

/* Description: checks if the string is all spaces
 *
 * Parameters:
 *    str: the given string
 *    len: the length of the given string
 *
 * Return: 0 if all is successfull and 1 otherwise
 */
int allSpaces(char *str, int len)
{
   int ret = 1;
   unsigned i;

   for (i = 0; i < len; i++) {
      if (! isspace(str[i]))
         ret = 0;
   }

   return ret;
}

/* Description: the main parsing of the given command, checks for errors BEFORE
 *    moving on and actually attempting to execute commands
 *
 * Parameters:
 *    numCmds: pointer to update the total number of commands
 *    cmdLine: the current command we are on in the shell
 *    allCommands: the array of strings to put the individual commands into
 *
 * Return: 0 if everything is successful, otherwise 1
 */
int parseCommandLine(unsigned *numCmds,
                     char *cmdLine,
                     char *allCommands[MAX_CMDS])
{
   unsigned cmds = 0, idx = 0;
   char *command;
   const char delim[] = "|";

   /* separate into commands based on number of pipes, checking for errors */
   if (cmdLine[0] == '|' || cmdLine[strlen(cmdLine) - 1] == '|') {
      fprintf(stderr, "cshell: Invalid pipe\n");
      return EXIT_FAILURE;
   }

   command = strtok(cmdLine, delim);

   while (command != NULL)
   {
      if (allSpaces(command, strlen(command))) {
         fprintf(stderr, "cshell: Invalid pipe\n");
         return EXIT_FAILURE;
      }
      allCommands[idx++] = command;
      cmds++;
      command = strtok(NULL, delim);
   }

   if (cmds > 20) {
      fprintf(stderr, "cshell: Too many commands\n");
      return EXIT_FAILURE;
   }

   *numCmds = cmds;

   return EXIT_SUCCESS;
}

/* Description: parses each individual command in the given array and makes it
 *    into an array that can be executed by execvp and puts that into a 
 *    pipeStage structure
 *
 * Parameters:
 *    commands: an array of the commands that were parsed
 *    numCmds: the number of commands in the array
 *    stages: the array of stages that need to be updated
 *
 * Return: 0 if all is successfull and 1 otherwise
 */
int makeStages(char *commands[MAX_CMDS],
               unsigned numCmds,
               pipeStage stages[MAX_CMDS])
{
   unsigned i;

   for (i = 0; i < numCmds; i++) {
      if (parseCommand(commands[i], &stages[i]))
         return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

/* Description: parses a command to update the stage structure with the proper
 *    information
 *
 * Parameters:
 *    command: the current command as a string
 *    stage: the stage structure to update
 */
int parseCommand(char *command, pipeStage *stage)
{
   int i, argIdx = 0, argCount = -1, ret = EXIT_SUCCESS;
   char *words[MAX_WORDS] = { NULL };

   /* split up the words in the command line */
   splitWords(command, words);

   for (i = 0; words[i] != NULL; i++) {
      /* if its a redirection symbol */
      if (redirectSymbols(words[i]))
         redirectionCheck(words, &i, stage, &ret);
      /* must be command or args */
      else if (commandCheck(words, &i, &argCount, &argIdx, stage))
         return EXIT_FAILURE;
   }

   return ret;
}

int redirectSymbols(char *symbol)
{
   return (strchr(symbol, INPUT_CHAR) != NULL 
      || strchr(symbol, OUTPUT_CHAR) != NULL);
}

void splitWords(char *command, char *words[MAX_WORDS])
{
   int idx = 0;
   char *curWord;
   const char delim[2] = " ";

   curWord = strtok(command, delim);
   words[idx++] = curWord;

   while (curWord != NULL) {
      curWord = strtok(NULL, delim);
      words[idx++] = curWord;
   }
}

void redirectionCheck(char *words[MAX_WORDS],
                      int *curIdx,
                      pipeStage *stage,
                      int *ret)
{
   stage->redirection = 1; 

   if (words[(*curIdx) + 1] == NULL) {
      fprintf(stderr, "cshell: syntax error\n");
      resetStage(stage);
      *ret = EXIT_FAILURE;
   }
   /* if it is output redirection */
   else if (strchr(words[(*curIdx)], INPUT_CHAR) == NULL)
      stage->outFile = words[(*curIdx) + 1]; 
   /* it is input redirection */
   else
      stage->inFile = words[(*curIdx) + 1]; 

   (*curIdx) += 1;
}

int commandCheck(char *words[MAX_WORDS],
                 int *curIdx,
                 int *argCount,
                 int *argIdx,
                 pipeStage *stage)
{
   int idx = *curIdx;

   if (*argIdx == 0)
      stage->file = words[idx];

   /* while next words isnt a redirection symbol */
   while (words[idx] != NULL && ! redirectSymbols(words[idx])) {
      stage->args[(*argIdx)++] = words[idx++];
      (*argCount)++;
      if (*argCount > MAX_ARGS) {
         fprintf(stderr, "cshell: %s: Too many arguments\n", stage->args[0]);
         resetStage(stage);
         return EXIT_FAILURE;    
      }
   }

   (*curIdx) = idx - 1;

   return EXIT_SUCCESS;
}

/* Description: launches the commands and creates a pipeline if necessary
 *
 * Parameters:
 *    numCmds: number of commands in the pipeline
 */
void executeCommands(int numCmds, pipeStage stages[MAX_CMDS])
{
   int i, curFds[2], prevFd, status;
   pid_t pid;

   for (i = 0; i < numCmds; i++) {
      copyAndPipe(i, numCmds, curFds, &prevFd);
      /* fork the child and check for error */
      if (-1 == (pid = fork()))
         errorAndExit("fork error");
      /* parent code, CHILD process */
      else if (pid == 0)
         childProcess(i, numCmds, curFds, prevFd, &stages[i]);
      /* parent code, PARENT process */
      else
         parentProcess(i, numCmds, curFds, prevFd);
      /* clear inFile and outFile */
      resetStage(&stages[i]);
   }
   waitForChildren(numCmds, &status);
}

/* Description: manage and launch pipes, this is before the first fork and 
 *    then again after launching child process but before launching the next
 *
 * Parameters:
 *    numCmd: the number of the command we're on
 *    numCmds: the total number of commands in pipeline
 *    curFds: array to store current fds
 *    prevFds: array to update previous fds
 */
void copyAndPipe(int numCmd, int numCmds, int curFds[2], int *prevFd)
{
   /* copy necessary previous fd over */
   if (numCmd > 0 && numCmd != (numCmds - 1))
      *prevFd = curFds[0];
   /* only make pipe when necessary */
   if (numCmds > 1 && numCmd < (numCmds - 1))
      makePipe(curFds);
}

void childProcess(
   int numCmd,
   int numCmds,
   int curFds[2],
   int prevFd,
   pipeStage *stage)
{
   /* with more than 1 command and covering the first and middle processes */
   if (numCmds > 1 && numCmd < (numCmds - 1))
      closeAndDupChildren(numCmd, curFds, prevFd);
   /* more than 1 command and last process */
   else if (numCmds > 1 && numCmd == (numCmds - 1))
      dupCheck(curFds[0], STDIN_FILENO);

   /* do proper commands for redirection if necessary */
   if (stage->redirection)
      redirect(stage);

   /* execute command */
   executeCommand(stage);
}

void redirect(pipeStage *stage)
{
   /* file for input redirection */
   if (stage->inFile != NULL)
      openAndDup(stage->inFile, READ_FLAGS, STDIN_FILENO); 
   /* file for output redirection */
   if (stage->outFile != NULL)
      openAndDup(stage->outFile, WRITE_FLAGS, STDOUT_FILENO); 

}

void openAndDup(char *fileName, int flags, int filedes2)
{
   int fd;

   /* open the file with proper flags and check for error */
   if (-1 == (fd = open(fileName, flags, 0600))) {
      fprintf(stderr, "cshell: %s: ", fileName);
      errorAndExit(NULL);
   }

   /* associate the created fd with stdin or stdout */
   dupCheck(fd, filedes2);
}

void closeAndDupChildren(int numCmd, int curFds[2], int prevFd)
{
   /* first process */
   if (numCmd == 0) {
      closeFd(curFds[0]);
      dupCheck(curFds[1], STDOUT_FILENO);
   }
   /* middle processes */
   else {
      closeFd(curFds[0]);
      dupCheck(prevFd, STDIN_FILENO);
      dupCheck(curFds[1], STDOUT_FILENO);
   }
}

void executeCommand(pipeStage *stage)
{
   execvp(stage->file, stage->args);
   fprintf(stderr, "cshell: %s: ", stage->file);
   perror(NULL);
   exit(EXIT_FAILURE);
}

void parentProcess(int numCmd, int numCmds, int curFds[2], int prevFd)
{
   /* more than 1 command and first/middle processes */
   if (numCmds > 1 && numCmd < (numCmds - 1))
      closeParent(numCmd, curFds, prevFd);
   /* more than 1 command and last process */
   else if (numCmds > 1 && numCmd == (numCmds - 1))
      closeFd(curFds[0]);

}

void closeParent(int numCmd, int curFds[2], int prevFd)
{
   /* first process */
   if (numCmd == 0)
      closeFd(curFds[1]);
   /* middle processes */
   else {
      closeFd(prevFd);
      closeFd(curFds[1]);
   }
}

void resetStage(pipeStage *stage)
{
   int i;

   stage->redirection = 0;
   stage->file = NULL;
   stage->inFile = NULL;
   stage->outFile = NULL;

   for (i = 0; i < MAX_ARGS + 2; i++)
      stage->args[i] = NULL;
}

/* Description: asynchronously waits for children to finish
 *
 * Parameters:
 *    numChildren: number of children to wait for
 *    status: to store the info from call to wait()
 */
void waitForChildren(int numChildren, int *status)
{
   int i;

   for (i = 0; i < numChildren; i++)
      wait(status);
}

/* Description: calls dup2 and checks its return value
 * 
 * Parameters:
 *    fd1: the file descriptor to use as filedes arg
 *    fd2: the file descriptor to use as filedes2 arg
 */
void dupCheck(int fd1, int fd2)
{
   if (-1 == dup2(fd1, fd2))
      errorAndExit("dup2 error");
}

/* Description: creates a pipe and checks for proper return value, exiting if
 *    there's a failure
 *
 * Parameters:
 *    fd: the array to store the file descriptors
 */
void makePipe(int fd[2])
{
   if (pipe(fd) < 0)
      errorAndExit("pipe error");
}

/* Description: closes a file descriptor and checks for errors
 *
 * Parameters:
 *    fd: the file descriptor to close
 */
void closeFd(int fd)
{
   if (close(fd) < 0) {
      fprintf(stderr, "fd = %d ", fd);
      errorAndExit("close error");
   }
}

/* Description: for when the return value of a function indicates an error
 *
 * Parameters:
 *    message: the message to display
 */
void errorAndExit(char* message)
{
   perror(message);
   exit(EXIT_FAILURE);
}
