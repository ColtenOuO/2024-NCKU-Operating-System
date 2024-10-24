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
        int msqid;
        char* shm_addr;
    } storage;
} mailbox_t;


struct timespec start, end;
double time_taken;

void receive_message(message_t* message_ptr, mailbox_t* mailbox_ptr) {
    if (mailbox_ptr->flag == 1) {
	    clock_gettime(CLOCK_MONOTONIC, &start);
        if (msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(message_ptr->mtext), 0, 0) == -1) {
            perror("msgrcv failed");
            exit(1);
        }
	clock_gettime(CLOCK_MONOTONIC, &end);
	time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
    } else if (mailbox_ptr->flag == 2) {
	clock_gettime(CLOCK_MONOTONIC, &start);
        strcpy(message_ptr->mtext, mailbox_ptr->storage.shm_addr);
    	clock_gettime(CLOCK_MONOTONIC, &end);
    	time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
    } else {
        printf("Unknown communication method\n");
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <communication method>\n", argv[0]);
        exit(1);
    }

    int method = atoi(argv[1]);
    mailbox_t mailbox;
    message_t message;
    mailbox.flag = method;

    sem_t *sender_sem = sem_open("/sender_sem", O_CREAT, 0644, 0);
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
        int shm_fd = shm_open("/shm_comm", O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("shm_open failed");
            exit(1);
        }
        mailbox.storage.shm_addr = mmap(0, sizeof(message_t), PROT_READ, MAP_SHARED, shm_fd, 0);
        if (mailbox.storage.shm_addr == MAP_FAILED) {
            perror("mmap failed");
            exit(1);
        }
    }



    while (1) {
        sem_wait(receiver_sem); 

        receive_message(&message, &mailbox);

        if (strcmp(message.mtext, "exit") == 0) {
            break;
        }
        printf("\033[32mReceived: %s\033[0m", message.mtext);
        sem_post(sender_sem);
    }

    printf("\nTotal time taken in receiving msg: %f seconds\n", time_taken);

    if (mailbox.flag == 2) {
        munmap(mailbox.storage.shm_addr, sizeof(message_t));
    }

    sem_close(sender_sem);
    sem_close(receiver_sem);
    sem_unlink("/sender_sem");
    sem_unlink("/receiver_sem");

    return 0;
}
