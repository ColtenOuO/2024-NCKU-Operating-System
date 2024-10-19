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
        int msqid;     // ID
        char* shm_addr; // Share Mem address
    } storage;
} mailbox_t;

void send_message(message_t message, mailbox_t* mailbox_ptr) {
    if ( mailbox_ptr -> flag == 1 ) {
        if ( msgsnd( mailbox_ptr -> storage.msqid, &message, sizeof(message.mtext), 0 ) == -1 ) {
            perror("msgsnd failed");
            exit(1);
        }
        printf("\033[31mSent: %s\033[0m", message.mtext);
    } else {
        printf("Share Mem。\n");
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

    printf("Opening sender semaphore...\n");
    sem_t *sender_sem = sem_open("/sender_sem", O_CREAT, 0644, 1);
    if (sender_sem == SEM_FAILED) {
        perror("sem_open sender_sem failed");
        exit(1);
    }

    printf("Opening receiver semaphore...\n");
    sem_t *receiver_sem = sem_open("/receiver_sem", O_CREAT, 0644, 0);
    if (receiver_sem == SEM_FAILED) {
        perror("sem_open receiver_sem failed");
        exit(1);
    }

    if (mailbox.flag == 1) {
        printf("Creating/Opening message queue...\n");
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

    printf("Opening input file: %s\n", input_file);
    FILE* file = fopen(input_file, "r");
    if (!file) {
        perror("fopen failed");
        exit(1);
    }

    while ( fgets(message.mtext, sizeof(message.mtext), file) != NULL ) {
        printf("Waiting for sender semaphore...\n");
        sem_wait(sender_sem);  // wait for signal
        message.mtype = 1;  // 設置消息類型
        send_message(message, &mailbox);

        printf("Unlocking receiver semaphore...\n");
        sem_post(receiver_sem);
    }

    // 直接開溜
    printf("Sending exit message...\n");
    strcpy(message.mtext, "exit");
    send_message(message, &mailbox); 
    sem_post(receiver_sem);

    fclose(file);

    sem_close(sender_sem);
    sem_close(receiver_sem);

    return 0;
}
