/* Program: Seventh Assignment
   Author:  Morgan McGuire
   Date:    Oct. 25, 2021
   File:    JobDispatcher.c
   Compile: gcc -o JobDispatcher JobDispatcher.c -Wall -pthread
   Run:     ./JobDispatcher

   This program constructs a doubly-linked list with nodes known
   as jobs. It contains functions to append jobs to the list, to insert
   jobs into the list, to delete the first job in the list, and to print
   the list (in either increasing or decreasing order). It accepts input
   for jobs and executes the jobs at the correct time using threads. The
   jobs contain variables for storing information about the command
   issued, the number of parameters for the command, the command's
   submission time, and the command's start time.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#define DELETE '-'       // Character constant for delete operation
#define INSERT '+'       // Character constant for insert operation
#define QUIT '!'         // Character constant for terminating program
#define PRINT 'p'        // Character constant for printing job list

struct JOB {
   char command[5][25];  // domain = { [a-zA-Z][a-zA-Z0-9_\.]*
   long submissionTime;  // domain = { positive long integer }
   int numOfParameters;  // domain = { positive integer }
   int startTime;        // domain = { positive integer }
   struct JOB *prev;     // points to prev node. NULL if this is first node
   struct JOB *next;     // points to next node. NULL if this is last node
};
typedef struct JOB Job;

struct LIST {
   int numOfJobs;        // number of jobs in the list
   Job *firstJob;        // points to first job. NULL if list empty
   Job *lastJob;         // points to last job. NULL if list empty
};
typedef struct LIST List;

struct ARGS {
   char operation;       // operation to be carried out
   List *listPtr;        // list that holds jobs
};  
typedef struct ARGS Args;

List *createList();
Job  *createJob();
void appendJob(List *listPtr, Job *jobPtr);
void insertOrdered(List *listPtr, Job *jobPtr);
Job *deleteFirstJob(List *listPtr);
void printList(List *listPtr);
void printForward(List *listPtr);
void printBackward(List *listPtr);
void insertOnly(List *listPtr, Job *jobPtr);
void insertFirst(List *listPtr, Job *jobPtr);
void insertBetween(List *listPtr, Job *jobPtr, Job *ahead);
void printJobInfo(Job *jobPtr);
void freeListJobs(List *listPtr);
void prepareCmdParam(char **argv, Job *jobPtr);
void *schedule(void *args);
void *dispatch(void *args);
void *execute(void *arg);

/* Main function that accepts user input for inserting jobs, deleting
   jobs, and terminating program. Creates dispatcher thread which
   handles execution of jobs at proper time. 
*/
int main() {
   List *listPtr = createList();
   Args *args = (Args *)malloc(sizeof(Args));
   args->listPtr = listPtr;
   long curSysTime;
   Job *jobPtr;

   pthread_t dispatcher;
   pthread_create(&dispatcher, NULL, dispatch,(void *) args);
  
   // While process should not terminate
   while(args->operation != QUIT) {

     scanf("%c", &args->operation);
     curSysTime = time(NULL);

     // If input was to insert job
     if(args->operation == INSERT) {
        insertOrdered(args->listPtr, createJob());
     }
     // If input was to delete first job
     else if(args->operation == DELETE) {
        printf("Current system time: %ld\n", curSysTime);
        jobPtr = (deleteFirstJob(args->listPtr));
        printJobInfo(jobPtr);
        free(jobPtr);
     }
     // If input was to print the job queue
     else if(args->operation == PRINT) {
        printList(args->listPtr);
     }
   } 
   free(args);                         
   free(listPtr);                         
}

/* Function deletes jobs from list at the time which
   they should be executed. It executes those jobs
   by calling fork and execvp. Used by dispatcher thread.
*/
void *dispatch(void *args) {
   Args *arg = (Args *) args; // Holds pointer to list and operation
   Job *jobPtr;
   long curSysTime;
   
   // While process should not terminate
   while(arg->operation != QUIT) {    
     jobPtr = arg->listPtr->firstJob;
     curSysTime = time(NULL);   
   
     // If first job exists and it is the correct time to execute it
     if(jobPtr != NULL &&
        curSysTime >= jobPtr->submissionTime + jobPtr->startTime) {
           jobPtr = deleteFirstJob(arg->listPtr);
           pthread_t executor;
           pthread_create(&executor, NULL, execute, (void *) jobPtr);
     }
     // Wait for right time to execute job
     else {
        sleep(1);
     }
   }
   return NULL;
}

