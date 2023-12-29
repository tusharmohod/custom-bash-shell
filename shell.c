#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64

#define BACKGROUND 0
#define SEQUENCE 1
#define PARALLEL 2

struct background_process {
    int list[MAX_INPUT_SIZE], count;
};

void ctrl_c_handler(int *ccsig) {
    int status, value = 1;
    int kill_by_ctrl_c = 0;
    if (kill_by_ctrl_c != 0) {
        value = kill(kill_by_ctrl_c, SIGKILL);
    }

    if (value == 0) {
        printf("Process killed.");
        waitpid(kill_by_ctrl_c, &status, 0);
    }
}

void exit_function(struct background_process *bg_process) {
    int curr_count = bg_process->count;
    for (int i = 0; i < curr_count; i++) {
        printf("Process killed with %d\n", bg_process->list[i]);
        kill(bg_process->list[i], 9);
    }
    bg_process->count = 0;
}

void change_directory(char **args) {
    char present_dir[MAX_INPUT_SIZE];
    getcwd(present_dir, MAX_INPUT_SIZE);

    if (args[1] == NULL) {
        printf("Expected argument is: cd <directory path>.\n");
        _exit(1);
        return;
    } else {
        if (strcmp(args[1], "~") == 0) {
            char *home = getenv("HOME");
            if (chdir(home) != 0) {
                printf("No such directory exists.\n");
                _exit(1);
            } else {
                printf("Directory changed from %s to %s.\n", present_dir, home);
                _exit(0);
            }
        } else {
            if (chdir(args[1]) != 0) {
                printf("No such directory exists.\n");
                _exit(1);
            } else {
                printf("Directory changed from %s to %s.\n", present_dir, args[1]);
                _exit(1);
            }
        }
    }
}

void read_history(char **args) {
    FILE* file_ptr = fopen("/home/mohod/history.txt", "r");

    if (file_ptr == NULL) {
        fprintf(stderr, "The file is not opened.\n");
        _exit(1);
        return;
    }
    fprintf(stdin, "This should not be printed\n");

    char ch;
    while ((ch = fgetc(file_ptr)) != EOF) {
        printf("%c", ch);
    };

    fclose(file_ptr);
    _exit(0);
}

void sys_command(char **args) {
    execvp(args[0], args);
    fprintf(stderr, "Command not found: %s\n", args[0]);
    _exit(1);
}

void run_command(char **args, struct background_process *bg_process) {
    if (strcmp(args[0], "exit") == 0) {
        exit_function(bg_process);
        printf("Exiting custom shell.\n");
        _exit(0);
    }
    
    if (strcmp(args[0], "cd") == 0) {
        change_directory(args);
    } else if (strcmp(args[0], "history") == 0) {
        read_history(args);
    } else {
        sys_command(args);
    }
}

void background_command(char **args, struct background_process *bg_process, int token_count) {
    args[token_count - 1] = NULL;
    pid_t pid = fork();
    int status;

    if (pid < 0) {
        printf("Fork failed.\n");
        return;
    } else if(pid == 0) {
        run_command(args, bg_process);
    } else {
        int curr_count = bg_process->count;
        bg_process->list[curr_count] = pid;
        ++curr_count;
        bg_process->count = curr_count;
        setpgid(pid, pid);
    }
}

void sequence_commands(char **args, struct background_process *bg_process) {
    int i, j = 0;
    int status;
    char *cmd[MAX_INPUT_SIZE];

    for (i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "&&") == 0) {
            cmd[j] = NULL;
			pid_t pid;
            pid = fork();
            if (pid < 0) {
                printf("Fork failed\n");
                return;
            } else if (pid == 0) {
                run_command(cmd, bg_process);
            } else {
                waitpid(pid, &status, 0);
                while (j >= 0) {
                    free(cmd[j--]);
                }
                j = 0;
            }
        } else {
            cmd[j] = malloc(MAX_INPUT_SIZE * sizeof(char));
            strcpy(cmd[j++], args[i]);
        }
    }

    cmd[j] = NULL;
	pid_t pid;
    pid = fork();
    if (pid < 0) {
        printf("Fork failed\n");
        return;
    } else if (pid == 0) {
        run_command(cmd, bg_process);
    } else {
        waitpid(pid, &status, 0);
        while (j >= 0) {
            free(cmd[j--]);
        }
        j = 0;
    }
}

