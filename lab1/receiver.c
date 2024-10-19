#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>

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
        if (msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(message_ptr->mtext), 0, 0) == -1) { // type = 0
            perror("msgrcv failed");
            exit(1);
        }
    } else {
        printf("Share Mem\n");
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <communication method>\n", argv[0]);
        exit(1);
    }

    int method = atoi(argv[1]); // 根據參數選擇通信方法

    mailbox_t mailbox;
    message_t message;
    mailbox.flag = method;

    sem_t *sender_sem = sem_open("/sender_sem", O_CREAT, 0644, 0); // O_CREAT: 信號不存在就創
    sem_t *receiver_sem = sem_open("/receiver_sem", O_CREAT, 0644, 0); // 0: 叫你先不要慌，1: 我好慌

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
    }

    while (1) {
        sem_wait(receiver_sem);

        receive_message(&message, &mailbox);
        printf("\033[32mReceived: %s\033[0m", message.mtext);

        if (strcmp(message.mtext, "exit") == 0) {
            break;
        }

        sem_post(sender_sem); 
    }

    sem_close(sender_sem);
    sem_close(receiver_sem);

    sem_unlink("/sender_sem");
    sem_unlink("/receiver_sem");

    return 0;
}
