#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>	
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

struct command {
    char *args[513]; // Max 512 arguments + one command
    char *in_file;
    char *out_file;
    int bg; // background or foreground with 0 being foreground
} *command;

int fg_only = 0;    // 1 means we're in foreground only mode 
int status = 0; // Use this to get the exit status

void foreground_mode() {    // Signal control doesn't like returning int so had to set to void
    if (fg_only == 0) {
        char* message = "\nEntering foreground only mode\n: ";
        write(STDOUT_FILENO, message, 34);
        fflush(stdout);
        fg_only = 1;
    }
    else {
        char* message = "\nExiting foreground only mode\n: ";
        write(STDOUT_FILENO, message, 33);
        fflush(stdout);
        fg_only = 0;
    }

    //return 0;
}


const char *expansion(char *argument, int pid) {
    char *var_expansion;
    char *temp;
    //int expanded = 0;

    var_expansion = strdup(argument); 
    //expanded = 0;
    for (int i = 0; i < strlen(var_expansion); i++) {   // Iterate through string to be expanded
        if (var_expansion[i] == '$' && var_expansion[i+1] == '$') { // Check for back to back $
            //expanded = 1;
            temp = strdup(var_expansion);
            temp[i] = '%';  // Turns the $$ to %d
            temp[i+1] = 'd';    // Thanks to my roomate for telling me this was a thing I could do!
            sprintf(var_expansion, temp, pid);  // Saves expanded string to var_expansion variable
        }
    }

    return var_expansion;
}


int get_command(int pid, char input[2048]) {
    char *context = NULL; // used so strtok_r saves its state
    int counter = 0;    // Used so the argument array in the command struct doesn't get overwritten when there's multiple arguments
    command = malloc(sizeof(struct command));

    command->bg = 0;    // Sets background flag for the current command to 0

    //input[strlen(input)-1]='\0';

    if (input[0] != '\n') {
        input[strcspn(input, "\n")] = 0;    // Terminal symbol in fgets is \n so this gets rid of the newline
    }

    char *token = strtok_r(input, " ", &context);
    char token_list[513][513];

    while (token != NULL) { // Splits input by spaces
        if (strcmp(token, "<") == 0) {
            token = strtok_r(NULL, " ", &context);
            command->in_file = strdup(token);
            if (strstr(token, "$$")) {
                command->in_file = strdup(expansion(token, pid));   // Saves expanded input file name to struct
            }
            //printf("%s\n", command->in_file);
        }
        else if (strcmp(token, ">") == 0) {
            token = strtok_r(NULL, " ", &context);
            command->out_file = strdup(token);
            if (strstr(token, "$$")) {
                command->out_file = strdup(expansion(token, pid));  // Saves expanded output file name to struct
            }
            //printf("%s\n", command->out_file);
        }
        else if (strcmp(token, "&") == 0) {
            command->bg = 1;
        }
        else {
            command->args[counter] = strdup(token);
            if (strstr(token, "$$")) {
                command->args[counter] = strdup(expansion(token, pid)); // Saves expanded argument to struct
            }
            counter++;
        }
        token = strtok_r(NULL, " ", &context);
    };

    return 0;
}


int exit_status() {
    // Jungmin from class helped me with this function
    if (WIFEXITED(status)) {    // Gets exit value from the status global variable
        printf("Exit value %d\n", WEXITSTATUS(status));
        fflush(stdout);
    }
    else {  // Gets terminated signal from status global variable
        printf("Terminated by signal %d\n", WTERMSIG(status));
        fflush(stdout);
    }

    return 0;
}


int other_commands() {
    int input;
    int output;
    pid_t spawnPid = fork();

    struct sigaction sigint_action = {0};

    switch(spawnPid) {
        case -1:
            perror("fork() failed!");
            fflush(stdout);
            exit(1);
            break;
        case 0: // Child
            if (command->bg == 0) { // Set ctrl+c from ignore to default
                sigint_action.sa_handler = SIG_DFL;
                sigaction(SIGINT, &sigint_action, NULL);
            }
            
            //if (command->in_file[0] != '\0') {  
            //if (strcmp(command->in_file, "")) {
            if (command->in_file != NULL) { // Have to check if it's NULL otherwise it'll break and skip everything else
                input = open(command->in_file, O_RDONLY);
                if (input == -1) {
                    perror("Can't open file to read\n");
                    exit(1);
                }

                int dup_check = dup2(input, 0); // 0 reads from terminal
                if (dup_check == -1) {
                    perror("Input redirection error\n");
                    exit(1);
                }
                
                fcntl(input, F_SETFD, FD_CLOEXEC);  // Closes file
            }

            //if (command->out_file[0] != '\0') {
            //if (strcmp(command->out_file, "")) {
            if (command->out_file != NULL) {
                output = open(command->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
                if (output == -1) {
                    perror("Can't open file to write\n");
                    exit(1);
                }

                int dup_check = dup2(output, 1); // 1 writes from terminal
                if (dup_check == -1) {
                    perror("Output redirection error\n");
                    exit(1);
                }
                
                fcntl(output, F_SETFD, FD_CLOEXEC); // Closes file
            }

            execvp(command->args[0], command->args);    // Runs the command
            perror("execvp error\n"); // Only shows if there's an error with execvp
            exit(1);

            break;
        default:    // Parent   IGNORE SIGINT (CTRL+C)

            //waitpid(spawnPid, &status, 0);

            if (command->bg == 1 && fg_only == 0) { // Doesn't wait for child
                printf("background pid is %d\n", spawnPid);
                fflush(stdout);
                pid_t new_pid = waitpid(spawnPid, &status, WNOHANG);
            }
            else {  // Waits for the child
                pid_t new_pid = waitpid(spawnPid, &status, 0);
            }

            break;
        
        }
    
    pid_t child_pid;

    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {   // Checks for any finished child processes
        printf("background pid %d is done: ", child_pid);
        exit_status();  // Output from here will be written on the same line as the above print statement
        fflush(stdout);
    }

    return 0;
}


int main() {
    int main_loop = 1;
    char input[2048];

    int pid = getpid();

    // Signal control code is mostly from the exploration with a few changes for this assignment

    // SIGINT (CTRL+C)
    struct sigaction sigint_action = {0};
    sigint_action.sa_handler = SIG_IGN;
    sigfillset(&sigint_action.sa_mask);
    sigint_action.sa_flags = 0;
    sigaction(SIGINT, &sigint_action, NULL);

    // SIGTSTP (CTRL+Z)
    signal(SIGTSTP, foreground_mode);   // Taken from the discord

    while (main_loop == 1) {
        printf(": ");
        fflush(stdout);
        fgets(input, 2048, stdin);  // Get user input and save to input variable (2048 characters max)

        get_command(pid, input);

        if (strcmp(command->args[0], "cd") == 0) {
            if (command->args[1] == NULL) { // Default directory is home
                chdir(getenv("HOME"));
            }
            else {
                chdir(command->args[1]);
            }
        }

        else if (strcmp(command->args[0], "status") == 0) {
            exit_status();
        }

        else if (strcmp(command->args[0], "exit") == 0) {
            return 0;
        }

        else {
            if (command->args[0][0] == '#' || command->args[0][0] == '\n') {    // Returns empty input and comments
                continue;
            }
            other_commands();   // All non built in commmands go here
        }
        
        //free(command);
    }
    
    return 0;
}

