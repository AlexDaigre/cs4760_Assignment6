#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h> 

void closeProgramSignal(int sig);
void closeProgram();

void setupSharedClock();
#define CLOCKVAR 0
#define SHMNAME "/tmp/daigreTmp99999"
int clockShmId;
int* clockShmPtr;

void setupMsgQueue();
#define QUEUEVAR 0
#define QUEUENAME "/tmp/daigreTmp99998"
int msgQueueId;

struct mesg_buffer { 
    long mtype;
    pid_t pid;
    int location;
    char readOrWrite;
} message;

int main (int argc, char *argv[]) {
    signal(SIGINT, closeProgramSignal);
    srand ( time(NULL) );

    // setupSharedClock();
    setupMsgQueue();

    int i;
    for(i = 0; i < 100; i++){
        message.mtype = 1;
        message.readOrWrite = (rand() % 2) == 0 ? 'r' : 'w';
        message.location = rand() % 32768;
        message.pid = getpid();
        int msgSent = msgsnd(msgQueueId, &message, sizeof(message), 0);
        if (msgSent < 0){
            printf("Child %d: failed to send message.\n", getpid());
            printf("Error: %d\n", errno);
            closeProgram();
        }

        msgrcv(msgQueueId, &message, sizeof(message), getpid(), 0);
    }

    // printf("Child %d: an error has occured.", getpid());
    // printf("Child %d: Recived permission to close\n.", getpid());
    closeProgram();
}

void closeProgramSignal(int sig){
    closeProgram();
}

void closeProgram(){
    shmdt(clockShmPtr);
    // printf("Child %d Exiting gracefully.\n", getpid());
    exit(0);
}

void setupSharedClock(){
    key_t sharedClockKey;
    if (-1 != open(SHMNAME, O_CREAT, 0777)) {
        sharedClockKey = ftok(SHMNAME, CLOCKVAR);
    } else {
        printf("ftok error in child: setupSharedClock\n");
        printf("Error: %d\n", errno);
        exit(1);
    }

    clockShmId = shmget(sharedClockKey, sizeof(int)*2, IPC_CREAT | 0777);
    if (clockShmId < 0) {
        printf("shmget error in child: setupSharedClock\n");
        printf("Error: %d\n", errno);
        exit(1);
    }

    clockShmPtr = (int *) shmat(clockShmId, NULL, 0);
    if ((long) clockShmPtr == -1) {
        printf("shmat error in child: setupSharedClock\n");
        printf("Error: %d\n", errno);
        shmctl(clockShmId, IPC_RMID, NULL);
        exit(1);
    }
}

void setupMsgQueue(){
    key_t msgQueueKey;
    if (-1 != open(QUEUENAME, O_CREAT, 0777)) {
        msgQueueKey = ftok(SHMNAME, QUEUEVAR);
    } else {
        printf("ftok error in child: setupMsgQueue\n");
        printf("Error: %d\n", errno);
        exit(1);
    }

    msgQueueId = msgget(msgQueueKey, 0777 |IPC_CREAT);
    if (msgQueueId < 0) {
        printf("msgget error in child: setupMsgQueue\n");
        printf("Error: %d\n", errno);
        exit(1);
    }
}