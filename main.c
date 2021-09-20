#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

//test
#define MAX 512 //user's input is less than 512 bytes
int hist_count = 0; //global variable for MyHistory function
int alias_count = 0;  //global variable for MyAlias function
pid_t ppid; //gloabal parent id
pid_t cpid; //global child id

void InteractiveMode();
void BatchMode(char *file);

int ParseCommands(char *userInput); //e.g., "ls -a -l; who; date;" is converted to "ls -al" "who" "date"
int ParseArgs(char *full_line, char *args[]); //e.g., "ls -a -l" is converted to "ls" "-a" "-l"
void ExecuteCommands(char *command, char *full_line);

void MyCD(char *dir_input, int arg_count);
void MyExit();
void MyPath(char *args[], int arg_count);
void MyHistory(char *args[], int arg_count);

void CommandRedirect(char *args[], char *first_command, int arg_count, char *full_line);
void PipeCommands(char *args[], char *first_command, int arg_count);
void signalHandle(int sig);
void MyAlias(char *args[], int arg_count);
void io_redirect(char *command, char *full_line);

char CURRENT_DIRECTORY[MAX]; //current directory
char *COMMANDS[MAX]; //commands to be executed
char *MYHISTORY[MAX]; //shell command history
char *MYPATH; //my PATH variable
char *MYALIAS[MAX]; //alias variable
const char *ORIG_PATH_VAR; //The original PATH contents
char *prompt;

int EXIT_CALLED = 0;//Functions seem to treat this as a global variable -DM


int main(int argc, char *argv[]){
    //error checking on user's input
	  if (!(argc < 3)) {
		  fprintf(stderr, "Error: Too many parameters\n");
		  fprintf(stderr, "Usage: './output [filepath]'\n");
		  exit(0);//No memory needs to be cleared
	  }

    //initialize your shell's enviroment
    MYPATH = (char*) malloc(1024);
	  memset(MYPATH, '\0', sizeof(MYPATH));
	  ORIG_PATH_VAR = getenv("PATH"); // needs to include <stdlib.h>

    //save the original PATH, which is recovered on exit
	  strcpy(MYPATH, ORIG_PATH_VAR);

    //make my own PATH, namely MYPATH
	  setenv("MYPATH", MYPATH, 1);

	  if(argc == 1) InteractiveMode();
     else if(argc == 2) BatchMode(argv[1]);

		//gets the parent id and sets it to ppid
    ppid = getpid();

    //handles the signal (Ctrl + C)
    signal(SIGINT, signalHandle);

    //handles the signal (Ctrl + Z)
    signal(SIGTSTP, signalHandle);

    //free all variables initialized by malloc()
	  free(MYPATH);

	return 0;
}

void BatchMode(char *file){

	FILE *fptr = fopen(file, "r");
    //error checking for fopen function
    if(fptr == NULL) {
		fprintf(stderr, "Error: Batch file not found or cannot be opened\n");
		MyExit();
    }

    char *batch_command_line = (char *)malloc(MAX);
    memset(batch_command_line, '\0', sizeof(batch_command_line));

    //reads a line from fptr, stores it into batch_command_line
    while(fgets(batch_command_line, MAX, fptr)){
	//remove trailing newline
	batch_command_line[strcspn(batch_command_line, "\n")] = 0;
	printf("Processing batchfile input: %s\n", batch_command_line);

        //parse batch_command_line to set the array COMMANDS[]
        //for example: COMMANDS[0]="ls -a -l", COMMANDS[1]="who", COMMANDS[2]="date"
        int cmd_count = ParseCommands(batch_command_line);

        //execute commands one by one
        for(int i=0; i< cmd_count; i++){
            char *temp = strdup(COMMANDS[i]); //for example: ls -a -l
            temp = strtok(temp, " "); //get the command
            ExecuteCommands(temp, COMMANDS[i]);
            //free temp
			free(temp);
        }
    }
    //free batch_command_line, and close fptr
	free(batch_command_line);
	fclose(fptr);
}

