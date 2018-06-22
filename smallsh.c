#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>

//Trim Last Character from a string by replaceing with \0
void trimLastChar(char * string){
	int length = strlen(string);
	string[length] = '\0';
}

//Gets input from user and save via getline.  Returns Char* of answer
char* getInput(){ 
	char* lineEntered = NULL;
	size_t bufferSize = 0;
	int connectionCounter = 0;
	int lineCounter=0;
	getline(&lineEntered, &bufferSize, stdin);
	return lineEntered;
}

//Loops over array of background processes and checks if they are done
//maximum number of background processes is 100 but indexes can be reused
void checkBackgroundProcesses(int array[]){
	int i = 0;
	for(i=0; i<100; i++){
		//0 is the value of an empty index so dont wait on 0 values
		if(array[i] != 0){
			//wait with no hang to check if done or move on
			int childExitMethod = -5;
			int temp = waitpid(array[i],&childExitMethod,WNOHANG);
			if( temp != 0) 
			{
				//if process ended with error, notify
				if(temp == -1){
					printf("error in background process\n");
					fflush(stdout);
				}
				printf("background pid %d is done: ",temp);
				if(childExitMethod > 1){
					printf("terminated by signal %d\n",childExitMethod);			
					fflush(stdout);
				}else{
					printf("exit value %d\n",childExitMethod);
					fflush(stdout);
				}
				array[i]= 0;
			}
			i++;
		}
	}		
}

//global variables
//displayChange used for writing to console foreground-only mode change
int displayChange = 0;
//mode used for tracking whether in foreground only mode
int mode = 1;

//Toggles mode value for foreground-only and sets displayChange to notify user
void printMode(int signo){
	mode = (mode + 1) % 2;
	displayChange = 1;
}


