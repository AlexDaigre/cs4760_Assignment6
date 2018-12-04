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
int maxProcesses;

struct memoryBlock {
    int inFrame;
    int refrenceBit;
    int dirtyBit;
    int readBit;
    int writeBit;
};

int pages[18][32];
struct memoryBlock frames[256];


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

    while(clockShmPtr[0] < 80){
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
    shmctl(clockShmId, IPC_RMID, NULL);
    // shmdt(clockShmPtr);
    fclose(outputFile);
    msgctl(msgQueueId, IPC_RMID, NULL);
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
    printf("Execed child %d\n", newForkPid);
    fprintf(outputFile, "Execed child %d\n", newForkPid);
    currentProcesses++;
}

void setupMsgQueue(){
    key_t msgQueueKey;
    if (-1 != open(QUEUENAME, O_CREAT, 0666)) {
        msgQueueKey = ftok(SHMNAME, QUEUEVAR);
    } else {
        printf("ftok error in parrent: setupMsgQueue\n");
        printf("Error: %d\n", errno);
        exit(1);
    }

    msgQueueId = msgget(msgQueueKey, (IPC_CREAT | 0666));
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

    printf("Parent: Recived msg from child: %d\n", requestingPid);

    int j;
    int processLocation;
    for(j = 0; j < 18; j++){
        if (openProcesses[j] == requestingPid){
            processLocation = j;
            break;
        }
    }

    message.mtype = requestingPid;

    int msgSent = msgsnd(msgQueueId, &message, sizeof(message), 0);
    if (msgSent < 0){
        printf("Parrent: failed to send message.\n");
    }
    printf("Parent: sent msg to child %d\n", requestingPid);
}