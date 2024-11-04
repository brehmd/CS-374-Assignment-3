#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
void var_expand(char**); //3. Provide expansion for the variable $$
// void built_in_exit(); //4. Execute 3 commands exit, cd, and status via code built into the shell
void built_in_cd(struct CommandLine*); //4. Execute 3 commands exit, cd, and status via code built into the shell
void built_in_status(int); //4. Execute 3 commands exit, cd, and status via code built into the shell
void other_commands(struct CommandLine*, int*); //5. Execute other commands by creating new processes using a function from the exec family of functions | 7. Support running commands in foreground and background processes
// void change_io(struct CommandLine*); //6. Support input and output redirection
// static void sigHandler(int); //8. Implement custom handlers for 2 signals, SIGINT and SIGTSTP
void print_cl(struct CommandLine*);
void free_cl(struct CommandLine*);


int main () {

    int last_exit_status = 0;
    int keep_running = 1;
    while(keep_running){
        struct CommandLine* cl = create_cl();
        // print_cl(cl);

        if (cl == NULL){
            continue;
        }

        // more code
        if (strcmp(cl->command, "exit") == 0){
            keep_running = 0;
        }
        else if (strcmp(cl->command, "cd") == 0){
            built_in_cd(cl);
        }
        else if (strcmp(cl->command, "status") == 0){
            built_in_status(last_exit_status);
        }
        else{
            other_commands(cl, &last_exit_status);
        }

        free_cl(cl);
    }
    
    

    return 0;
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
    printf("exit value %d\n", status);
    // printf("ran built_in_status\n");
}


void other_commands(struct CommandLine* cl, int* last_exit_status){
    printf("running other_commands\n");
    // code goes here
    printf("ran other_commands\n");
}

