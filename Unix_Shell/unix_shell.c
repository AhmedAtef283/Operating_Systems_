#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#define MAX_LINE 80 
#define DELIMITERS " \t\n\v\f\r"


void init_args(char *args[]) {
    for(size_t i = 0; i != MAX_LINE / 2 + 1; ++i) {
        args[i] = NULL;
    }
}

void first_command(char *command) {
    strcpy(command, "");
}


void refresh_args(char *args[]) {
    while(*args) {
        free(*args);  
        *args++ = NULL;
    }
}


size_t change_input(char *args[], char *original_command) {
    size_t num = 0;
    char command[MAX_LINE + 1];
    strcpy(command, original_command);  
    char *token = strtok(command, DELIMITERS);
    while(token != NULL) {
        args[num] = malloc(strlen(token) + 1);
        strcpy(args[num], token);
        ++num;
        token = strtok(NULL, DELIMITERS);
    }
    return num;
}


int get_input(char *command) {
    char input_buffer[MAX_LINE + 1];
    if(fgets(input_buffer, MAX_LINE + 1, stdin) == NULL) {
        fprintf(stderr, "Failed\n");
        return 0;
    }
    if(strncmp(input_buffer, "!!", 2) == 0) {
        if(strlen(command) == 0) {  
            fprintf(stderr, "No history available\n");
            return 0;
        }
        printf("%s", command);    
        return 1;
    }
    strcpy(command, input_buffer);  
    return 1;
}


int check_ampersand(char **args, size_t *size) {
    size_t len = strlen(args[*size - 1]);
    if(args[*size - 1][len - 1] != '&') {
        return 0;
    }
    if(len == 1) {  
        free(args[*size - 1]);
        args[*size - 1] = NULL;
        --(*size);  
    } else {
        args[*size - 1][len - 1] = '\0';
    }
    return 1;
}


unsigned check_redirection(char **args, size_t *size, char **input_file, char **output_file) {
    unsigned flag = 0;
    size_t to_remove[4], remove_cnt = 0;
    for(size_t i = 0; i != *size; ++i) {
        if(remove_cnt >= 4) {
            break;
        }
        if(strcmp("<", args[i]) == 0) {   
            to_remove[remove_cnt++] = i;
            if(i == (*size) - 1) {
                fprintf(stderr, "No input file provided!\n");
                break;
            }
            flag |= 1;
            *input_file = args[i + 1];
            to_remove[remove_cnt++] = ++i;
        } else if(strcmp(">", args[i]) == 0) {   
            to_remove[remove_cnt++] = i;
            if(i == (*size) - 1) {
                fprintf(stderr, "No output file provided!\n");
                break;
            }
            flag |= 2;
            *output_file = args[i + 1];
            to_remove[remove_cnt++] = ++i;
        }
    }
    for(int i = remove_cnt - 1; i >= 0; --i) {
        size_t pos = to_remove[i];  
        while(pos != *size) {
            args[pos] = args[pos + 1];
            ++pos;
        }
        --(*size);
    }
    return flag;
}


int redirection_io(unsigned io_flag, char *input_file, char *output_file, int *input_desc, int *output_desc) {
    if(io_flag & 2) {  
        *output_desc = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 644);
        if(*output_desc < 0) {
            fprintf(stderr, "Failed to open the output file: %s\n", output_file);
            return 0;
        }
        dup2(*output_desc, STDOUT_FILENO);
    }
    if(io_flag & 1) { 
        *input_desc = open(input_file, O_RDONLY, 0644);
        if(*input_desc < 0) {
            fprintf(stderr, "Failed to open the input file: %s\n", input_file);
            return 0;
        }
        dup2(*input_desc, STDIN_FILENO);
    }
    return 1;
}


void close_terminal(unsigned io_flag, int input_desc, int output_desc) {
    if(io_flag & 2) {
        close(output_desc);
    }
    if(io_flag & 1) {
        close(input_desc);
    }
}


void pipe_detect(char **args, size_t *args_num, char ***args2, size_t *args_num2) {
    for(size_t i = 0; i != *args_num; ++i) {
        if (strcmp(args[i], "|") == 0) {
            free(args[i]);
            args[i] = NULL;
            *args_num2 = *args_num -  i - 1;
            *args_num = i;
            *args2 = args + i + 1;
            break;
        }
    }
}


int run_command(char **args, size_t args_num) {
    int run_concurrently = check_ampersand(args, &args_num);
    char **args2;
    size_t args_num2 = 0;
    pipe_detect(args, &args_num, &args2, &args_num2);
    pid_t pid = fork();
    if(pid < 0) {   
        fprintf(stderr, "Failed to fork!\n");
        return 0;
    } else if (pid == 0) { 
        if(args_num2 != 0) {   
            int fd[2];
            pipe(fd);
            pid_t pid2 = fork();
            if(pid2 > 0) {  
                char *input_file, *output_file;
                int input_desc, output_desc;
                unsigned io_flag = check_redirection(args2, &args_num2, &input_file, &output_file);    
                io_flag &= 2;  
                if(redirection_io(io_flag, input_file, output_file, &input_desc, &output_desc) == 0) {
                    return 0;
                }
                close(fd[1]);
                dup2(fd[0], STDIN_FILENO);
                wait(NULL);     
                execvp(args2[0], args2);
                close_terminal(io_flag, input_desc, output_desc);
                close(fd[0]);
                fflush(stdin);
            } else if(pid2 == 0) {  
                char *input_file, *output_file;
                int input_desc, output_desc;
                unsigned io_flag = check_redirection(args, &args_num, &input_file, &output_file);    
                io_flag &= 1;   
                if(redirection_io(io_flag, input_file, output_file, &input_desc, &output_desc) == 0) {
                    return 0;
                }
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO);
                execvp(args[0], args);
                close_terminal(io_flag, input_desc, output_desc);
                close(fd[1]);
                fflush(stdin);
            }
        } else {    
            char *input_file, *output_file;
            int input_desc, output_desc;
            unsigned io_flag = check_redirection(args, &args_num, &input_file, &output_file);    
            if(redirection_io(io_flag, input_file, output_file, &input_desc, &output_desc) == 0) {
                return 0;
            }
            execvp(args[0], args);
            close_terminal(io_flag, input_desc, output_desc);
            fflush(stdin);
        }
    } else { 
        if(!run_concurrently) { 
            wait(NULL);
        }
    }
    return 1;
}

int main(void) {
    char *args[MAX_LINE / 2 + 1]; 
    char command[MAX_LINE + 1];
    init_args(args);
    first_command(command);
    while (1) {
        printf("?Enter Command>");
        fflush(stdout);
        fflush(stdin);
        refresh_args(args);
        if(!get_input(command)) {
            continue;
        }
        size_t args_num = change_input(args, command);
        if(args_num == 0) { 
            printf("Please enter the command! (or type \"exit\" to exit)\n");
            continue;
        }
        if(strcmp(args[0], "exit") == 0) {
            break;
        }
        run_command(args, args_num);
    }
    refresh_args(args);    
    return 0;
}
