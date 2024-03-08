#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <mush.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>

#define BUFFER_SIZE 1000
#define READ_END 0
#define WRITE_END 1
#define TRUEE 1

FILE *read_input(int argc, char **argv);
void create_pipes(int pipe_array[][2], int numberChildren);
void check_cd(clstage stage);
void create_args(clstage stage, char **args);
void execute_child(int i, int pipe_array[][2], int numberChildren, char **args, clstage stage);
void dup_pipes(int i, int pipe_array[][2], int numberChildren);
void closepipes(int pipe_array[][2], int numberOfPipes);
void print_usage_exit();
void print_cd_usage();
void perror_exit(char *msg);
extern char *readLongString(FILE *file);
extern pipeline crack_pipeline(char *line);
extern void print_pipeline(FILE *file, pipeline c1);
extern void free_pipeline(pipeline c1);
void redirection(char* filename, int ret_fd);
void set_homedir();
void wait_childs(int numberChildren);
void handler(int signum);
void handler2(int signum);

static int sig;

int main(int argc, char *argv[])
{
    char *line;
    pipeline pl;
    clstage stage;
    int numberChildren, i;
    int (*pipe_array)[2];
    pid_t child;
    FILE *file = read_input(argc, argv);
    
    while(TRUEE)
    {
        sig = 1;
        signal(SIGINT, &handler);
        if(file == stdin)
            printf("8-P ");
        if((line = readLongString(file)) == NULL)
        {
            printf("\n");
            exit(-1);
        }
        sig = 0;
        pl = crack_pipeline(line);
        if(pl == NULL)
            continue;
        stage = pl->stage;
        numberChildren = pl->length;
        pipe_array = malloc((numberChildren - 1) * 2 * sizeof(int));
        create_pipes(pipe_array, numberChildren);
        /*checking for cd, continues on to nect iteration after*/
        if(strcmp(stage->argv[0], "cd") == 0)
        {
            check_cd(stage);
            free_pipeline(pl);
            free(line);
            free(pipe_array);
            continue;
        }
    /*create a child for each process*/
        for(i = 0; i < numberChildren; i++)
        {
            char *args[(stage->argc)+1]; 
            create_args(stage, args);
            if((child = fork()) == 0)
                execute_child(i, pipe_array, numberChildren, args, stage);
            if (child == -1)
                perror_exit("fork");
            stage++;
        }
        closepipes(pipe_array, numberChildren-1);
        wait_childs(numberChildren);
        free_pipeline(pl);
        free(line);
        free(pipe_array);
    }
    return 0;
}

/* function for determining if there is an argument file*/
FILE *read_input(int argc, char **argv)
{
    FILE *file;
    if(argc > 2)
        print_usage_exit();
    if(argc == 1)
        file = stdin;
    else
        if((file = fopen(argv[1], "r")) == NULL)
            perror_exit("open");
    return file;
}
/* function for creating the pipes*/
void create_pipes(int pipe_array[][2], int numberChildren)
{
    int i;
    for (i = 0; i < numberChildren - 1; i++) {
        if (pipe(pipe_array[i]))
            perror_exit("pipe creation");
    }
}
/* function for checking if the cd arg is valid*/
void check_cd(clstage stage){
    
    if((stage->argc) > 2)
        print_cd_usage();
    if((stage->argc) == 2)
    {
        if(strcmp(stage->argv[1], "~") == 0)
            set_homedir();
        else if(chdir(stage->argv[1]))
            perror(stage->argv[1]);
    }
    else
        set_homedir();
}
/*takes the argv strings of a stage*/
void create_args(clstage stage, char **args)
{
    int k;
    for(k = 0; k < stage->argc; k++)
    {
        args[k] = stage->argv[k];
    }
    args[stage->argc] = NULL;
}
/* function for actually executing the command of the child*/
/* also checks for redirection*/
void execute_child(int i, int pipe_array[][2], int numberChildren, char **args, clstage stage)
{
    dup_pipes(i, pipe_array, numberChildren);
    closepipes(pipe_array, numberChildren-1);
    if(stage -> inname)
        redirection(stage -> inname, STDIN_FILENO);
    if(stage -> outname)
        redirection(stage -> outname, STDOUT_FILENO);
    execvp(stage->argv[0], args);
    perror_exit(stage->argv[0]);
}
/* dups the pipes depending on their placement*/
void dup_pipes(int i, int pipe_array[][2], int numberChildren)
{
    if(i == 0)
    {
        if(dup2(pipe_array[i][WRITE_END], STDOUT_FILENO) == -1)
            perror_exit("dup2");
    }
    else if(i < (numberChildren-1))
    {
        if(dup2(pipe_array[i-1][READ_END], STDIN_FILENO) == -1)
            perror_exit("dup2");
        if(dup2(pipe_array[i][WRITE_END], STDOUT_FILENO) == -1)
            perror_exit("dup2");
    }
    else if(i == (numberChildren-1))
    {
        if(dup2(pipe_array[i-1][READ_END], STDIN_FILENO) == -1)
            perror_exit("dup2");
    }
}
/* closes any open pipes*/
void closepipes(int pipe_array[][2], int numberOfPipes) {
    int j;
    for (j = 0; j < numberOfPipes; ++j) {
        if (close(pipe_array[j][READ_END]))
            perror_exit("close read end");
        if (close(pipe_array[j][WRITE_END]))
            perror_exit("close write end");
    }
}

/* function that handles redirection*/
void redirection(char* filename, int ret_fd){
    int fd, mode = 0; 
    if(ret_fd == STDOUT_FILENO){
        mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IWOTH;
        fd = open( filename , O_WRONLY | O_CREAT | O_TRUNC, mode);
    }else{
        fd = open(filename,  O_RDONLY);
    }
    if( fd == -1){
        perror_exit(filename);
    }
    if(dup2(fd, ret_fd) == -1){
        perror_exit(filename);
    }
}

/* this function is used for getting the home directory*/
void set_homedir(){
    struct passwd *user;
    char* home;
    user = getpwuid(getuid());
    home = user -> pw_dir;
    if(chdir(home)){
        perror(home);
    }
    return;
}
/* function that waits for all the children to terminate*/
void wait_childs(int numberChildren)
{
    int i;
    for(i = 0; i < numberChildren; i++)
    {
        if(wait(NULL) == -1)
            perror("wait");
    }
}
/* function for printing usage and exiting*/
void print_usage_exit()
{
    fprintf(stderr, "usage: mush2 [file]\n");
    exit(-1);
}
/* function for printing usage and exiting for cd*/
void print_cd_usage()
{
    fprintf(stderr, "usage: cd [ destdir ]\n");
}
/* function for printing error and exiting*/
void perror_exit(char *msg)
{
    perror(msg), 
    exit(-1);
}
/* signal handler*/
void handler(int signum)
{
    if(signum == SIGINT)
    {
        if(sig)
            write(STDOUT_FILENO, "\n8-P ", strlen("\n8-P "));
        else
            write(STDOUT_FILENO, "\n", strlen("\n"));
    }
}