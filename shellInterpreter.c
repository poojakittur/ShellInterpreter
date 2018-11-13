#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<unistd.h>
#include<fcntl.h>
#include<ctype.h>

int maxBgProcess = 0;
struct bgProcess{
	pid_t pid;
	char command[1024];
}bgProcesses[100];

char **tokenize_input(char *, char *);
char *findAbsolutePath(char *);
void executeCommands(char **, int, int, char *, int, char *);
char *trimSpaces(char *);
int executeBuiltInCommands(char **);
char *tokenizeRedirection(char *, char *);

int main(int argc, char **argv){

	char line[1024];
	pid_t child_pid;
	char **inputArguments;
	int i = 0; 
	char *delimiter; 
	int isBackground = 0;
	int isOutputRedirection = 0, isInputRedirection = 0;
	char *outputFilename, *inputFilename;

	while(1){	

		isInputRedirection = isOutputRedirection = isBackground = 0;
		printf("MyShell:");
		if(fgets(line, sizeof line, stdin)){
			line[strcspn(line, "\n")] = '\0';
		
		}
		
		if(strcmp(line, "") == 0)
			continue;
		
		//Check if input command is a background command
		if(strstr(line, "bg")){
			strncpy(line, line+2, sizeof line);
			isBackground = 1;
			strncpy(bgProcesses[maxBgProcess].command, line, sizeof line);
		}

		//check for I/O redirection : ( > )
		if(strstr(line, ">")){
			isOutputRedirection = 1;
			outputFilename = tokenizeRedirection(line, ">");
			
			//check if there is a input filename appended in output filename
			if(strstr(outputFilename , "<")){
				isInputRedirection = 1;
				inputFilename = tokenizeRedirection(outputFilename, "<");
				outputFilename = trimSpaces(outputFilename);
			}
			//printf("Output filename is : %s\n", outputFilename);
		}
		
		//check for I/O redirection : ( < )
		if(strstr(line, "<")){
			isInputRedirection = 1;
			inputFilename = tokenizeRedirection(line, "<");
		}

		inputArguments = tokenize_input(trimSpaces(line)," ");
		
		executeCommands(inputArguments, isBackground, isOutputRedirection, outputFilename, isInputRedirection, inputFilename);

		free(inputArguments);
	}
	
}

char *tokenizeRedirection(char *line, char *symbol){
	char *filename;
	strtok(line,symbol);
	filename = strtok(NULL,symbol);
	filename = trimSpaces(filename);
	return filename;
}

char * trimSpaces(char *filename){

	int i;
	while (isspace(*filename)) filename++;   // skip left side white spaces
   	for (i = strlen (filename) - 1; (isspace (filename[i])); i--) ;   // skip right side white spaces
	filename[i + 1] = '\0';
	return filename;
}

char **tokenize_input(char *inputLine, char *delimiter)
{	
	int bufferSize = 64;
	char **inputArg = malloc(bufferSize * sizeof(char*));
	char *inputLinePtr;
	int index = 0;

	if (!inputArg) {
    		perror("Allocation error\n");
  	}
	
	inputLinePtr = strtok(inputLine,delimiter);

	while(inputLinePtr != NULL){
		inputArg[index] = inputLinePtr;
		index++;

		if(index >= bufferSize)
		{
			bufferSize = bufferSize + bufferSize;
			inputArg = realloc(inputArg, bufferSize * sizeof(char*));

			if (!inputArg) {
    				perror("Allocation error\n");
  			}
			
		}
		
		inputLinePtr = strtok(NULL, delimiter);
	}
	
	inputArg[index] = NULL;
	return inputArg;
}

char *findAbsolutePath(char *inputCommand){
	char *pathValue = malloc(8 * sizeof(char*));
	char *pathname = malloc(8 * sizeof(char*));
	char **arrayOfFolders;
	int i = 0;
	int length;
	char *tempArg = malloc(8 * sizeof(char*));
	int accessStatus;

	accessStatus = access(inputCommand, X_OK);
	if(accessStatus == 0){
		return inputCommand;
	}

	//fetching the Value of Path Variable
	strcpy(pathValue, getenv("PATH"));
	arrayOfFolders = tokenize_input(pathValue, ":");
	length = sizeof(arrayOfFolders);
	
	for(i=0; i<sizeof(arrayOfFolders); i++)
	{
		strcpy(tempArg, arrayOfFolders[i]);
		strcat(tempArg, "/");
		sprintf(pathname, "%s%s",tempArg, inputCommand);		
		
		//finding the correct executable
		accessStatus = access(pathname, X_OK);
		if(accessStatus == 0)
		{
			break;
		}
	}
	free(pathValue);
	free(tempArg);
	return pathname;
	
}

void executeCommands(char **inputArguments, int isBackground, int isOutputRedirection, char *outputFilename, int isInputRedirection, char 				*inputFilename)
{
	pid_t child_pid;
	int status;
	char *pathname;
	int argLength, i = 0;
	int outputFd, inputFd;
	int j = 0;

	//check if command is built-in
	if(executeBuiltInCommands(inputArguments)){
		return;
	}
	
	pathname = findAbsolutePath(inputArguments[0]);
	printf("Pathname received is : %s\n", pathname);
	child_pid = fork();

	if(child_pid == 0){
		//child process

		if(isOutputRedirection){
			outputFd = open(outputFilename, O_WRONLY |O_CREAT, 0644);
			close(1);
			dup(outputFd);
			close(outputFd);
		}

		if(isInputRedirection){
			inputFd = open(inputFilename, O_RDONLY);
			close(0);
			dup(inputFd);
			close(inputFd);
		}	
		if(execv(pathname, inputArguments) == -1){
			perror("Invalid command entered");
		}

		_exit(1);
		printf("This wont be printed if command is executed correctly ");
	} else if(child_pid < 0){
		printf("Error while executing");
		perror("Error while creating the child");
	} else {
		//parent process
		if(isBackground){
			bgProcesses[maxBgProcess++].pid = child_pid;
			status = waitpid(child_pid, &status, WNOHANG);

		}else{
			while(wait(0) != child_pid);
		}
	}
}

int executeBuiltInCommands(char **inputArguments){

	int isBuiltInCmd = 0;
	int i;
	int status;

	if (strcmp(inputArguments[0], "cd") == 0) {
			isBuiltInCmd = 1;
      			if(inputArguments[1] == NULL)
				perror("Less number of arguments");
			else{
				if(chdir(inputArguments[1]) != 0) {
      					perror("Error while changing the directory");
    				}
			}
    		}else if(strcmp(inputArguments[0], "exit") == 0){
			isBuiltInCmd = 1;
			for(i=0; i<maxBgProcess; i++){
				if(waitpid(bgProcesses[i].pid, &status, WNOHANG) == 0){
					kill(bgProcesses[i].pid, SIGKILL);
				}
			}
			maxBgProcess = 0;
			exit(0);
		}else if(strcmp(inputArguments[0], "processes") == 0){
			isBuiltInCmd = 1;
			if(maxBgProcess == 0){
				printf("No background processes running !!!\n");
				return isBuiltInCmd;
			}
			printf("Maximum background processes are : %d\n",maxBgProcess);
			printf("**********************Background Processes*********************\n");
			printf("PID\t\t\tCommand\n"); 
			for(i=0; i<maxBgProcess; i++){
				if(waitpid(bgProcesses[i].pid, &status, WNOHANG) == 0){
					printf("%d\t\t\t%s\n",bgProcesses[i].pid, bgProcesses[i].command); 
				}
			}
		}

		return isBuiltInCmd;

}

