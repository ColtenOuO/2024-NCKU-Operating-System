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
    char mtext[100];
} message_t;

typedef struct {
    int flag;      // 1 for message passing, 2 for shared memory
    union {
        int msqid;
        char* shm_addr;
    } storage;
} mailbox_t;

void receive_message(message_t* message_ptr, mailbox_t* mailbox_ptr) {
    if (mailbox_ptr->flag == 1) {
        if (msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(message_ptr->mtext), 0, 0) == -1) {
            perror("msgrcv failed");
            exit(1);
        }
    } else if (mailbox_ptr->flag == 2) {
        // 從共享記憶體讀取消息
        strcpy(message_ptr->mtext, mailbox_ptr->storage.shm_addr);
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

    struct timespec start, end;
    double time_taken;

    // 開始計時
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        sem_wait(receiver_sem);  // 等待接收者信號量

        receive_message(&message, &mailbox);

        if (strcmp(message.mtext, "exit") == 0) {
            break;
        }

        sem_post(sender_sem);  // 解鎖發送者
    }

    // 結束計時
    clock_gettime(CLOCK_MONOTONIC, &end);

    // 計算總時間
    time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
    printf("Total time taken in receiving msg: %f seconds\n", time_taken);

    if (mailbox.flag == 2) {
        munmap(mailbox.storage.shm_addr, sizeof(message_t));
    }

    sem_close(sender_sem);
    sem_close(receiver_sem);
    sem_unlink("/sender_sem");
    sem_unlink("/receiver_sem");

    return 0;
}
