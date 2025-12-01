#include <stdio.h>     
#include <stdlib.h>   
#include <unistd.h>    
#include <sys/wait.h> 
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <cstring>  
#include <time.h> 
#include <semaphore.h>


struct Rubric {
    int q_num;
    char rubric_char;
};

struct ExamState {
    int student_num;
    bool is_marked[5];
};

struct Ctrl {
    int current_exam;
    bool stop;
};

struct SharedData {
    Rubric rubric[5];
    ExamState exam;
    Ctrl ctl;
    
    sem_t sem_rubric;  // for rubric access
    sem_t sem_mark;    // for exam.is_marked access
    sem_t sem_exam;    // protects load_exam + ctl.current_exam + ctl.stop
};

int all_qs_marked(SharedData* shared_data) {
    for (int i = 0; i < 5; i++) {
        if (!shared_data->exam.is_marked[i]) {
            return 0; 
        }
    }
    return 1;
}

void check_rubric(SharedData* shared_data, int ta_id) {
    for (int i = 0; i < 5; i++)
    {
        int delay = 500 + rand() % 501; // Random delay between 500ms and 1000ms
        usleep(delay * 1000); // Convert to ms

        int change_rubric = rand() % 2;

        if (change_rubric) {
            
            //Lock
            sem_wait(&shared_data->sem_rubric);

            char old_char = shared_data->rubric[i].rubric_char;
            char new_char = old_char + 1;

            shared_data->rubric[i].rubric_char = new_char;
            
            printf("TA %d changed rubric for Q%d from '%c' to '%c'\n", ta_id, shared_data->rubric[i].q_num, old_char, new_char);
        
            FILE* rubric_file = fopen("rubric.txt", "r+");
            if (rubric_file != NULL) {
                for (int j = 0; j < 5; j++) {

                    fprintf(rubric_file, "%d , %c\n", shared_data->rubric[j].q_num, shared_data->rubric[j].rubric_char);
                }
                fclose(rubric_file);
            } else {
                printf("TA %d failed to open rubric.txt for writing\n", ta_id);
            }
            //Unlock
            sem_post(&shared_data->sem_rubric);
        } else {
            printf("TA %d did not change rubric for Q%d\n", ta_id, i + 1);
        }
    }
}

void mark_exam_questions(SharedData* shared_data, int ta_id) {
    int student_num = shared_data->exam.student_num;

    while (!all_qs_marked(shared_data)) {
        for (int i = 0; i < 5; i++){
            //Lock
            sem_wait(&shared_data->sem_mark);

            if (!shared_data->exam.is_marked[i]) {
                shared_data->exam.is_marked[i] = true;
                //unlock
                sem_post(&shared_data->sem_mark);
                printf("TA %d marking Q%d for student %d\n", ta_id, i + 1, student_num);

                int delay = 1000 + rand() % 1001; 
                usleep(delay * 1000);

                printf("TA %d finished marking Q%d for student %d\n", ta_id, i + 1, student_num);
            } 

            // Unlock
            sem_post(&shared_data->sem_mark);
        }
    }

    printf("TA %d finished marking all questions for student %d\n", ta_id, student_num);
}


int load_exam(SharedData* shared_data, int exam_index) {
    char filename[32];
    sprintf(filename, "exam%02d", exam_index + 1);

    FILE* exam_file = fopen(filename, "r");
    if (exam_file == NULL) {
        printf("TA Failed to open exam file %s\n", filename);
        return -1;
    }

    char line[256];
    if (fgets(line, sizeof(line), exam_file) == NULL) {
        perror("Failed to read exam file");
        fclose(exam_file);
        return -1;
    }

    int student_number = atoi(line);
    shared_data->exam.student_num = student_number;

    for (int i = 0; i < 5; i++) {
        shared_data->exam.is_marked[i] = false;
    }


    shared_data->ctl.current_exam = exam_index;

    printf("Loaded exam %d for student %d\n", exam_index + 1, student_number);
    fclose(exam_file);
    return 0;

}


