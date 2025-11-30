#include <stdio.h>     
#include <stdlib.h>   
#include <unistd.h>    
#include <sys/wait.h> 

struct Rubric {
    int q_num;
    char rubric_char;
};

struct ExamState {
    int student_num;
    bool is_completed[5];
};

struct Ctrl {
    int current_exam;
    bool stop;
};

struct SharedData {
    Rubric rubric[5];
    ExamState exam;
    Ctrl ctl;
};


int main(int argc, char* argv[]) {

    int num_TAs = atoi(argv[1]);

    printf("Parent process PID = %d\n", getpid());
    printf("Creating %d TAs..\n", num_TAs);

    for(int i = 0; i < num_TAs; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            return 1;

        } else if (pid == 0) {
            // Child process
            printf("TA %d process PID = %d\n", i + 1, getpid());
            sleep(1);

            printf("TA %d process PID = %d finished work.\n", i + 1, getpid());
            return 0;
        }
    }

    for (int i = 0; i < num_TAs; i++) {
        wait(NULL);
    }

    printf("All TAs finished their work.\n");


    return 0;
}