int ParseCommands(char *str){

	int i = 0;

	char *token = strtok(str, ";"); //breaks str into a series of tokens using ;

	while(token != NULL){
		//error checking for possible bad user inputs
		//Removes Spaces at beginning
		while (token[0] == ' ') {
			int size = strlen(token);
			for (int j=0; j<size; j++) {
				token[j] = token[j+1];
			}
		}

		//If after, removing all whitespaces we're left with a NULL char,
		//then the command is empty and will be ignored
		if (token[0] == '\0') {
			token = strtok(NULL, ";");
			continue;
		}

		//Removes all but one whitespace in between args
		for (int j=0; j<strlen(token); j++) {
			//fprintf(stderr,"Token Edit: %s\n", token);
			if (token[j] == ' ' && token[j+1] == ' ') {
				int size = strlen(token);
				for (int k=j; k<size; k++)
					token[k] = token[k+1];
				j--;
			}
		}

        //save the current token into COMMANDS[]
        COMMANDS[i] = token;
        i++;
        //move to the next token
        token = strtok(NULL, ";");
	}

	return i;
}

void ExecuteCommands(char *command, char *full_line){

	char *args[MAX]; //hold arguments

	MYHISTORY[hist_count%20] = strdup(full_line); //array of commands
	hist_count++;

    //save backup full_line
    char *backup_line = strdup(full_line);

    if (strcmp(command, "alias") == 0 && strchr(full_line, '=') != NULL) {
		//break full_line into a series of tokens by the delimiter space (or " ")
		char *token = strchr(full_line, ' ');
		while (token[0] == ' ') {
			int size = strlen(token);
			for (int j=0; j<size; j++) {
				token[j] = token[j+1];
			}
		}
		MYALIAS[alias_count] = strdup(token);
		alias_count++;
	}
	else {
		//parse full_line to get arguments and save them to args[] array
		int arg_count = ParseArgs(full_line, args);

		//restores full_line
        strcpy(full_line, backup_line);
        free(backup_line);

		//check if built-in function is called
		if(strcmp(command, "cd") == 0)
			MyCD(args[0], arg_count);
		else if(strcmp(command, "exit") == 0)
			EXIT_CALLED = 1;
		else if(strcmp(command, "path") == 0)
			MyPath(args, arg_count);
		else if(strcmp(command, "myhistory") == 0)
			MyHistory(args, arg_count);
		else if(strcmp(command, "alias") == 0)
			MyAlias(args, arg_count);
		else
			CommandRedirect(args, command, arg_count, full_line);
		//free memory used in ParsedArgs() function
		for(int i=0; i<arg_count-1; i++){
			if(args[i] != NULL){
				free(args[i]);
				args[i] = NULL;
			}
		}
	}
}

int ParseArgs(char *full_line, char *args[]){
	int count = 0;

    //break full_line into a series of tokens by the delimiter space (or " ")
	char *token = strtok(full_line, " ");
	//skip over to the first argument
	token = strtok(NULL, " ");

    while(token != NULL){
        //copy the current argument to args[] array
        args[count] = strdup(token);
        count++;
        //move to the next token (or argument)
        token = strtok(NULL, " ");
    }

    return count + 1;
}

void CommandRedirect(char *args[], char *first_command, int arg_count, char *full_line){
	pid_t pid;
	int status;

	//if full_line contains pipelining and redirection, error displayed
	if (strchr(full_line, '|') != NULL && (strchr(full_line, '<') != NULL || strchr(full_line, '>') != NULL)) {
	    fprintf(stderr,"Command cannot contain both pipelining and redirection\n");
	}
	//if full_line contains "<" or ">", then io_redirect() is called
	else if (strchr(full_line, '<') != NULL || strchr(full_line, '>') != NULL) {
		io_redirect(first_command, full_line);
	}
	//if full_line contains "|", then PipeCommands() is called
	else if (strchr(full_line, '|') != NULL) {
		PipeCommands(args, first_command, arg_count);
	}
	else {//else excute the current command
		//set the new cmd[] array so that cmd[0] hold the actual command
		//cmd[1] - cmd[arg_count] hold the actual arguments
		//cmd[arg_count+1] hold the "NULL"
		char *cmd[arg_count + 1];
		cmd[0] = first_command;
		for (int i=1; i<arg_count; i++)
			cmd[i] = args[i-1];
		cmd[arg_count] = '\0';

		pid = fork();
		if(pid == 0) {
			execvp(*cmd, cmd);
			fprintf(stderr,"%s: command not found\n", *cmd);
			MyExit();//Ensures child exits after executing command
		}
		else wait(&status);
	}
}

