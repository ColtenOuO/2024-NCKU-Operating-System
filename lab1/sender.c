#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>  // 用於時間測量

typedef struct {
    long mtype;
    char mtext[2000];
} message_t;

typedef struct {
    int flag;      // 1 for message passing, 2 for shared memory
    union {
        int msqid;     // ID for message queue
        char* shm_addr; // Share memory address
    } storage;
} mailbox_t;
struct timespec start, end;
double time_taken;
void send_message(message_t message, mailbox_t* mailbox_ptr) {
    if ( mailbox_ptr->flag == 1 ) {
        clock_gettime(CLOCK_MONOTONIC, &start);
	    if ( msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message.mtext), 0) == -1 ) {
            perror("msgsnd failed");
            exit(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
    } else if (mailbox_ptr->flag == 2) {

	    clock_gettime(CLOCK_MONOTONIC, &start);
        strcpy(mailbox_ptr->storage.shm_addr, message.mtext); 
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

    } else {
        printf("Unknown communication method\n");
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <communication method> <input file>\n", argv[0]);
        exit(1);
    }

    int method = atoi(argv[1]);
    const char* input_file = argv[2];

    mailbox_t mailbox;
    message_t message;
    mailbox.flag = method;

    sem_t *sender_sem = sem_open("/sender_sem", O_CREAT, 0644, 1);
    sem_t *receiver_sem = sem_open("/receiver_sem", O_CREAT, 0644, 0);
   
    if (sender_sem == SEM_FAILED || receiver_sem == SEM_FAILED) {
        perror("sem_open failed");
        exit(1);
    }

    if (mailbox.flag == 1) {
        key_t key = ftok("receiver.c", 'B');
        if (key == -1) {
            perror("ftok failed");
            exit(1);
        }
        mailbox.storage.msqid = msgget(key, 0666 | IPC_CREAT);
        if (mailbox.storage.msqid == -1) {
            perror("msgget failed");
            exit(1);
        }
    } else if (mailbox.flag == 2) {
        int shm_fd = shm_open("/shm_comm", O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("shm_open failed");
            exit(1);
        }
        ftruncate(shm_fd, sizeof(message_t)); 
        mailbox.storage.shm_addr = mmap(0, sizeof(message_t), PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (mailbox.storage.shm_addr == MAP_FAILED) {
            perror("mmap failed");
            exit(1);
        }
    }

    FILE* file = fopen(input_file, "r");
    if (!file) {
        perror("fopen failed");
        exit(1);
    }

    while (fgets(message.mtext, sizeof(message.mtext), file) != NULL) {
        sem_wait(sender_sem);

        message.mtype = 1;
        printf("\033[31mSent: %s\033[0m", message.mtext);
        send_message(message, &mailbox);
        sem_post(receiver_sem);
    }

    sem_wait(sender_sem);
    strcpy(message.mtext, "exit");
    send_message(message, &mailbox);
    sem_post(receiver_sem); 

    fclose(file);

    printf("\nTotal time taken in sending msg: %f seconds\n", time_taken);

    if (mailbox.flag == 2) {
        munmap(mailbox.storage.shm_addr, sizeof(message_t));
        shm_unlink("/shm_comm"); 
    }

    sem_close(sender_sem);
    sem_close(receiver_sem);

    return 0;
}
