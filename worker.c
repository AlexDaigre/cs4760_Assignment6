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
#define SHMNAME "/tmp/daigreTmp43648"

void setupMsgQueue();
#define QUEUEVAR 0
#define QUEUENAME "/tmp/daigreTmp48083"
int msgQueueId;

int clockShmId;
int* clockShmPtr;

struct mesg_buffer { 
    long mtype; 
    char mtext[100]; 
} message; 

void requestOrReleaseResource(int requestOrRelease);
void releaseAllResources();
int maxResources[20];
int alocatedResources[20] = {0};

int main (int argc, char *argv[]) {
    signal(SIGINT, closeProgramSignal);
    srand ( time(NULL) );

    char* maxResourcesString = argv[1];
    char* stringElement = strtok(maxResourcesString, "/");

    int i = 0;
    while (stringElement != NULL){
        maxResources[i++] = atoi(stringElement);
        stringElement = strtok (NULL, "/");
    }

    setupSharedClock();
    setupMsgQueue();

    // printf("time: %d:%d\n", clockShmPtr[0], clockShmPtr[1]);

    // printf("{");
    // for (i = 0; i < 20; ++i) {
    //     printf("%d,", maxResources[i]);
    // }
    // printf("}\n");

    for(;;){
        int action = rand() % 100;

        if (action >= 96){
            //release resources and close program
            releaseAllResources();
            closeProgram();
        } else if ((action < 86) && (action >= 78)){
            //request resource
            requestOrReleaseResource(1);
        } else if ((action < 78) && (action >= 70)){
            //release resource
            requestOrReleaseResource(1);
        } else if (action < 70){
            //do nothing
            continue;
        }

    }

    printf("Child %d: an error has occured.", getpid());
    closeProgram();
}

void closeProgramSignal(int sig){
    closeProgram();
}

void closeProgram(){
    // shmctl(clockShmId, IPC_RMID, NULL);
    shmdt(clockShmPtr);
    printf("Child %d Exiting gracefully.\n", getpid());
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

    clockShmId = shmget(sharedClockKey, sizeof(int)*2, IPC_CREAT | 0666);
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
        printf("ftok error in parrent: setupMsgQueue\n");
        printf("Error: %d\n", errno);
        exit(1);
    }

    msgQueueId = msgget(msgQueueKey, 0777 |IPC_CREAT);
    if (msgQueueId < 0) {
        printf("msgget error in parrent: setupMsgQueue\n");
        printf("Error: %d\n", errno);
        exit(1);
    }
}

void requestOrReleaseResource(int requestOrRelease) {
    int resorcesToRequest[20];
    int i;
    for (i = 0; i < 20; i++){
        int amount = (rand() % 4) * requestOrRelease;
        // int amount = (rand() % 4);
        int newAllocation = alocatedResources[i] + amount;
        if ((newAllocation <= maxResources[i]) && (newAllocation >= 0)){
            resorcesToRequest[i] = amount;
        } else {
            resorcesToRequest[i] = 0;
        }
    }

    message.mtype = 1;
    // message.mtext[0] ='\0';
    strcpy(message.mtext, "");
    sprintf(
        message.mtext, 
        "%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d", 
        getpid(),
        resorcesToRequest[0],
        resorcesToRequest[1],
        resorcesToRequest[2],
        resorcesToRequest[3],
        resorcesToRequest[4],
        resorcesToRequest[5],
        resorcesToRequest[6],
        resorcesToRequest[7],
        resorcesToRequest[8],
        resorcesToRequest[9],
        resorcesToRequest[10],
        resorcesToRequest[11],
        resorcesToRequest[12],
        resorcesToRequest[13],
        resorcesToRequest[14],
        resorcesToRequest[15],
        resorcesToRequest[16],
        resorcesToRequest[17],
        resorcesToRequest[18],
        resorcesToRequest[19]
    );
    
    printf("Child %d: sending message to parent requesting ", getpid());
    printf("{");
    for (i = 0; i < 20; ++i) {
        printf("%d,", resorcesToRequest[i]);
    }
    printf("}\n");

    int msgSent = msgsnd(msgQueueId, &message, sizeof(message), 0);
    if (msgSent < 0){
        printf("Child %d: failed to send message.\n", getpid());
        printf("Error: %d\n", errno);
        closeProgram();
    }
    printf("Child %d: SENT message to parent\n", getpid());

    msgrcv(msgQueueId, &message, sizeof(message), getpid(), 0);

    for (i = 0; i < 20; i++){
        alocatedResources[i] += resorcesToRequest[i];
    }

    printf("Child %d: recived message from parent\n", getpid());
}


void releaseAllResources(){
    message.mtype = 1;
    // message.mtext[0] ='\0';
    strcpy(message.mtext, "");
    sprintf(
        message.mtext, 
        "%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d", 
        getpid(),
        alocatedResources[0] * -1,
        alocatedResources[1] * -1,
        alocatedResources[2] * -1,
        alocatedResources[3] * -1,
        alocatedResources[4] * -1,
        alocatedResources[5] * -1,
        alocatedResources[6] * -1,
        alocatedResources[7] * -1,
        alocatedResources[8] * -1,
        alocatedResources[9] * -1,
        alocatedResources[10] * -1,
        alocatedResources[11] * -1,
        alocatedResources[12] * -1,
        alocatedResources[13] * -1,
        alocatedResources[14] * -1,
        alocatedResources[15] * -1,
        alocatedResources[16] * -1,
        alocatedResources[17] * -1,
        alocatedResources[18] * -1,
        alocatedResources[19] * -1
    );

    int msgSent = msgsnd(msgQueueId, &message, sizeof(message), 0);
    if (msgSent < 0){
        printf("Child %d: failed to send message.\n", getpid());
        printf("Error: %d\n", errno);
        closeProgram();
    }
    printf("Child %d: Requested to close\n", getpid());

    msgrcv(msgQueueId, &message, sizeof(message), getpid(), 0);

    printf("Child %d: authorised to close\n", getpid());
}