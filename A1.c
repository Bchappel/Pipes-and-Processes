#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAX_FILES 100
#define ALPHABET_SIZE 26

pid_t child_pids[MAX_FILES];
int pipe_indices[MAX_FILES];
int pipes[MAX_FILES][2];
char * file_names[MAX_FILES]; // Changed to dynamic allocation
int file_count = 0;
int executed = 0;

void sigchld_handler(int signum){
    int child_status;
    pid_t child_pid;

    while((child_pid = waitpid(-1, &child_status, WNOHANG)) > 0){
        if(WIFEXITED(child_status)){
            printf("\t[OUTPUT] :: Child %d terminated normally with the status: %d\n", child_pid, WIFEXITED(child_status));
            executed++;
            int pid_index = -1;

            // Find the child index in the arrays
            for(int i = 0; i < file_count; i++){
                if(child_pids[i] == child_pid){
                    pid_index = i;
                    break;
                }
            }

            if(pid_index != -1){

                int histogram[ALPHABET_SIZE];
                if(read(pipes[pipe_indices[pid_index]][0], histogram, sizeof(int) * ALPHABET_SIZE) == -1){
                    perror("read");
                }else{
                    char filename[256];
                    sprintf(filename, "%s%d.hist", file_names[pipe_indices[pid_index]], (int)child_pid);
                    FILE *hist_file = fopen(filename, "w");
                    if(hist_file == NULL){
                        perror("fopen");
                    }else{
                        for(int i = 0; i < ALPHABET_SIZE; i++){
                            fprintf(hist_file, "%c %d\n", 'a' + i, histogram[i]);
                        }
                        if(fclose(hist_file) != 0){
                            perror("fclose");
                        }
                    }
                }

            }else{
                printf("[ERROR] :: Child PID %d not found in the child_pids array.\n", child_pid);
            }

            signal(SIGCHLD, sigchld_handler);
        }else if(WIFSIGNALED(child_status)){
            printf("\t[OUTPUT] :: Child %d terminated by signal %s.\n", child_pid, strsignal(WTERMSIG(child_status)));
            signal(SIGCHLD, sigchld_handler);
        }
    }
}

void calculate_histogram(const char * file_name, int * histogram){
    FILE *file = fopen(file_name, "r");
    int c;

    if(file == NULL){
        fprintf(stderr, "\t[ERROR] :: Unable to open file: %s\n", file_name);
        exit(EXIT_FAILURE);
    }

    while((c = fgetc(file)) != EOF){
        if(isalpha(c)){
            c = tolower(c); /*convert to lower case*/
            histogram[c - 'a']++;
        }
    }

    fclose(file);
    file = NULL;
}

int main(int argc, char *argv[]){
    printf("CIS*3110 :: Assignment 1 :: Braedan Chappel\n");

    signal(SIGCHLD, sigchld_handler); /*register the handler for SIGCHLD*/

    if(argc <= 1){
        fprintf(stderr, "\t[ERROR] Insufficient number of command line arguments.\n");
        return EXIT_FAILURE;
    }

    /*dynamically allocate the name array*/
    for (int i = 0; i < argc; i++){
        file_names[i] = malloc(256 * sizeof(char));
        if (file_names[i] == NULL) {
            perror("malloc");
            return EXIT_FAILURE;
        }
    }

    /*process the command line*/
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "SIG") == 0){
            printf("\t[INPUT]  :: Sending SIGINT to child %d\n", i);
            strcpy(file_names[file_count], argv[i]);
            file_count++;
        }else{
            printf("\t[INPUT]  :: File name added: %s\n", argv[i]);
            strcpy(file_names[file_count], argv[i]);
            file_count++;
        }
    }

    /*Create pipes and fork child processes*/
    for(int i = 0; i < file_count; i++){
        if(pipe(pipes[i]) == -1){
            close(pipes[i][1]);
            close(pipes[i][0]); /*closes the read end of the pipe*/
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork(); /*call to fork*/

        if(pid == -1){
            close(pipes[i][1]);
            close(pipes[i][0]);
            perror("fork");
            exit(EXIT_FAILURE);
        }else if(pid == 0){
            /*child process*/
            close(pipes[i][0]);

            int histogram[ALPHABET_SIZE] = {0};
            calculate_histogram(file_names[i], histogram);

            /*write to the pipe*/
            if(write(pipes[i][1], histogram, sizeof(int) * ALPHABET_SIZE) == -1){
                perror("write");
                exit(EXIT_FAILURE);
            }
            
            sleep(10 + 3 * i);

            for(int i = 0; i < argc; i++){
                free(file_names[i]); /*Free dynamically allocated memory for file_names array*/
            }

            close(STDIN_FILENO);  /*Close standard input*/
            close(STDOUT_FILENO); /*Close standard output*/
            close(STDERR_FILENO); /*close standard error*/

            close(pipes[i][1]);
            exit(EXIT_SUCCESS);
        }else{
            /*parent process*/
            if(strcmp(file_names[i], "SIG") == 0){
                kill(pid, SIGINT);
            }

            child_pids[i] = pid;
            pipe_indices[i] = i;
            close(pipes[i][1]);
        }
    }

    /*Wait for all child processes to terminate*/
    while(executed < file_count){
        sleep(1); // Sleep to avoid busy waiting
    }

    // Close read ends of the pipes in the parent process
    for(int i = 0; i < file_count; i++){
        close(pipes[i][0]);
    }

    close(STDIN_FILENO);  /*Close standard input*/
    close(STDOUT_FILENO); /*Close standard output*/
    close(STDERR_FILENO); /*close standard error*/

    for(int i = 0; i < argc; i++){
        free(file_names[i]); /*Free dynamically allocated memory for file_names array*/
    }

    return EXIT_SUCCESS;
}