void InteractiveMode(){
	/*
		For some reason, this fixes the seg faults
		experienced in ParseArgs. No idea why
		-DM

		Update: This var may no longer be necessary, further testing needed
		-DM
	*/
	int status = 0;

    //get custom prompt
    prompt = (char*)malloc(MAX);
    printf("Enter custom prompt: ");
    fgets(prompt, MAX, stdin);

    //remove newline from prompt
    if (prompt[strlen(prompt)-1] == '\n') {
        prompt[strlen(prompt)-1] = '\0';
    }

	while(1){
		char *str = (char*)malloc(MAX);

		printf("%s> ", prompt);
		fgets(str, MAX, stdin);

		//error checking for empty commandline
		if (strlen(str) == 1) {
			continue;
		}

		//remove newline from str
		if (str[strlen(str)-1] == '\n') {
			str[strlen(str)-1] = '\0';
		}

		//parse commands
		int cmd_num = ParseCommands(str);//this function can be better designed

		//execute commands that are saved in COMMANDS[] array
		for(int i=0; i < cmd_num; i++){
			char *temp = strdup(COMMANDS[i]);
			temp = strtok(temp, " ");
			ExecuteCommands(temp, COMMANDS[i]);
			//free temp
			free(temp);
		}

		//ctrl-d kill

		free(str);

		// if exit was selected
		if(EXIT_CALLED) {
		    free(prompt);
		    MyExit();
		}
	}
}

void MyCD(char *dir_input, int arg_count){
	    if (arg_count == 1) {
	        chdir(getenv("HOME"));
	        printf("PWD: %s\n", getenv("PWD"));
	    }
	    else if (chdir(dir_input) == -1) {
	        perror(dir_input);
	    }
	    else {
	        printf("PWD: %s\n", getenv("PWD"));
	    }
}

void MyExit(){ // printf free malloc (IGNORE: For highlighting puposes) -DM
    //clean up
	//Max 20 Elements in History
	if (hist_count > 20) {
		hist_count = 20;
	}
	//Free all memory in MYHISTORY
	for(int i=0; i<hist_count; i++){
		if(MYHISTORY[i] != NULL){
			free(MYHISTORY[i]);
			MYHISTORY[i] = NULL;
		}
	}
	//Free all memory in MYALIAS
	for(int i=0; i<alias_count; i++){
		if(MYALIAS[i] != NULL){
			free(MYALIAS[i]);
			MYALIAS[i] = NULL;
		}
	}
    //reset path
    setenv("MYPATH", ORIG_PATH_VAR, 1);

    free(MYPATH);

    exit(0);
}

void MyPath(char *args[], int arg_count){

    if(arg_count == 1) { //output path
        printf("%s\n", MYPATH);
    }
    else if(arg_count == 3) {
        if(strcmp(args[0], "+") == 0) { //append path name
            strcat(MYPATH, ":"); //append path name seperator
            if(strcmp(args[1], "$HOME") == 0) { //append home directory
                char *home = getenv("HOME");
                strcat(MYPATH, home);
            }
            else if(args[1][0] == '~') { //append home directory and extra
                char *home = getenv("HOME");
                strcat(MYPATH, home);
                strcat(MYPATH, strchr(args[1], '/'));
            }
            else { //append custom path name
                strcat(MYPATH, args[1]);
            }

            setenv("MYPATH", MYPATH, 1);
            printf("Path is now: %s\n", MYPATH); //output new path
        }
        else if(strcmp(args[0], "-") == 0) { //remove path name
            char *path_names[MAX];
            char *new_path;
            new_path = (char*) malloc(1024);
            memset(new_path, '\0', sizeof(new_path));
            int i = 0;

            //split path into path names
            char *token = strtok(MYPATH, ":");
            while( token != NULL ) {
                path_names[i] = token;
                token = strtok(NULL, ":"); //retrieve next path name
                i++;
            }
            path_names[i] = NULL; //add NULL termination

            //find and remove path name
            for(int j=0; j<i; j++) {
                if(strcmp(path_names[j], args[1]) == 0) {
                    path_names[j] = NULL;
                    if(j == i-1) { //case for final path name being remo$
                        i--;
                    }
                }
            }

            //reconstruct path
            for(int j=0; j<i-1; j++) {
                if(path_names[j] != NULL) {
                    strcat(new_path, path_names[j]);
                    strcat(new_path, ":");
                }
            }
            strcat(new_path, path_names[i-1]); //append final path name without $
            strcpy(MYPATH, new_path);

            setenv("MYPATH", MYPATH, 1); //set new path
            printf("Path is now: %s\n", MYPATH); //output new path
            free(new_path);
        }
    }
    else { //incorrect input detection
        fprintf(stderr, "Error: Command not recognized\n");
    }

}