int main() {

	//SIGINT used to ignore SIGINT Signal
	struct sigaction SIGINT_action = {0};
	SIGINT_action.sa_handler = SIG_IGN;
	sigaction(SIGINT, &SIGINT_action, NULL);

	//SIGTSTP used to handle SIGTSTP Signame 
	struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = printMode;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = SA_RESTART; 
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);
	
	//Get and set parent process id 
	int curPID = getpid();
	char currentPID[50];
	sprintf(currentPID, "%d", curPID);
	
	//used for recording last exit status or exit signal of processes
	int execStatus = 0;
	
	//set default variable used for getting input 
	char s[2] = " ";
	char* temp = "";	
	char inputBuffer [2048];
		
	//variables for forking child processes
	int childExitStatus = -5;
	int childExitMethod = -5;
	pid_t spawnPid = -5;

	//set HOME envrionment value  
	char HOME [200];
	strcpy(HOME, getenv("HOME"));

	//pidArray used to store ids of background processes
	int pidArray [100] = {0};

	//used to count how many whitespace delimited arguments user enters
	int copyCounter = 0;
	
	//array of char* pointers to store each whitespace delimited user input 
	char* inputArgsArray[512];
	
	while(1){
		//if foreground-only has been toggled, notify user
		if(displayChange == 1){
			if( mode == 1){
				printf("\nEntering foreground-only mode (& is now ignored)\n");
				fflush(stdout);
			}else{
				printf("\nExiting foreground-only mode\n");
				fflush(stdout);
			}
			displayChange = 0;
		}
		
		//Check if any background processes have completed
		checkBackgroundProcesses(pidArray);	
		
		//Print : prompt
		printf(":");
		fflush(stdout);

		//Free memory for inputArgs Array so no leaks 
		int i = 0;
		for( i=0; i<copyCounter; i++){
			free(inputArgsArray[i]);
		}		
		
		//Get User Input
		temp = getInput();
	
		//Tokenize input by " " and save to input args array 	
		char* token;
		token = strtok(temp, " ");
		copyCounter = 0;

		//Tokenization loop storing values into inputArgsArray
		while(token != NULL){
			inputArgsArray[copyCounter] = malloc(sizeof(char) * 1024);
			strcpy( inputArgsArray[copyCounter], token);		
	
			token = strtok(NULL," ");
			copyCounter++;	
		}
				
		//Remove newline from last element otherwise parsing errors occur
		int stringLength = strlen(inputArgsArray[copyCounter -1]);
		inputArgsArray[copyCounter - 1][stringLength-1] = '\0'; 
		inputArgsArray[copyCounter] = NULL;
	       	
		//Convert $$ into Parent PID in each char* of inputArgsArray
		char* occurIndex;
		int ii = 0;
		//loop for each value in inputArgsArray
		while(ii<copyCounter){
		
			//char address of first instance of $$ pattern 
			occurIndex = strstr(inputArgsArray[ii],"$$");

			//keep looping in case there are multiple $$ in one argument i.e. aaa$$bbb$$
			while(occurIndex != NULL){

				//buffer will hold the first half of string before $$ instance
				char firstBuffer [100];
			
				//index value of where $$ occurs
				int indexChange = occurIndex - inputArgsArray[ii];
				
				//copy string up to $$ into firstBuffer
			        strncpy(firstBuffer, inputArgsArray[ii],indexChange);
							
				//copy value of parent pid and second half of string to buffer
				sprintf(firstBuffer + indexChange, "%s%s", currentPID, occurIndex + strlen("$$"));

				//replace original string with string containing PID
				strcpy(inputArgsArray[ii], firstBuffer);		
				
				//check if there is another instance of $$
				occurIndex = strstr(inputArgsArray[ii],"$$");
			}
			ii++;
		}
 
		//If user enters exit, exit program 
		if( (strcmp(inputArgsArray[0], "exit")) == 0){		
			int i = 0;
			
			//kill all child processes
			for(i=0; i<100; i++){
				if(pidArray[i] != 0){
					kill(pidArray[i],SIGTERM);
					wait(pidArray[i]);
				}
			}
			int f = 0;
			for( f=0; f<copyCounter; f++){
				free(inputArgsArray[f]);
			}		
			//exit parent 
			inputArgsArray[0] = NULL;
			exit(0);
		}
		
		//if user enters cd the change directory
		else if(strcmp(inputArgsArray[0], "cd")==0){
			
			//jump to HOME if no input else jump to user input
			if( inputArgsArray[1] == NULL){
				chdir(HOME);
			}else{
				chdir(inputArgsArray[1]);
			}
		}

		//if user enters status, return last process return value 
		else if(strcmp(inputArgsArray[0], "status")==0){
			if(execStatus > 1){
				printf("terminated by signal %d\n",execStatus );
				fflush(stdout);
			}else{
				printf("exit value %d\n",execStatus);
				fflush(stdout);
			}
		}
		//if users enters string starting with # or doesnt do anything, do nothing and repromp
		else if(inputArgsArray[0][0]=='#' || inputArgsArray[0][0] == '\0'){
		
		}
		//Create a child process
		else{
			spawnPid = fork();

			//If we are inside parent process and foregroun-only is off
			//Prints the background id and saves process to background pid array to track
			if( spawnPid != 0 && ( strcmp(inputArgsArray[copyCounter-1],"&") == 0 ) && mode == 1){
				printf(" background pid is %d\n",spawnPid);
				fflush(stdout);
				int i = 0;
				for(i = 0; i < 100; i++){
					if(pidArray[i] == 0){
						pidArray[i] = spawnPid;
						i = 200;
					}
				}
			}

			//If we are in a child process
			if( spawnPid == 0){
			
				int fileOut = 0;
				int fileIn = 0;
				int i = 0;
				int portCounter = 3;
				
				for(i=0; i<copyCounter; i++){
					//if user inputs > or < then open fileOut for writing or fileIn for reading
					if(strcmp(inputArgsArray[i],">") == 0 || strcmp(inputArgsArray[i],"<") == 0){ 
						if( strcmp(inputArgsArray[i],">") == 0){
							//open fileOut for writing
							fileOut = open(inputArgsArray[i+1], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR );
							
							//change stdout to file 
							if( dup2(portCounter,1) == -1){
								printf("cannot open %s for output\n",inputArgsArray[i+1]);
								fflush(stdout);
								exit(1);
							}
							portCounter++;
						}else{
							//Open fileIn for reading
							fileIn = open(inputArgsArray[i+1], O_RDONLY);	
							//change stdin to stdout
							if( dup2(portCounter,0) == -1){
								printf("cannot open %s for output\n",inputArgsArray[i+1]);
								fflush(stdout);
								exit(1);
							}
							portCounter++;
						}	
						inputArgsArray[i] = NULL;
					}
				}
		
				//If user wants background process and foreground-only is toggled off
				if(strcmp(inputArgsArray[copyCounter-1],"&") == 0 && mode == 1){

					//change last value & to \0 because & messes up parsing 
					inputArgsArray[copyCounter-1] = '\0';
					
					//set output and input to null if they have not been set
					if(fileOut == 0){
						fileOut = open("/dev/null", O_RDWR | O_CREAT , S_IRUSR | S_IWUSR);
						dup2(portCounter,1);
						portCounter++;
					}
					if(fileIn == 0){
						fileIn = open("/dev/null", O_RDWR | O_CREAT , S_IRUSR | S_IWUSR);
						dup2(portCounter,1);
						portCounter++;
					}
				
					//ignore SigInt Signal
					struct sigaction SIGINT_background = {0};
					SIGINT_background.sa_handler = SIG_IGN;
					sigaction(SIGINT, &SIGINT_background, NULL);

					//ignore SigTSTP Signal
					struct sigaction SIGTSTP_action = {0};
					SIGTSTP_action.sa_handler = SIG_IGN;
					sigaction(SIGTSTP, &SIGTSTP_action, NULL);
					
					//execute and if execute fails, exit with error value
					if( execvp(inputArgsArray[0],inputArgsArray) == -1){
						exit(1);
					}
					exit(1);
				
				//If not background then must be foreground process											
				}else{
					//If foreground-only is toggled on and user enters &, trim it off
					if(strcmp(inputArgsArray[copyCounter-1],"&") == 0){
						inputArgsArray[copyCounter-1] = '\0';
					}
				
					//ignore SigInt Signal
					struct sigaction SIGTSTP_action = {0};
					SIGTSTP_action.sa_handler = SIG_IGN;
					sigaction(SIGTSTP, &SIGTSTP_action, NULL);
					
					//Handle SigInt in the default way
					struct sigaction SIGINT_foreground = {0};
					SIGINT_foreground.sa_handler = SIG_DFL;
					sigaction(SIGINT, &SIGINT_foreground, NULL);

					//execute and if execute fails, exit with an error
					if( execvp(inputArgsArray[0],inputArgsArray) == -1){
						printf("%s: no such file or command exists.\n",inputArgsArray[0]);
						fflush(stdout);
						exit(1);
					}		
					exit(1);	
				}
			}

			//If foreground-only toggled or foreground process, wait for process to end
			fflush(stdout);
			if( strcmp(inputArgsArray[copyCounter-1],"&") != 0 || mode == 0){
				waitpid(spawnPid, &childExitMethod, 0);
				
				//If ended by signal save the signal as the exit status
				if (WIFSIGNALED(childExitMethod) != 0){
					printf("\nThe process was terminated by signal %d\n",childExitMethod );
					fflush(stdout);
						
					execStatus = childExitMethod;
					
				}else if(childExitMethod == 0 ){
					execStatus = 0;
				}else{
					execStatus = 1;
				}	
			}
		}	
	}
	return 0;
}
