#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

int LAST_EXIT_STATUS = 0;
int FG_MODE = 0;

struct BackgroundArray{
    pid_t* pids;
    int size;
    int capacity;
};

struct CommandLine{
    char* command;

    int num_args;
    char* arguments[512];

    int is_input;
    char* input_file;

    int is_output;
    char* output_file;
    
    int is_background;
};

struct CommandLine* create_cl(); //1. Provide a prompt for running commands | 2. Handle blank lines and comments, which are lines beginning with the # character
struct BackgroundArray* create_bga();
void add_pid(struct BackgroundArray*, pid_t);
void rm_pid(struct BackgroundArray*, pid_t);
void free_bga(struct BackgroundArray*);
void var_expand(char**); //3. Provide expansion for the variable $$
void built_in_exit(struct BackgroundArray*); //4. Execute 3 commands exit, cd, and status via code built into the shell
void built_in_cd(struct CommandLine*); //4. Execute 3 commands exit, cd, and status via code built into the shell
void built_in_status(int); //4. Execute 3 commands exit, cd, and status via code built into the shell
void other_commands(struct CommandLine*, struct BackgroundArray*); //5. Execute other commands by creating new processes using a function from the exec family of functions | 7. Support running commands in foreground and background processes
int change_io(char*, char*); //6. Support input and output redirection
static void sigHandler(int); //8. Implement custom handlers for 2 signals, SIGINT and SIGTSTP
void print_cl(struct CommandLine*);
void free_cl(struct CommandLine*);

int main () {

    // set up signal handler

    // initialize bg_array
    struct BackgroundArray* bga = create_bga();

    // int last_exit_status = 0; // this should be a global variable
    int keep_running = 1;
    while(keep_running){


        // check array of all background PIDs
        // https://stackoverflow.com/questions/4200373/just-check-status-process-in-c
        for (int i = 0; i < bga->capacity; i++){
            if (bga->pids[i] == 0){
                continue;
            }

            int status;
            pid_t return_pid = waitpid(bga->pids[i], &status, WNOHANG); /* WNOHANG def'd in wait.h */
        
            if (return_pid == -1) {
                perror("Failure to check PID\n");
            }
            else if (return_pid == 0) {
                continue;
            }
            else if (return_pid == bga->pids[i]) {
                printf("background pid %d is done: exit value %d\n", bga->pids[i], status);
                rm_pid(bga, bga->pids[i]);
                
                // free memory of newargs
            }
        }

        struct CommandLine* cl = create_cl();
        // print_cl(cl);

        if (cl == NULL){
            continue;
        }

        // more code
        if (strcmp(cl->command, "exit") == 0){
            built_in_exit(bga);
            keep_running = 0;
        }
        else if (strcmp(cl->command, "cd") == 0){
            built_in_cd(cl);
        }
        else if (strcmp(cl->command, "status") == 0){
            built_in_status(LAST_EXIT_STATUS);
        }
        else{
            other_commands(cl, bga);
        }

        free_cl(cl);
    }

    free_bga(bga);

    return 0;
}

struct BackgroundArray* create_bga(){
    struct BackgroundArray* bga = malloc(sizeof(struct BackgroundArray));
    bga->pids = calloc(3, sizeof(pid_t));
    bga->capacity = 3;
    bga->size = 0;
    return bga;
}



void add_pid(struct BackgroundArray* bga, pid_t pid){
    if (bga->size == bga->capacity){
        bga->capacity *= 2;
        bga->pids = realloc(bga->pids, bga->capacity * sizeof(pid_t));
        for(int k = bga->size; k < bga->capacity; k++){
            bga->pids[k] = 0;
        }
    }
    for(int i = 0; i < bga->capacity; i++){
        if (bga->pids[i] == 0){
            bga->size++;
            bga->pids[i] = pid;
            return;
        }
    }
    return;
}

void rm_pid(struct BackgroundArray* bga, pid_t pid){
    for(int i = 0; i < bga->capacity; i++){
        if (bga->pids[i] == pid){
            bga->size--;
            bga->pids[i] = 0;
            return;
        }
    }
    return;
}

void free_bga(struct BackgroundArray* bga){
    free(bga->pids);
    bga->pids = NULL;
    free(bga);
    bga = NULL;
    return;
}


