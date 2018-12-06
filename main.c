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
#include "queue.c"

void childClosedSignal(int sig);
int closeChild();
void closeProgramSignal(int sig);
void closeProgram();

void setupOutputFile();

void advanceTime();

void createProcesses();

void setupSharedClock();
#define CLOCKVAR 0
#define SHMNAME "/tmp/daigreTmp99999"
int clockShmId;
int* clockShmPtr;

void setupMsgQueue();
#define QUEUEVAR 0
#define QUEUENAME "/tmp/daigreTmp99998"
int msgQueueId;

void reciveMessages();

struct mesg_buffer { 
    long mtype;
    pid_t pid;
    int location;
    char readOrWrite;
} message;

FILE* outputFile;

int currentProcesses;
pid_t openProcesses[18] = {0};
pid_t blockedProcesses[18] = {0};
int maxProcesses;

struct memoryBlock {
    int pid;
    int refrenceBit;
    int dirtyBit;
    int readBit;
    int writeBit;
};

int pages[18][32] = {0};
int openFrames[256] = {0};
struct memoryBlock frames[256];

int createNextProcessAt = -1;

int freeFrameSpace();

struct Queue* pageQueue;

int main (int argc, char *argv[]) {
    srand ( time(NULL) );
    //set signals
    signal(SIGCHLD, childClosedSignal);
    signal(SIGINT, closeProgramSignal);

    //set default values and get command line inputs
    int c;
    int maxRunTime = 20;
    int maxProcesses = 18;
    char* logFile = "logFile.txt";

    while ((c = getopt (argc, argv, "hs:l:t:")) != -1){
        switch (c){
            case 'h':
                printf("Options:\n-h: Help\n-l: The given argument(string) specifies the neame of the logfile.\n-t: The given number(int) specifies the max amount of time the program will run for.\n");
                exit(0);
                break;
            case 's':
                if (atoi(optarg) <= 0 || atoi(optarg) > 18){
                    maxProcesses = 18;
                } else {
                    maxProcesses = atoi(optarg);
                }
                break;
            case 'l':
                logFile = optarg;
                break;
            case 't':
                maxRunTime = atoi(optarg);
                break;
            default:
                printf("there was an error with arguments");
                exit(1);
                break;
        }
    }

    //setup output file
    setupOutputFile();

    //display run parameters
    printf("Log file name: %s\n", logFile);
    fprintf(outputFile, "Log file name: %s\n", logFile);
    printf("Max run time: %d\n", maxRunTime);
    fprintf(outputFile, "Max run time: %d\n", maxRunTime);

    //Intilize various shared memory
    setupSharedClock();
    clockShmPtr[0] = 0;
    clockShmPtr[1] = 0;

    setupMsgQueue();

    pageQueue = createQueue(256);

    while(clockShmPtr[0] < 1){
    // while(1==1){
        if ((currentProcesses < maxProcesses)){
            createProcesses();
        }
        advanceTime();
        reciveMessages();
    }

    closeProgram();
}

void childClosedSignal(int sig){
    closeChild();
}

int closeChild(){
    pid_t closedChild = wait(NULL);
    if (closedChild > 0){
        int i;
        for(i = 0; i < 18; i++){
            if (openProcesses[i] == closedChild){
                openProcesses[i] = 0;
            }
        }
        currentProcesses--;
    }
    return closedChild;
}

void closeProgramSignal(int sig){
    closeProgram();
}

void closeProgram(){
    msgctl(msgQueueId, IPC_RMID, NULL);
    shmctl(clockShmId, IPC_RMID, NULL);
    // shmdt(clockShmPtr);
    fclose(outputFile);
    int i;
    for(i = 0; i < 18; i++){
        if (openProcesses[i] != 0){
            kill(openProcesses[i], SIGINT);
        }
    }
    printf("Exiting gracefully.\n");
    while (closeChild() > 0){}
    exit(0);
}

void setupOutputFile(){
    char* logFile = "logFile.txt";
    outputFile = fopen(logFile, "w");
    if (outputFile == NULL){
        printf("Failed to open output file.\n");
        fprintf(outputFile, "Failed to open output file.\n");
        closeProgram();
    }
}