void MyHistory(char *args[], int arg_count){
  int elem = hist_count;
  if (elem > 20) {    //ensuring 20 elements
      elem = 20;
  }

  if (arg_count == 1) {   //print history list
	  printf("Current History:\n");
      for (int i=0; i<elem; i++)
          printf("%d: %s\n", i+1, MYHISTORY[i]);
  }
  else if (arg_count == 2) {    //clear history
      if (args[0][1] == 'c') {  //assign c as the arg to clear
          for(int i=0; i<elem; i++){
              if(MYHISTORY[i] != NULL){   //clear
                  free(MYHISTORY[i]);
                  MYHISTORY[i] = NULL;
              }
          }
          printf("History Cleared\n");
          hist_count = 0;
      }
      else {
          fprintf(stderr,"usage: 'history -c'\n");
      }
  }
  else if (arg_count == 3) {    //to execute one of the commands in the history list
      if (args[0][1] == 'e') {  // assign e as the arg to execute
          //Execute Element
          int i = atoi(args[1]);
          if (i > 20 || i < 1) {  //error case: outside of range
              fprintf(stderr,"Choose a number between 1 and 20\n");
              return;
          }
          char *temp = strdup(MYHISTORY[i-1]);
          temp = strtok(temp, " ");
          ExecuteCommands(temp, MYHISTORY[i-1]);
          //free temp
          free(temp);

      }
      else {
          fprintf(stderr,"usage: 'history -e <myhistory_number>'\n");
      }
  }
  else {
      fprintf(stderr,"usage: 'history [arg]'\n");
      fprintf(stderr,"usage: 'history -c'\n");
      fprintf(stderr,"usage: 'history -e <myhistory_number>'\n");
  }
}

