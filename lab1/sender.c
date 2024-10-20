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
        int msqid;     // ID for message queue
        char* shm_addr; // Share memory address
    } storage;
} mailbox_t;

void send_message(message_t message, mailbox_t* mailbox_ptr) {
    if ( mailbox_ptr->flag == 1 ) {
        if ( msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message.mtext), 0) == -1 ) {
            perror("msgsnd failed");
            exit(1);
        }
    } else if (mailbox_ptr->flag == 2) {
        // 使用共享記憶體
        strcpy(mailbox_ptr->storage.shm_addr, message.mtext);  // 將消息寫入共享記憶體
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
        ftruncate(shm_fd, sizeof(message_t));  // 設置共享記憶體大小
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

    struct timespec start, end;
    double time_taken;

    // 開始計時
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (fgets(message.mtext, sizeof(message.mtext), file) != NULL) {
        sem_wait(sender_sem);  // 等待信號量

        message.mtype = 1;

        send_message(message, &mailbox);
        sem_post(receiver_sem);  // 解鎖接收者
    }

    // 發送退出消息
    strcpy(message.mtext, "exit");
    sem_wait(sender_sem);
    send_message(message, &mailbox);
    sem_post(receiver_sem); 

    fclose(file);

    // 結束計時
    clock_gettime(CLOCK_MONOTONIC, &end);

    // 計算總時間
    time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
    printf("Total time taken in sending msg: %f seconds\n", time_taken);

    if (mailbox.flag == 2) {
        munmap(mailbox.storage.shm_addr, sizeof(message_t));
        shm_unlink("/shm_comm"); // 刪除共享記憶體對象
    }

    sem_close(sender_sem);
    sem_close(receiver_sem);

    return 0;
}
