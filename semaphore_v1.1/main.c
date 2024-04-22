#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define DB_SIZE 10
#define SEM_WRITER "/sem_writer"
#define SHM_PATH "/shm_example"

int shm_fd;
int *database;
sem_t *writer_sem;

unsigned long long factorial(int n) {
    if (n == 0) return 1;
    unsigned long long f = 1;
    for (int i = 1; i <= n; ++i) f *= i;
    return f;
}

void handle_sigint(int sig) {
    printf("Received SIGINT, cleaning up and exiting...\n");

    // Закрытие и удаление разделяемой памяти и семафора
    munmap(database, DB_SIZE * sizeof(int));
    close(shm_fd);
    shm_unlink(SHM_PATH);
    sem_close(writer_sem);
    sem_unlink(SEM_WRITER);

    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);

    writer_sem = sem_open(SEM_WRITER, O_CREAT | O_EXCL, 0666, 1);
    if (writer_sem == SEM_FAILED) {
        perror("Failed to open semaphor");
        exit(EXIT_FAILURE);
    }

    shm_fd = shm_open(SHM_PATH, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Failed to open shared memory");
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_fd, DB_SIZE * sizeof(int));
    database = mmap(NULL, DB_SIZE * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (database == MAP_FAILED) {
        perror("Failed to map shared memory");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < DB_SIZE; i++) {
        database[i] = rand() % 20 + 1;
    }

    pid_t pid;
    int n = 5;

    for (int i = 0; i < n; i++) {
        pid = fork();
        if (pid == 0) {
            if (i % 2 == 0) {
                while (1) {
                    sem_wait(writer_sem);
                    int idx = rand() % DB_SIZE;
                    int old_val = database[idx];
                    int new_val = rand() % 20 + 1;
                    database[idx] = new_val;
                    printf("Writer %d: index %d, old value %d, new value %d\n", getpid(), idx, old_val, new_val);
                    sem_post(writer_sem);
                    sleep(1);
                }
            } else {
                while (1) {
                    int idx = rand() % DB_SIZE;
                    int val = database[idx];
                    if (val <= 20) {
                        printf("Reader %d: index %d, value %d, factorial %llu\n", getpid(), idx, val, factorial(val));
                    } else {
                        printf("Reader %d: index %d, value %d, factorial too large\n", getpid(), idx, val);
                    }
                    sleep(2);
                }
            }
        }
    }
    while (wait(NULL) > 0);

    return 0;
}


//int main() {
//    if (sem_unlink(SEM_WRITER) == -1) {
//        perror("Failed to unlink semaphor");
//        return 1;
//    }
//    printf("Semaphore unlinked successfully.\n");
//    return 0;
//}