void PipeCommands(char *args[], char *first_command, int arg_count){
    char *args1[MAX]; //args for first pipe command
    char *args2[MAX]; //args for second pipe command
    char *args3[MAX]; //args for third pipe command
    int i = 0; //iterates through args[]
    int j = 1; //iterates through args1[], args2[], and args[3]
    int status;
    int pid;

    //command parsing
    args1[0] = strdup(first_command);
    while(strchr(args[i], '|') == NULL) {
        args1[j] = strdup(args[i]);
        i++;
        j++;
    }
    j = 1;
    i++;
    if(i < arg_count-1) { //obtains second command
        args2[0] = strdup(args[i]);
    }
    i++;
    while(i < arg_count-1 && strchr(args[i], '|') == NULL) { //obtains arguments for second command, if any
        args2[j] = strdup(args[i]);
        i++;
        j++;
    }
    j = 1;
    i++;
    if(i < arg_count-1) { //obtains third command, if any
        args3[0] = strdup(args[i]);
    }
    i++;
    while(i < arg_count-1) { //obtains arguments for third command, if any
        args3[j] = strdup(args[i]);
        i++;
        j++;
    }

    //one pipe execution
    if(args3[0] == NULL) {
        int fd1[2]; //file descriptors for pipe
        pipe(fd1); //pipe

        if((pid = fork()) == -1) { //error checking
            fprintf(stderr, "Error: Fork unsuccessful\n");
            exit(1);
        }
        if(pid == 0) { //first child
            dup2(fd1[1], 1); //output to pipe

            //close pipes
            close(fd1[0]);
            close(fd1[1]);

            execvp(*args1, args1);

            //error checking
            fprintf(stderr,"%s: command not found\n", *args1);
            exit(1);
        }
        else { //parent
            if((pid = fork()) == -1) { //error checking
                fprintf(stderr, "Error: Fork unsuccessful\n");
                exit(1);
            }
            if(pid == 0) { //second child
                dup2(fd1[0], 0); //input from pipe

                //close pipe
                close(fd1[0]);
                close(fd1[1]);

                execvp(*args2, args2);

                //error checking
                fprintf(stderr,"%s: command not found\n", *args2);
                exit(1);
            }
        }

        //close pipe
        close(fd1[0]);
        close(fd1[1]);

        //wait for children to finish
        for(int k=0; k<2; k++)
            wait(&status);
    }

    //two pipe execution
    else {
        int fd1[2], fd2[2]; //file descriptors for two pipes
        pipe(fd1); //pipe 1
        pipe(fd2); //pipe 2

        if((pid = fork()) == -1) { //error checking
            fprintf(stderr, "Error: Fork unsuccessful\n");
            exit(1);
        }
        if(pid == 0) { //first child
            dup2(fd1[1], 1); //output to pipe 1

            //close pipes
            close(fd1[0]);
            close(fd1[1]);
            close(fd2[0]);
            close(fd2[1]);

            execvp(*args1, args1);

            //error checking
            fprintf(stderr,"%s: command not found\n", *args1);
            exit(1);
        }
        else { //parent
            if((pid = fork()) == -1) { //error checking
                fprintf(stderr, "Error: Fork unsuccessful\n");
                exit(1);
            }
            if(pid == 0) { //second child
                dup2(fd1[0], 0); //input from pipe 1
                dup2(fd2[1], 1); //output to pipe 2

                //close pipes
                close(fd1[0]);
                close(fd1[1]);
                close(fd2[0]);
                close(fd2[1]);

                execvp(*args2, args2);

                //error checking
                fprintf(stderr,"%s: command not found\n", *args2);
                exit(1);
            }
            else {
                if((pid = fork()) == -1) { //error checking
                    fprintf(stderr, "Error: Fork unsuccessful\n");
                    exit(1);
                }
                if(pid == 0) { //third child
                    dup2(fd2[0], 0); //input from pipe 2

                    //close pipes
                    close(fd1[0]);
                    close(fd1[1]);
                    close(fd2[0]);
                    close(fd2[1]);

                    execvp(*args3, args3);

                    //error checking
                    fprintf(stderr,"%s: command not found\n", *args3);
                    exit(1);
                }
            }
        }

        //close pipes
        close(fd1[0]);
        close(fd1[1]);
        close(fd2[0]);
        close(fd2[1]);

        //wait for children to finish
        for(int k=0; k<3; k++)
            wait(&status);
    }

    //cleanup
    j = 0;
    while(args1[j] != NULL) {
        free(args1[j]);
        args1[j] = NULL;
    }
    j = 0;
    while(args2[j] != NULL) {
        free(args2[j]);
        args2[j] = NULL;
    }
    j = 0;
    while(args3[j] != NULL) {
        free(args3[j]);
        args3[j] = NULL;
    }
}

void signalHandle(int sig){
  pid_t cpid = getpid(); //creates a child process

	if (cpid != ppid) {
    signal(sig, SIG_DFL); //Sets the signal to default
    kill(cpid, sig); //kill the child process based on what signal it handles
	}
}