int main(int argc, char* argv[]) {
    if (argc != 2)
    {
        printf("Num of TAs should be at least 2\n");
        return 1;
    }
    
    srand(time(NULL));

    int num_TAs = atoi(argv[1]);
    if (num_TAs < 2) {
        printf("Num of TAs should be at least 2\n");
        return 1;
    }


    //Shared Memory
    int shared_size = sizeof(SharedData);
    int shared_id = shmget(IPC_PRIVATE, shared_size, IPC_CREAT | 0666);
    if (shared_id < 0) {
        perror("shmget failed");
        return 1;
    }

    SharedData* shared_data = (SharedData*)shmat(shared_id, NULL, 0);
    if (shared_data == (SharedData*)-1) {
        perror("shmat failed");
        return 1;
    }

    memset(shared_data, 0, shared_size);
    shared_data -> ctl.current_exam = 0;
    shared_data -> ctl.stop = false;

    printf("Parent process PID = %d\n", getpid());
    printf("Creating %d TAs..\n", num_TAs);

    //Adding Rubric
    FILE* rubric_file = fopen("rubric.txt", "r");
    if (rubric_file == NULL) {
        perror("Failed to open rubric.txt");
        return 1;
    }

    char line[256];
    int rubric_index = 0;
    while (fgets(line, sizeof(line), rubric_file) != NULL && rubric_index < 5) {
        int q_num;
        char rubric_char;

        if (sscanf(line, "%d , %c", &q_num, &rubric_char) == 2) {
            shared_data->rubric[rubric_index].q_num = q_num;
            shared_data->rubric[rubric_index].rubric_char = rubric_char;
            rubric_index++;
        } else {
            printf("Invalid line in rubric.txt: %s", line);
        }
    }

    fclose(rubric_file);

    printf("Rubric loaded:\n");
    for (int j = 0; j < 5; j++) {
        printf("  Q%d -> '%c'\n",
            shared_data->rubric[j].q_num,
            shared_data->rubric[j].rubric_char);
    }


    if (rubric_index < 5)
    {
        printf("rubric entries are below 5\n");
        return 1;
    }

    //loading exams 
    if (load_exam(shared_data, 0) != 0) {
        shmdt(shared_data);
        shmctl(shared_id, IPC_RMID, NULL);
        return 1;
    }

    // Checking semaphore initialization
    if (sem_init(&shared_data->sem_rubric, 1, 1) == -1)
    {
        perror("sem_init sem_rubric failed");
        return 1;
    }

    if (sem_init(&shared_data->sem_mark, 1, 1) == -1)
    {
        perror("sem_init sem_mark failed");
        return 1;
    }

    if (sem_init(&shared_data->sem_exam, 1, 1) == -1)
    {
        perror("sem_init sem_exam failed");
        return 1;
    }

    for(int i = 0; i < num_TAs; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
            return 1;

        } else if (pid == 0) {
            printf("TA %d process PID = %d\n", i + 1, getpid());
            
            while (!shared_data->ctl.stop)
            {
                check_rubric(shared_data, i + 1);
                mark_exam_questions(shared_data, i + 1);

                //Lock
                sem_wait(&shared_data->sem_exam);

                int next_exam = shared_data->ctl.current_exam + 1;
                if (load_exam(shared_data, next_exam) != 0)
                {
                    printf("TA %d: No more exams to load\n", i + 1);
                    shared_data->ctl.stop = true;
                }

                if (shared_data->exam.student_num == 9999)
                {
                    printf("TA %d: reached student 9999\n", i + 1);
                    shared_data->ctl.stop = true;
                }

                //Unlock
                sem_post(&shared_data->sem_exam);
            }
            

            printf("TA %d done with currnet exam, exiting:\n", i + 1);

            return 0;
        }
    }

    for (int i = 0; i < num_TAs; i++) {
        wait(NULL);
    }



    


    if (sem_destroy(&shared_data->sem_rubric) == -1) {
        perror("sem_destroy sem_rubric failed");
    }

    if (sem_destroy(&shared_data->sem_mark) == -1) {
        perror("sem_destroy sem_mark failed");
    }   

    if (sem_destroy(&shared_data->sem_exam) == -1) {
        perror("sem_destroy sem_exam failed");
    }


    // Detaching 
    if (shmdt(shared_data) == -1)
    {
        perror("shmdt failed");
        return 1;
    } 
    if (shmctl(shared_id,IPC_RMID, nullptr) == -1)
    {
        perror("shmctl failed");
        return 1;
    }

    printf("All TAs finished their work.\n");

    return 0;
}