void parallel_commands(char **args, struct background_process *bg_process) {
    int i, j = 0;
    int status;
    char *cmd[MAX_INPUT_SIZE];
    int parallel_process[MAX_INPUT_SIZE];
    int process_count = 0;
	
    for (i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "&&&")==0) {
            cmd[j] = NULL;
            pid_t pid;
            pid = fork();
            if (pid < 0) {
                printf("Fork failed\n");
                return ;
            } else if (pid == 0) {
                run_command(cmd, bg_process);
            } else {
                parallel_process[process_count++] = pid;
                while (j >= 0) {
                    free(cmd[j--]);
                }
                j = 0;
            }
        }
        else {
            cmd[j] = malloc(MAX_INPUT_SIZE * sizeof(char));
            strcpy(cmd[j++], args[i]);
        }
    }    

    cmd[j] = NULL;
    pid_t pid;
    pid = fork();

    if (pid < 0) {
        printf("Fork failed\n");
        return;
    } else if (pid == 0){ 
        run_command(cmd, bg_process);
    } else {
        parallel_process[process_count++] = pid;
        while (j >= 0) {
            free(cmd[j--]);
        }
        for(int i = 0; i < process_count; i++) {
            waitpid(parallel_process[i], &status, 0);
        }
    }
}

void single_command(char **args, struct background_process *bg_process) {
    if (args[0] == '\0') {
        return;
    }
    
    if (strcmp(args[0], "exit") == 0) {
        exit_function(bg_process);
        printf("Exiting custom shell. Thank you. \n");
        _exit(0);
    }

    pid_t pid = fork();
    int status;

    if (pid < 0) {
        printf("Fork failed\n");
        return;
    } else if (pid == 0) {
        run_command(args, &bg_process);
    } else {
        pid = waitpid(pid, &status, 0);
    }
}

int check_command_type(char **args, int *command_type) {
    int token_count = 1;
	int i;

    if (command_type[BACKGROUND] == 1) {
        for (i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "&") == 0) {
                ++token_count;
            }
        }
    } else if (command_type[SEQUENCE] == 1) {
        for (i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "&&") == 0) {
                ++token_count;
            }
        }
    } else if (command_type[PARALLEL] == 1) {
        for (i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "&&&") == 0) {
                ++token_count;
            }
        }
    }

    return token_count;
}

void execute_command(char **args, int *command_type, struct background_process *bg_process) {
    int token_count = check_command_type(args, command_type);

    if (token_count == 1) {
        single_command(args, bg_process);
    }  else {
        if (command_type[BACKGROUND] == 1) {
            background_command(args, bg_process, token_count);
        } else if (command_type[SEQUENCE] == 1) {
            sequence_commands(args, bg_process);
        } else if (command_type[PARALLEL] == 1) {
            parallel_commands(args, bg_process);
        }
    }
}

void add_to_history(char **args) {
    FILE* file_ptr = fopen("/home/mohod/history.txt", "a+");

    if (file_ptr == NULL) {
        printf("The history file is not opened. The program will now exit.");
        return;
    }
	
	int i;
    for (i = 0; args[i] != NULL; i++) {
        fputs(args[i], file_ptr);
        fputs(" ", file_ptr);
    }
    fputs("\n", file_ptr);
    
    fclose(file_ptr);
}

char **tokenize(char *line) {
    char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
    char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
    int i, tokenIndex = 0, tokenNo = 0;

    for(i = 0; i < strlen(line); i++){
        char readChar = line[i];
        if (readChar == ' ' || readChar == '\n' || readChar == '\t') {
            token[tokenIndex] = '\0';
            if (tokenIndex != 0) {
                tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE * sizeof(char));
                strcpy(tokens[tokenNo++], token);
            }
            tokenIndex = 0; 
        } else {
            token[tokenIndex++] = readChar;
        }
    }

    free(token);
    tokens[tokenNo] = NULL ;
    return tokens;
}

void main(void) {
    char line[MAX_INPUT_SIZE];
    char **tokens;
    int command_type[3];
    struct background_process bg_process;
    bg_process.count = 0;

    signal(SIGINT, ctrl_c_handler);

    while (1) {
        command_type[0] = 0;
        command_type[1] = 0;
        command_type[2] = 0;

        printf("shell $ ");
        bzero(line, MAX_INPUT_SIZE);
        fgets(line, MAX_INPUT_SIZE, stdin);

        if (line[0] == '\0') {
            continue;
        }

        line[strlen(line)] = '\n';
        tokens = tokenize(line);

        int i;
        for (i = 0; tokens[i] != NULL; i++) {
            if (strcmp(tokens[i], "&") == 0) {
                command_type[BACKGROUND] = 1;
                break;
            } else if (strcmp(tokens[i], "&&") == 0) {
                command_type[SEQUENCE] = 1;
                break;
            } else if (strcmp(tokens[i], "&&&") == 0) {
                command_type[PARALLEL] = 1;
                break;
            }
        }

        add_to_history(tokens);
        execute_command(tokens, command_type, &bg_process);

        for (i = 0; tokens[i] != NULL; i++) {
            free(tokens[i]);
        }
        free(tokens);
    }

    exit(0);
}
