#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

static volatile sig_atomic_t LAST_EXIT_STATUS = 0;
static volatile sig_atomic_t FG_MODE = 0;

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
static void parent_sigHandler(int); //8. Implement custom handlers for 2 signals, SIGINT and SIGTSTP
static void fg_sigHandler(int); //8. Implement custom handlers for 2 signals, SIGINT and SIGTSTP
void print_cl(struct CommandLine*);
void free_cl(struct CommandLine*);


// handles all function calls and the loop of execution
int main () {

    // set up signal handlers
    struct sigaction sa;
    sa.sa_handler = SIG_IGN; // Ignore the signal
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    sigset_t mask;
    struct sigaction act = {0};

    if (sigfillset(&mask) == -1) {
        printf("Setting mask failed\n");
        fflush(stdout);
        return 0;
    }

    act.sa_handler = &parent_sigHandler;
    act.sa_mask = mask;

    if (sigaction(SIGTSTP, &act, NULL) == -1) {
        printf("SIGTSTP handler registartion failed\n");
        fflush(stdout);
        return EXIT_FAILURE;
    }

    // initialize bg_array
    struct BackgroundArray* bga = create_bga();

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
                fflush(stdout);
                rm_pid(bga, bga->pids[i]);
            }
        }

        struct CommandLine* cl = create_cl();
        // print_cl(cl);

        if (cl == NULL){
            continue;
        }

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


// creates the array of background processes 
struct BackgroundArray* create_bga(){
    struct BackgroundArray* bga = malloc(sizeof(struct BackgroundArray));
    bga->pids = calloc(3, sizeof(pid_t));
    bga->capacity = 3;
    bga->size = 0;
    return bga;
}


// adds a value to the bga and dynamically allocates more memory if needed
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

// removes a value from the bga
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

// frees all memory from the bga
void free_bga(struct BackgroundArray* bga){
    free(bga->pids);
    bga->pids = NULL;
    free(bga);
    bga = NULL;
    return;
}

// used while debugging my code to print everything captured in the cl struct
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

// frees all memory allocated in the cl struct
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

// Prompts the user for a command, parses it, and returns a CommandLine structure
struct CommandLine* create_cl(){
    char user_entry[2048];
    printf(": ");
    fflush(stdout);
    fgets(user_entry, sizeof(user_entry), stdin);
    user_entry[strcspn(user_entry, "\n")] = 0;

    if (strcmp(user_entry, "") == 0 || user_entry[0] == '#'){
        return NULL;
    }

    char* p_entry = user_entry;

    // expands the cl string $$ to the current PID
    var_expand(&p_entry);

    struct CommandLine* cl = malloc(sizeof(struct CommandLine));

    // first segment is the command string
    char* token = strtok(p_entry, " ");

    cl->command = calloc(strlen(token) +1, sizeof(char));
    strcpy(cl->command, token);

    // next segment are the arguments
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

    // next segment is the file input
    if (token != NULL && *token == '<'){
        token = strtok(NULL, " ");
        cl->input_file = calloc(strlen(token) + 1, sizeof(char));
        strcpy(cl->input_file, token);
        cl->is_input = 1;
        token = strtok(NULL, " ");
    }
    else{cl->input_file = NULL; cl->is_input = 0;}

    // next segment is the file output
    if (token != NULL && *token == '>'){
        token = strtok(NULL, " ");
        cl->output_file = calloc(strlen(token) + 1, sizeof(char));
        strcpy(cl->output_file, token);
        cl->is_output = 1;
        token = strtok(NULL, " ");
    }
    else{cl->output_file = NULL; cl->is_output = 0;}

    // last grab if process is to be in background or not
    if (token != NULL && *token == '&'){
        cl->is_background = 1;
    }
    else{cl->is_background = 0;}

    if (strcmp(p_entry, user_entry)){
        free(p_entry);
    }

    return cl;
}

// expands every instance of $$ to the current PID
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

// exits the program, but first kills all alive children processes
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

// changes the current directory
void built_in_cd(struct CommandLine* cl){
    if (cl->num_args){
        if (chdir(cl->arguments[0]) == -1){
            printf("ERROR - %s is not a valid path name\n", cl->arguments[0]);
            fflush(stdout);
        }
    }
    else{
        if (chdir(getenv("HOME")) == -1){
            printf("ERROR - cannot cd HOME\n");
            fflush(stdout);
        }
    }
    // char cwd[1024];
    // getcwd(cwd, sizeof(cwd));
    // printf("Check cwd: %s\n", cwd);
}