void print_cl(struct CommandLine* cl){
    if(cl == NULL){return;}

    printf("--------------------------------------\n");
    printf("cl->command: %s\n", cl->command);

    printf("cl->num_args: %d\n", cl->num_args);
    for (int i = 0; i < cl->num_args; i++){
        printf("   cl->arguments[%d]: %s\n", i, cl->arguments[i]);
    }
    
    printf("cl->is_input: %d\n", cl->is_input);
    printf("cl->input_file: %s\n", cl->input_file);

    printf("cl->is_output: %d\n", cl->is_output);
    printf("cl->output_file: %s\n", cl->output_file);

    printf("cl->is_background: %d\n", cl->is_background);
    printf("--------------------------------------\n");

    return;
}

void free_cl(struct CommandLine* cl){
    if (cl == NULL){
        return;
    }

    if (cl->command != NULL){
        free(cl->command);
        cl->command = NULL;
    }

    for (int i = 0; i < cl->num_args; i++){
        free(cl->arguments[i]);
        cl->arguments[i] = NULL;
    }
    cl->num_args = 0;
    
    if(cl->is_input){
        free(cl->input_file);
        cl->input_file = NULL;
    }
    cl->is_input = 0;

    if(cl->is_output){
        free(cl->output_file);
        cl->output_file = NULL;
    }
    cl->is_output = 0;

    free(cl);
    return;
}

struct CommandLine* create_cl(){
    char user_entry[2048];
    printf(": ");
    fgets(user_entry, sizeof(user_entry), stdin);
    user_entry[strcspn(user_entry, "\n")] = 0;

    if (strcmp(user_entry, "") == 0 || user_entry[0] == '#'){
        return NULL;
    }

    char* p_entry = user_entry;

    var_expand(&p_entry);
    // printf("p_entry: %s\n", p_entry);

    struct CommandLine* cl = malloc(sizeof(struct CommandLine));

    char* token = strtok(p_entry, " ");

    cl->command = calloc(strlen(token) +1, sizeof(char));
    strcpy(cl->command, token);

    int arg_num = 0;
    token = strtok(NULL, " ");
    while(token != NULL && *token != '<' && *token != '>' && *token != '&'){
        cl->arguments[arg_num] = calloc(strlen(token) +1, sizeof(char));
        strcpy(cl->arguments[arg_num], token);
        arg_num++;
        token = strtok(NULL, " ");
    }
    cl->num_args = arg_num;
    for (int i = arg_num; i < 512; i++){
        cl->arguments[i] = NULL;
    }

    if (token != NULL && *token == '<'){
        token = strtok(NULL, " ");
        cl->input_file = calloc(strlen(token) + 1, sizeof(char));
        strcpy(cl->input_file, token);
        cl->is_input = 1;
        token = strtok(NULL, " ");
    }
    else{cl->input_file = NULL; cl->is_input = 0;}

    if (token != NULL && *token == '>'){
        token = strtok(NULL, " ");
        cl->output_file = calloc(strlen(token) + 1, sizeof(char));
        strcpy(cl->output_file, token);
        cl->is_output = 1;
        token = strtok(NULL, " ");
    }
    else{cl->output_file = NULL; cl->is_output = 0;}

    if (token != NULL && *token == '&'){
        cl->is_background = 1;
    }
    else{cl->is_background = 0;}

    if (strcmp(p_entry, user_entry)){
        free(p_entry);
    }

    return cl;
}

void var_expand(char** string){
    const char *target = "$$";

    char proc_id[10];
    sprintf(proc_id, "%d", getpid());
    const char *replacement = proc_id;

    char *pos;
    int target_len = strlen(target);
    int replacement_len = strlen(replacement);
    
    // Count occurrences of target in str
    int count = 0;
    char *temp = *string;
    while ((temp = strstr(temp, target)) != NULL) {
        count++;
        temp += target_len; // Move past the last found occurrence
    }

    // If no occurrences found, return the original string
    if (count == 0) {
        return;
    }

    // Allocate memory for the new string
    char *result = malloc(strlen(*string) + count * (replacement_len - target_len) + 1);
    if (!result) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    char *current = result;
    while ((pos = strstr(*string, target)) != NULL) {
        // Copy the part before target
        int len = pos - *string;
        strncpy(current, *string, len);
        current += len;

        // Copy the replacement
        strcpy(current, replacement);
        current += replacement_len;

        // Move past the target in the original string
        *string = pos + target_len;
    }

    // Copy the remaining part of the original string
    strcpy(current, *string);

    // Replace the original string with the new string
    strcpy(*string, result);
    *string = result; // Update the original pointer
}