/* Function executes job by using system calls for fork and execvp
   and then prints job information and execution status.
*/
void *execute(void *arg) {
   Job *jobPtr = (Job *) arg;
   long curSysTime = time(NULL);
   char *argv[5];
   int status;
   int cpid = fork();

   // Child process executes and restarts into job process
   if(cpid == 0) {
      prepareCmdParam(argv, jobPtr);
      execvp(argv[0], argv);
      exit(1);  // Ensures child process terminates
   }
   // Parent process executes and waits for child to finish execution
   else {
      waitpid(cpid, &status, 0);
   }
   printf("Job Deleted: \n");
   printJobInfo(jobPtr);
   printf("Current system time: %ld\n", curSysTime);
   printf("Status: %d\n\n", status);
   free(jobPtr);
   return NULL;
}

/* Function prepares arguments for call to execvp
   system call.
*/
void prepareCmdParam(char **argv, Job *jobPtr) {
   
   // Places job's command line parameters into argument array
   for(int i = 0; i < jobPtr->numOfParameters; i++) {
              argv[i] = jobPtr->command[i];
   }
   argv[jobPtr->numOfParameters] = NULL; // Signifies end of arguments

}

/* Deallocates jobs in list provided during function call.
*/
void freeListJobs(List *listPtr) {
   Job *temp = listPtr-> firstJob;  // Pointer used to iterate through list
   Job *freed;                      // Pointer used to free jobs

   //Iterates through list to free jobs
   while(temp != NULL) {
      freed = temp;
      temp = temp->next;
      free(freed);
   }
}

/* Creates list with variables for number of jobs in the
   list, first job, and last job.
*/
List *createList() {
   List *listPtr = (List*)malloc(sizeof(List));    
   listPtr->numOfJobs = 0;         
   listPtr->firstJob = NULL;                     
   listPtr->lastJob = NULL;                      
   return listPtr;
}

/* Creates job by scanning for user input and assigning that input
   to command-line parameters  and  startTime. Submission time of
   job is also stored using time function.
*/
Job *createJob() {
   Job *job = (Job*)malloc(sizeof(Job));  
   scanf("%d", &job->numOfParameters);

   // Loop gathers input for parameters                                         
   for(int i = 0; i < job->numOfParameters; i++) { 
      scanf("%s",job->command[i]);
   }   

   scanf("%d", &job->startTime);
   time_t seconds;                                  
   job->submissionTime = time(&seconds); // Time since Unix Epoch 
   job->prev = NULL;                                                   
   job->next = NULL;                                                           
   return job;
}

/* Function inserts job into list so it is the only job
   in the list. This means job is both the first and last
   job in the list.
*/
void insertOnly(List *listPtr, Job *jobPtr) {
   listPtr->firstJob = jobPtr;
   listPtr->lastJob = jobPtr;
}

/* Function inserts job into the list as the first job in the
   list.
*/
void insertFirst(List *listPtr, Job *jobPtr) {
   jobPtr->next = listPtr->firstJob;
   listPtr->firstJob->prev = jobPtr;
   listPtr->firstJob = jobPtr;
}

/* Function inserts job in between two jobs in the list.
*/
void insertBetween(List *listPtr, Job *jobPtr, Job *ahead) {
   jobPtr->next = ahead;
   jobPtr->prev = ahead->prev;
   ahead->prev->next = jobPtr;
   ahead->prev = jobPtr;
}

/* Function inserts job into the list as the last job in the
   list.
*/ 
void insertLast(List *listPtr, Job *jobPtr) {
   listPtr->lastJob->next = jobPtr;
   jobPtr->prev = listPtr->lastJob;
   listPtr->lastJob = jobPtr;
}