// prints the last status of an exited process
void built_in_status(int status){
    if (status == 2){
        printf("terminated by signal 2\n");
        fflush(stdout);
    }
    else{
        printf("exit value 1\n");
        fflush(stdout);
    }
}

// Executes non-built-in commands, managing foreground and background processes
void other_commands(struct CommandLine* cl, struct BackgroundArray* bga){
    // new arguments list to be passed into execvp
    char** newargs = (char**)calloc(cl->num_args + 2, sizeof(char*));

    newargs[0] = (char*)malloc((strlen(cl->command) + 1) * sizeof(char));
    strcpy(newargs[0], cl->command);
    for(int i = 1; i <= cl->num_args; i++){
        newargs[i] = (char*)malloc((strlen(cl->arguments[i-1]) + 1) * sizeof(char));
        strcpy(newargs[i], cl->arguments[i-1]);
    }
    newargs[cl->num_args + 1] = NULL;

    // fork()
    int childStatus;

    pid_t spawnPid = fork();

    // manage parent and child processes 
    switch(spawnPid){
        case -1:
            perror("failure to fork()\n");
            LAST_EXIT_STATUS = 1;
            exit(1);
            break;

        case 0:
            ;
            // this ignore does not work
            struct sigaction sa = {0};
            sa.sa_handler = SIG_IGN; // Ignore the signal
            sa.sa_flags = SA_RESTART;

            if (sigaction(SIGTSTP, &sa, NULL) == -1) {
                perror("sigaction");
                exit(1);
            }

            struct sigaction sa2 = {0};
            sigset_t mask;

            // if background set io to /dev/null and ignore interrupts
            if(cl->is_background && FG_MODE == 0){
                change_io("/dev/null", "/dev/null");
            }
            else{ // if foreground allow ^C
                if (sigfillset(&mask) == -1) {
                    printf("Setting mask failed\n");
                    fflush(stdout);
                    exit(1);
                }

                sa2.sa_handler = &fg_sigHandler;
                sa2.sa_mask = mask;

                if (sigaction(SIGINT, &sa2, NULL) == -1) {
                    printf("SIGINT handler registartion failed\n");
                    fflush(stdout);
                    exit(1);
                }
            }
            
            int io_status = change_io(cl->input_file, cl->output_file); // if null then change_io ignores
            
            if (io_status == 0){
                // execvp()
                execvp(cl->command, newargs);
                perror("failure to execvp()");
            }

            // free memory on failure
            for(int i = 0; i <= cl->num_args + 1; i++){ 
                free(newargs[i]);
            }
            free(newargs);
            free_cl(cl);
            exit(1);
            break;

        default:
            // free argument memory
            for(int i = 0; i <= cl->num_args + 1; i++){
                free(newargs[i]);
            }
            free(newargs);

            // if background print pid and parent return to prompt without waiting
            // add pid to bg_array
            if(cl->is_background && FG_MODE == 0){
                printf("background pid is %d\n", spawnPid);
                fflush(stdout);
                add_pid(bga, spawnPid);
                return;
            }

            spawnPid = waitpid(spawnPid, &childStatus, 0);

            // if foreground update last_exit_status
            if (childStatus == 2){built_in_status(2);}
            LAST_EXIT_STATUS = childStatus;
            break;
    }
}

// changes io direction if strings are not NULL
int change_io(char* input_file, char* output_file){
    if(input_file != NULL){
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

// handles SIGTSTP in the parent function
static void parent_sigHandler(int sig){
    if (sig == SIGTSTP){

        if (FG_MODE == 0){
            const char* message = "\nEntering foreground-only mode (& is now ignored)\n";
            write(STDOUT_FILENO, message, 50);
            fflush(stdout);
            FG_MODE = 1;
        }
        else if (FG_MODE == 1){
            const char* message = "\nExiting foreground-only mode\n";
            write(STDOUT_FILENO, message, 30);
            fflush(stdout);
            FG_MODE = 0;
        }
    }
}

// Handles SIGINT in the child process and passes SIGINT as exit
static void fg_sigHandler(int sig){
    if (sig == SIGINT){
        exit(SIGINT);
    }
}