void built_in_exit(struct BackgroundArray* bga){
    // terminate all children
    for (int i = 0; i < bga->capacity; i++){
        if (bga->pids[i] == 0){
            continue;
        }
        kill(bga->pids[i], SIGKILL);
        bga->pids[i] = 0;
    }
    return;
}


void built_in_cd(struct CommandLine* cl){
    // printf("running built_in_cd\n");
    if (cl->num_args){
        if (chdir(cl->arguments[0]) == -1){
            printf("ERROR - %s is not a valid path name\n", cl->arguments[0]);
        }
    }
    else{
        if (chdir(getenv("HOME")) == -1){
            printf("ERROR - cannot cd HOME\n");
        }
    }
    // char cwd[1024];
    // getcwd(cwd, sizeof(cwd));
    // printf("Check cwd: %s\n", cwd);
    // printf("ran built_in_cd\n");
}


void built_in_status(int status){
    // printf("running built_in_status\n");

    // run some if statements to handle different status codes

    printf("exit value %d\n", status);
    // printf("ran built_in_status\n");
}


void other_commands(struct CommandLine* cl, struct BackgroundArray* bga){
    // printf("running other_commands\n");

    // check background and foreground

    // new arguments list
    char** newargs = (char**)calloc(cl->num_args + 2, sizeof(char*));

    newargs[0] = (char*)malloc((strlen(cl->command) + 1) * sizeof(char));
    strcpy(newargs[0], cl->command);
    for(int i = 1; i <= cl->num_args; i++){
        newargs[i] = (char*)malloc((strlen(cl->arguments[i-1]) + 1) * sizeof(char));
        strcpy(newargs[i], cl->arguments[i-1]);
    }
    newargs[cl->num_args + 1] = NULL;

    // for(int i = 0; i <= cl->num_args + 1; i++){
    //     printf("  newargs[%d] = %s\n", i, newargs[i]);
    // }

    // fork()
    int childStatus;

    pid_t spawnPid = fork();

    switch(spawnPid){
        case -1:
            perror("failure to fork()\n");
            LAST_EXIT_STATUS = 1;
            exit(1); // is exit valid here?
            break;

        case 0:
            // printf("Child Process %d is running\n", getpid());
            
            // if background set io to /dev/null
            if(cl->is_background){
                change_io("/dev/null", "/dev/null");
            }
            
            int io_status = change_io(cl->input_file, cl->output_file); // if null then change_io ignores
            
            if (io_status == 0){
                // execvp()
                execvp(cl->command, newargs);
                perror("failure to execvp()");
            }

            for(int i = 0; i <= cl->num_args + 1; i++){
                free(newargs[i]);
            }
            free(newargs);
            free_cl(cl);
            exit(1);
            break;

        default:
            // printf("Parent Process %d is running\n", getpid());

            // if background print pid and parent return to prompt without waiting
            // add pid to bg_array
            if(cl->is_background){
                printf("background pid is %d\n", spawnPid);
                add_pid(bga, spawnPid);
                return;
            }

            spawnPid = waitpid(spawnPid, &childStatus, 0);

            // printf("back to the parent process %d\n", getpid());

            // if foreground update last_exit_status
            if (childStatus == 0){
                LAST_EXIT_STATUS = 0;
            }
            else{
                LAST_EXIT_STATUS = 1;
            }
            
            for(int i = 0; i <= cl->num_args + 1; i++){
                free(newargs[i]);
            }
            free(newargs);
            break;
    }

    // printf("ran other_commands\n");
}

int change_io(char* input_file, char* output_file){ //currently does not free child newarg memory when exit() child process on error || solution: return an error int instead of exit()
    if(input_file != NULL){
        // run code
        int sourceFD = open(input_file, O_RDONLY);
        if (sourceFD == -1) {
            perror("source open()"); 
            return 1;
        }
        
        int result = dup2(sourceFD, 0);
        if (result == -1) { 
            perror("source dup2()"); 
            return 2;
        }
    }
    if(output_file != NULL){
        //run code
        int targetFD = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (targetFD == -1) { 
            perror("target open()"); 
            return 3;
        }

        int result = dup2(targetFD, 1);
        if (result == -1) { 
            perror("target dup2()"); 
            return 4;
        }
    }

    return 0;
}

static void sigHandler(int sig){
    if (sig == SIGINT){
        // interrupt FG process
        // display sigint interupt using write()
        // change LAST_EXIT_STATUS
        return;
    }

    else if (sig == SIGSTOP){
        // call wait()
        // set FG_MODE
        // display an informative message using write()
        // handle FG_MODE in function calls
        return;
    }
}