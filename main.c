//For understanding ucontext data structure the following link was very helpful:
//http://nitish712.blogspot.com.tr/2012/10/thread-library-using-context-switching.html
//Man7 pages for signal alarm and linux.die pages for make and swap context functions were also used:
//http://man7.org/linux/man-pages/man2/signal.2.html
//http://man7.org/linux/man-pages/man2/alarm.2.html
//https://linux.die.net/man/3/swapcontext
//For scheduling this entry from stackoverflow was very helpful for understanding do and don'ts of signal handlers:
//https://stackoverflow.com/questions/15398556/switching-thread-contexts-with-sigalrm

#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define STACK_SIZE 1024*64

typedef struct ThreadInfo //Struct to keep the state and context
{
	ucontext_t context;
	int state; //Empty:0, Ready:1, Running:2, Finished:3 
}ThreadInfo;

struct ThreadInfo ActiveThreads[5]; //Struct array

int threadIndex = 0; //Thread index is the current active thread
int temp; //Teis used to keep the old value of thread index while scheduling
int next; //Next is the next thread to run
int contextSwitch = 0; //Context switch signal 
unsigned int remainingTime;

void Printer(int n,int x); //Function for threads to print their outputs
void CreateThread(int index, int number, int id); //Context creation function
void Scheduler(int signum); //Scheduler function to be used as signal handler
int ReorderThreads(void); //When we insert new things to keep execution order correct we move the active threads compacting empty places

int main (int argc, char * const argv[])
{	
	int argument,index,inserted; 

	printf("EE442 - HW#3 - 2030393 \nSchedule "); //Print the information
	for (argument=1;argument<argc;argument++)
	{
		printf("%s ",argv[argument]);
	}	
	printf("\n\n");

	ActiveThreads[0].state = 2; //Mark the thread as running
	signal(SIGALRM, Scheduler); //Set up the scheduler function as SIGALRM signal handler
	alarm(1); //Set the first alarm manually 
	
	for (argument=1;argument<argc; argument++) //Create each thread one by one
	{	
		inserted = 0;
		while (inserted == 0) //When scheduler is run it tries to insert the next argument by checking if there is any empty spot
		{	
			if (contextSwitch == 1) //While checking if we receive a context switch signal our time is up so we  swap context
			{
				contextSwitch = 0;
				swapcontext(&(ActiveThreads[0].context),&(ActiveThreads[next].context));
			}
			for (index=1;index<5;index++)
			{	
				if (ActiveThreads[index].state == 0) //Check for a empty spot on the array
				{	
					index=ReorderThreads(); //If there is one move the gaps to the end and place the new thread at the left most empty state, here returned number is the first empty state
					CreateThread(index,strtol(argv[argument],NULL,10),argument); //Pass the index to place, the number to count to and tab number information to creation function
					inserted = 1; //Raise inserted flag
					break; 
				}
			}
		}
	}

	ActiveThreads[0].state = 3; //All the threads are created, state becomes finished 
	raise(SIGALRM); //Raise the alarm signal manually to indicate completion 
	// Here there is no while loop to wait because raising the signal is an atomic instruction 
	contextSwitch = 0; //Scheduler raises the context switch flag and next contains the thread to be run
	swapcontext(&(ActiveThreads[0].context),&(ActiveThreads[next].context)); //Swap context is used but we could also use set context since we will never schedule main again
	while (1)
	{
	}
	return 0;
}

void Printer(int n, int x) 
{	
	int i;  
	for (i=0;i<n;i++)
	{	
		printf("Thread #%d -> %d \n",x,i); 
		usleep(100000);
		if (contextSwitch == 1) //If context switch flag is raised swap context
		{	
			contextSwitch = 0;
			if (next != temp) //If the next thread to schedule is different swap context else no need for a context switch
			{
				printf("\n"); 
				swapcontext(&(ActiveThreads[temp].context),&(ActiveThreads[next].context));
			}
		}
	}
	printf("\n");
	ActiveThreads[threadIndex].state = 3; //The thread is finished so mark it and raise the signal
	raise(SIGALRM);
	free(ActiveThreads[temp].context.uc_stack.ss_sp);//Freeing of stack is done here because doing extensive actions on signal handler might cause seg faults
	contextSwitch = 0;	
	setcontext(&(ActiveThreads[next].context));
	
}

void CreateThread(int index, int number, int id) //Standard creation sequence, we also mark the thread as ready 
{
	getcontext(&(ActiveThreads[index].context)); 
	ActiveThreads[index].context.uc_link = &(ActiveThreads[0].context);
	ActiveThreads[index].context.uc_stack.ss_sp = malloc(STACK_SIZE);
	ActiveThreads[index].context.uc_stack.ss_size = STACK_SIZE;
	ActiveThreads[index].state = 1;
	makecontext(&(ActiveThreads[index].context), (void (*)(void))Printer, 2, number, id);		
}

void Scheduler(int signum) 
{	
	remainingTime = alarm(0); //When alarm is invoked again it returns the time to finish from the last time
	if (remainingTime != 0) //We understand if it is non zero the signal was raised manually which means the last active thread finished
	{	
		if (threadIndex != 0) //If the finished thread is not main mark it as empty
		{	
			ActiveThreads[threadIndex].state = 0;
		}
	} 
	else ActiveThreads[threadIndex].state = 1; //If the signal was raised because of the alarm set the thread as ready because it still has execution to be done
	if (threadIndex == 4) next = 0; //If we are at the end of the array start from beginning 
	else next = threadIndex + 1; //Else get the next element
	for (;next<5;next++) //Look for the next thread to activate 
	{		
		if (ActiveThreads[next].state == 1) //If a thread is ready mark it as running 
		{
			ActiveThreads[next].state = 2;
			break;
		}
		if ((next == threadIndex) && (ActiveThreads[next].state != 1)) //If we reach traverse the array and find the same thread and it is finished or empty we are done
		{
			free(ActiveThreads[0].context.uc_stack.ss_sp); //Free the stack 
			ActiveThreads[0].context.uc_stack.ss_sp = NULL;	
			exit(1); //Exit
		
		if (next == 4) next = -1; //When traversing the array if we are at entry 4 and we didn't find a ready thread we start from 0 
	}
	temp = threadIndex; //If a thread is found keep the index at temp
	threadIndex = next; //Thread index becomes next 
	alarm(1); //Set the next alarm
	contextSwitch = 1; //Raise the signal
}

int ReorderThreads(void) //This is a simple bubble sort algorithm to move the empty spots to the end of the array
{	int i,j,count = 1;
	for(i=1;i<4;i++)
	{	
		for(j=1;j<4-i;j++)
		{
			if(ActiveThreads[j].state == 0 && (ActiveThreads[j+1].state != 0)) //If the next entry has a non empty thread and this entry is empty
			{
				ActiveThreads[j].state = ActiveThreads[j+1].state; //Swap the states
				ActiveThreads[j+1].state = 0;
				ActiveThreads[j].context.uc_stack.ss_sp = ActiveThreads[j+1].context.uc_stack.ss_sp; //Swap the stack pointers
				ActiveThreads[j+1].context.uc_stack.ss_sp = NULL;
				ActiveThreads[j].context.uc_mcontext = ActiveThreads[j+1].context.uc_mcontext; //Copy the context
			}
		}
	}
	for(i=1;i<5;i++) //Find the count of non empty entries, 1+number of non empty entries give the first empty space on the array
	{
		if(ActiveThreads[i].state == 1)
			count++;
	}
	return count;
}