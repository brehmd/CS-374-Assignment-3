#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
// char* var_expand(char*); //3. Provide expansion for the variable $$
// void built_in_exit(); //4. Execute 3 commands exit, cd, and status via code built into the shell
// void build_in_cd(); //4. Execute 3 commands exit, cd, and status via code built into the shell
// void build_in_status(); //4. Execute 3 commands exit, cd, and status via code built into the shell
// void other_commands(struct CommandLine*); //5. Execute other commands by creating new processes using a function from the exec family of functions | 7. Support running commands in foreground and background processes
// void change_input(struct CommandLine*); //6. Support input and output redirection
// void change_output(struct CommandLine*); //6. Support input and output redirection
// static void sigHandler(int); //8. Implement custom handlers for 2 signals, SIGINT and SIGTSTP
void print_cl(struct CommandLine*);
void free_cl(struct CommandLine*);


int main () {
    struct CommandLine* cl = create_cl();

    print_cl(cl);

    free_cl(cl);

    return 0;
}


void print_cl(struct CommandLine* cl){
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
    struct CommandLine* cl = malloc(sizeof(struct CommandLine));

    char user_entry[2048];
    printf("\n: ");
    fgets(user_entry, sizeof(user_entry), stdin);
    user_entry[strcspn(user_entry, "\n")] = 0;

    if (strcmp(user_entry, "") == 0 || user_entry[0] == '#'){
        return NULL;
    }

    char* token = strtok(user_entry, " ");

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

    return cl;
}