/* Function inserts jobs into list in non-decreasing order based
   on value of submissionTime + startTime.
*/
void insertOrdered(List *listPtr, Job *jobPtr) {
     // If list is empty
    if(listPtr->numOfJobs == 0) {                      
      insertOnly(listPtr, jobPtr);
   }

   // If list is not empty
   else {                 
      Job *temp = listPtr->firstJob;           
      int tempValue = temp->submissionTime + temp->startTime;         
      int insertValue = jobPtr->submissionTime + jobPtr->startTime;   

      // If new job should be first in the list
      if(insertValue < tempValue) {              
         insertFirst(listPtr, jobPtr);
      }

      //  If new job's location is after first job's
      else {                                                      
     
        // While value is still larger and temp is not last job 
        while(insertValue > tempValue && temp->next != NULL) {                  
           temp = temp->next;                                       
           tempValue = temp->submissionTime + temp->startTime;       
        }
       
        // Location is not first or last in list
        if(insertValue <= tempValue) {           
           insertBetween(listPtr, jobPtr, temp);                           
        }

        // If new job should be last in the last
        else {                                                 
           insertLast(listPtr, jobPtr);                       
        }
     }
   }
   listPtr->numOfJobs++;                                             
}

/* Function deletes and returns first job in the list if at least one job
   exists in the list. Otherwise, returns NULL.
*/
Job *deleteFirstJob(List *listPtr) {
   // If list is empty
   if(listPtr->numOfJobs == 0) {                           
      return NULL;
   }
   
   // If list is not empty
   else {                                   
      Job *deleted = listPtr->firstJob;             // First job deleted
      listPtr->firstJob = listPtr->firstJob->next;  // First job updates

      // If deleted job was not NULL
      if(listPtr->firstJob != NULL) { 
         listPtr->firstJob->prev = NULL;  // Detaches deleted job from list
      }  
      listPtr->numOfJobs--;                                        
     
      return deleted;
   }
}

/* Appends job to end of the list and updates
   the list varaible numOfJobs.
*/
void appendJob(List *listPtr, Job *jobPtr) {
   // If list is empty
   if(listPtr->numOfJobs == 0) {         
      insertOnly(listPtr, jobPtr);
   }

   // If list is not empty
   else {                 
      insertLast(listPtr, jobPtr);
   }

   listPtr->numOfJobs++;                       
}

/* Prints list of jobs in increasing order by
   calling function printForward.
*/
void printList(List *listPtr) {
   printf("# of jobs: %d\n", listPtr->numOfJobs);       
   printForward(listPtr);
   printf("\n");      
}

/* Prints list of jobs and their information in
   increasing order (from begginging to end of
   the list).
*/
void printForward(List *listPtr) {
   Job *temp = listPtr->firstJob;   // Pointer used to iterate through list

   // Loop iterates through job list
   for(int i = 0; i < listPtr->numOfJobs; i++) {
      printf("Job %d:\n", i+1);
      printJobInfo(temp);

      temp = temp->next;    // Move to the next job in list
   }
}

/* Prints list of jobs and their information in
   decreasing order (from end to beginning of
   the list).
*/
void printBackward(List *listPtr) {
   Job *temp = listPtr->lastJob;    // Pointer used to iterate through list
   
   // Loop iterates through job list
   for(int i = 0; i < listPtr->numOfJobs; i++) {
      printf("Job %d:\n", i+1);
      printJobInfo(temp);

      temp = temp->prev;   // Move to the previous job in list
   }
}

/* Prints a job's program name, command-line parameters,
   submission time, and start time.
*/
void printJobInfo(Job *jobPtr) {
   printf("Program Name: ");

   // Loop prints job's command-line parameters
   for(int i = 0; i < jobPtr->numOfParameters; i++) {
      printf("%s ", jobPtr->command[i]);
   }
   printf("\n");
   printf("Submission Time: %ld\n", jobPtr->submissionTime);
   printf("Start Time: %d\n", jobPtr->startTime);
}