void MyAlias(char *args[], int arg_count) {
	if (arg_count == 1) {		//alias list
			fprintf(stderr,"Current Aliases:\n");
			for (int i=0; i<alias_count; i++)
					printf("alias %s\n", MYALIAS[i]);
	}
	else if (arg_count == 2) {		//clear list
			if (args[0][1] == 'c') {
					for(int i=0; i<alias_count; i++){
							if(MYALIAS[i] != NULL){
									free(MYALIAS[i]);
									MYALIAS[i] = NULL;
							}
							alias_count = 0;
					}
					printf("All Aliases Cleared\n");
			}
			else {
					fprintf(stderr,"usage: 'alias -c'\n");
			}
	}
	else if (arg_count == 3) {			//remove specific alias
			if (args[0][1] == 'r') {

		char *delAlias = args[1];
		char* token;

		if (alias_count == 0) {			
			printf("There is no aliases on record\n");
			return;
		}

		for (int i=0; i<alias_count; i++) {
			char* temp;
			temp = strdup(MYALIAS[i]);
			token = strtok(temp,"=");

			if (strcmp(token, delAlias) == 0) {
				printf("Alias %s was removed\n", MYALIAS[i]);
				free(temp);
				free(MYALIAS[i]);
				for (int j=i; j<alias_count; j++) {
					MYALIAS[j] = MYALIAS[j+1];
				}
				alias_count--;
				return;
			}
			else {
				free(temp);
			}
		}
		printf("No match found\n");
			}
			else {
					fprintf(stderr,"usage: 'alias -r <alias_name>'\n");
			}
	}
	else {
			fprintf(stderr,"usage: 'alias [args]'\n");
			fprintf(stderr,"usage: 'alias -c'\n");
			fprintf(stderr,"usage: 'alias -r'\n");
			fprintf(stderr,"usage: 'alias <alias_name>='<command>''\n");
	}
}

void io_redirect(char *command, char* full_line) {//TO BE ADDED
	char *args[MAX];
	char *filename;

	char *backup_line = strdup(full_line);

	//parse full_line command by whitespace using ParseArgs
	int arg_count = ParseArgs(full_line, args);
	char *command_args[arg_count-2];

	strcpy(full_line, backup_line);
    free(backup_line);

	//create boolean condition variables for output_redirection and input redirection
	int output_redirect = 0;
	int input_redirect = 0;

	//copy values from args array to command_args, except the < > character and the fielname
	// for example, if args={"ps", "aux", ">", "out"}, command_args={"ps", "aux"}
	if (command[0] == '>' || command[0] == '<') {
		fprintf(stderr,"Needs a command\n");
		return;
	}
	else if (args[arg_count-2][0] == '>' || args[arg_count-2][0] == '<') {
		fprintf(stderr,"Needs a file\n");
		return;
	}

	command_args[0] = command;
	if (strchr(full_line, '>') != NULL) {
		output_redirect = 1;
		int i = 0;
		while (strcmp(args[i],">")) {
			command_args[i+1] = args[i];
			i++;
		}
	}
	else if (strchr(full_line, '<') != NULL) {
		input_redirect = 1;
		int i = 0;
		while (strcmp(args[i],"<")) {
			command_args[i+1] = args[i];
			i++;
		}
	}
	else {
		fprintf(stderr, "Failed... Returning To Interactive\n");
		return;
	}
	command_args[arg_count-2] = '\0';

	//start working here
	pid_t id = fork();
	if (id==0) {
		//if >
      	if (output_redirect) {
			//get file_name
			filename = args[arg_count-2];
			//to pipe
            int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd < 0) {
                printf("Couldn't run\n");
				exit(0);
        	}
			//creating a pipe
            dup2(fd, 1);
            close(fd);
            execvp(command_args[0], command_args);
			fprintf(stderr,"%s: command not found\n",args[0]);
			exit(0);
        }
		else if (input_redirect) {
			// do similarly to output_redirect
			filename = args[arg_count-2];
			//to pipe
            int fd = open(filename, O_RDONLY);
            if (fd < 0) {
                printf("Couldn't run\n");
				exit(0);
        	}
			//creating a pipe
            dup2(fd, 0);
            close(fd);
            execvp(command_args[0], command_args);
			fprintf(stderr,"%s: command not found\n",args[0]);
			exit(0);
		}
		else {
			fprintf(stderr,"Could not redirect\n");
			exit(0);
		}
	}
	else if (id > 0) {
		wait(NULL);
	}
	else {
		// print error
		fprintf(stderr,"Fork Error\n");
	}
}