void setupSharedClock(){
    key_t sharedClockKey;
    if (-1 != open(SHMNAME, O_CREAT, 0666)) {
        sharedClockKey = ftok(SHMNAME, CLOCKVAR);
    } else {
        printf("ftok error in parrent: setupSharedClock\n");
        printf("Error: %d\n", errno);
        exit(1);
    }

    clockShmId = shmget(sharedClockKey, sizeof(int)*2, IPC_CREAT | 0666);
    if (clockShmId < 0) {
        printf("shmget error in parrent: setupSharedClock\n");
        printf("Error: %d\n", errno);
        exit(1);
    }

    clockShmPtr = (int *) shmat(clockShmId, NULL, 0);
    if ((long) clockShmPtr == -1) {
        printf("shmat error in parrent: setupSharedClock\n");
        printf("Error: %d\n", errno);
        shmctl(clockShmId, IPC_RMID, NULL);
        exit(1);
    }
}

void advanceTime(){
    // clockShmPtr[0] += 1;
    // clockShmPtr[1] += rand() % 1000;
    clockShmPtr[1] += 1000;
    while (clockShmPtr[1] >= 1000000000){
        clockShmPtr[1] -= 1000000000;
        clockShmPtr[0]++;
        printf("%d:%d\n", clockShmPtr[0], clockShmPtr[1]);
        fprintf(outputFile, "%d:%d\n", clockShmPtr[0], clockShmPtr[1]);
    }
}

void createProcesses(){
    if (createNextProcessAt < 0){
        int randNumber = 1000;
        createNextProcessAt = randNumber + clockShmPtr[1];
        // printf("next process at %d seconds\n", createNextProcessAt);
    }

    if ((clockShmPtr[1] > createNextProcessAt) && (createNextProcessAt > 0)){
        int i;
        int openSpace;
        for(i = 0; i < 18; i++){
            if (openProcesses[i] == 0){
                openSpace = i;
                break;
            }
        }
        pid_t newForkPid;
        newForkPid = fork();
        if (newForkPid == 0){
            execlp("./worker","./worker", NULL);
            fprintf(stderr, "Failed to exec worker!\n");
            fprintf(outputFile, "Failed to exec worker!\n");
            exit(1);
        }
        openProcesses[openSpace] = newForkPid;
        createNextProcessAt = -1;
        printf("Execed child %d\n", openProcesses[openSpace]);
        fprintf(outputFile, "Execed child %d\n", openProcesses[openSpace]);
        currentProcesses++;
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

    msgQueueId = msgget(msgQueueKey, (IPC_CREAT | 0777));
    if (msgQueueId < 0) {
        printf("msgget error in parrent: setupSharedClock\n");
        printf("Error: %d\n", errno);
        exit(1);
    }
}

void reciveMessages(){
    int msgRecived  = msgrcv(msgQueueId, &message, sizeof(message), 1, IPC_NOWAIT);
    if (msgRecived == -1){
        return;
    }

    pid_t requestingPid = message.pid;
    int requestedAddress = message.location;
    int requestedPage = requestedAddress / 1024;
    char readOrWrite = message.readOrWrite;

    printf("Parent: %d requested read of Address %d / page %d at %d:%d\n", requestingPid, requestedAddress, requestedPage, clockShmPtr[0], clockShmPtr[1]);
    fprintf(outputFile, "Parent: %d requested read of Address %d / page %d at %d:%d\n", requestingPid, requestedAddress, requestedPage, clockShmPtr[0], clockShmPtr[1]);

    int j;
    int processLocation;
    for(j = 0; j < 18; j++){
        if (openProcesses[j] == requestingPid){
            processLocation = j;
            break;
        }
    }

    if (pages[processLocation][requestedPage] == 0){
        printf("Parrent: Page %d is not in a frame: pagefault.\n", requestedPage);
        fprintf(outputFile, "Parrent: Page %d is not in a frame: pagefault.\n", requestedPage);
        int j;
        int openFrame = -1;
        for(j = 0; j < 256; j++){
            if (openFrames[j] == 0){
                openFrame = j;
                printf("Parrent: Frame %d is open, page %d will be loaded here.\n", openFrame, requestedPage);
                fprintf(outputFile, "Parrent: Frame %d is open, page %d will be loaded here.\n", openFrame, requestedPage);
                break;
            }
        }
        if (openFrame < 0){
            openFrame = freeFrameSpace();
            printf("Parrent: Cleared frame %d, page %d will be loaded here.\n", openFrame, requestedPage);
            fprintf(outputFile, "Parrent: Cleared frame %d is open, page %d will be loaded here.\n", openFrame, requestedPage);
        }

        if (readOrWrite == 'w'){
            pages[processLocation][requestedPage] = openFrame;
            struct memoryBlock newBlock;
            newBlock.dirtyBit = 1;
            newBlock.pid = requestingPid;
            newBlock.readBit = 0;
            newBlock.refrenceBit = 1;
            newBlock.writeBit = 1;
            frames[openFrame] = newBlock;
            openFrames[openFrame] = 1;
            enqueue(pageQueue, openFrame);
            printf("Parrent: Process %d wrote to page %d: now in frame %d.\n", requestingPid, requestedPage, openFrame);
            fprintf(outputFile, "Parrent: Process %d wrote to page %d: now in frame %d.\n", requestingPid, requestedPage, openFrame);
        } else {
            pages[processLocation][requestedPage] = openFrame;
            struct memoryBlock newBlock;
            newBlock.dirtyBit = 1;
            newBlock.pid = requestingPid;
            newBlock.readBit = 1;
            newBlock.refrenceBit = 1;
            newBlock.writeBit = 0;
            frames[openFrame] = newBlock;
            openFrames[openFrame] = 1;
            enqueue(pageQueue, openFrame);
            printf("Parrent: Process %d read from page %d: now in frame %d.\n", requestingPid, requestedPage, openFrame);
            fprintf(outputFile, "Parrent: Process %d read from page %d: now in frame %d.\n", requestingPid, requestedPage, openFrame);
        }
    } else {
        if (readOrWrite == 'w'){
            int blockInFrame = pages[processLocation][requestedPage];
            struct memoryBlock block = frames[blockInFrame];
            block.dirtyBit = 1;
            block.writeBit = 1;
            block.refrenceBit = 1;
            printf("Parrent: Process %d wrote to page %d: already in frame %d.\n", requestingPid, requestedPage, blockInFrame);
            fprintf(outputFile, "Parrent: Process %d wrote to page %d: already in frame %d.\n", requestingPid, requestedPage, blockInFrame);
        } else {
            int blockInFrame = pages[processLocation][requestedPage];
            struct memoryBlock block = frames[blockInFrame];
            block.dirtyBit = 1;
            block.readBit = 1;
            block.refrenceBit = 1;
            printf("Parrent: Process %d read from page %d: already in frame %d.\n", requestingPid, requestedPage, blockInFrame);
            fprintf(outputFile, "Parrent: Process %d read from page %d: already in frame %d.\n", requestingPid, requestedPage, blockInFrame);
        }
    }

    message.mtype = requestingPid;

    int msgSent = msgsnd(msgQueueId, &message, sizeof(message), 0);
    if (msgSent < 0){
        printf("Parrent: failed to send message.\n");
    }
    printf("Parent: sent msg to child %d indicating requested action was taken.\n", requestingPid);
    fprintf(outputFile, "Parent: sent msg to child %d indicating requested action was taken.\n", requestingPid);
}

int freeFrameSpace(){
    for (;;){
        int cannidateForDeletion = front(pageQueue);

        if (frames[cannidateForDeletion].refrenceBit == 0 && frames[cannidateForDeletion].dirtyBit == 0){
            openFrames[cannidateForDeletion] = 0;
            dequeue(pageQueue);
            printf("Parrent: Dealocated frame %d.\n", cannidateForDeletion);
            fprintf(outputFile, "Parrent: Dealocated frame %d.\n", cannidateForDeletion);
            return cannidateForDeletion;
        } else {
            frames[cannidateForDeletion].refrenceBit = 0;
            frames[cannidateForDeletion].dirtyBit = 0;
            dequeue(pageQueue);
            enqueue(pageQueue, cannidateForDeletion);
            printf("Parrent: Frame %d granted second chance.\n", cannidateForDeletion);
            fprintf(outputFile, "Parrent: Frame %d granted second chance.\n", cannidateForDeletion);
        }
    }
}