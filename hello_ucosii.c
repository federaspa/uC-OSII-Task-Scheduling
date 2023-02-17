#include <stdio.h>
#include "includes.h"
#include "system.h"
#include <unistd.h>
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_timer_regs.h"
#include "alt_types.h"
#include <ucos_ii.h>


/* Definition of Task Stacks */
#define   TASK_STACKSIZE       2048
OS_STK    task1_stk[TASK_STACKSIZE];
OS_STK    task2_stk[TASK_STACKSIZE];
OS_STK    task3_stk[TASK_STACKSIZE];
OS_STK    task4_stk[TASK_STACKSIZE];
OS_STK    task5_stk[TASK_STACKSIZE];

/* Definition of Task Priorities */
#define TASK1_PRIORITY      1
#define TASK2_PRIORITY      2
#define TASK3_PRIORITY      3
#define TASK4_PRIORITY      4
#define TASK5_PRIORITY      5

#define NB_TASKS            5

OS_EVENT *PostButton;
OS_EVENT *PostSwitch;
OS_EVENT *PostTimerRandom;
OS_EVENT *PostTimerPlayer;
OS_EVENT *PostAvg;

TASK_USER_DATA OurTaskUserData[NB_TASKS];
volatile int edge_capture;

#define TIMEOUT  10

int LUT[10] = {0x40, 0x79, 0x24, 0x30, 0x19, 0x12, 0x02, 0x78, 0x00, 0x10};
int LUT_HEX[6] = {HEX0_BASE,HEX1_BASE,HEX2_BASE,HEX3_BASE,HEX4_BASE,HEX5_BASE};

//our function to compute the power
int powOur(int base, int exp){
	int res = 1;
	for(int i=0; i<exp;i++) res*=base;
	return res;

}

//to switch off all the 7 segments
void resetBCD(){
	for(int i=0; i<6;i++) {
		IOWR_ALTERA_AVALON_PIO_DATA(LUT_HEX[i],0x7F);
	}
}

//our function to print the number split
void printBCD(int num){
	  int j = num;
	  int t;

	  //here we print the milliseconds
	  for (int k=3; k>=0; k--){
		  t = j/powOur(10,k);
		  IOWR_ALTERA_AVALON_PIO_DATA(LUT_HEX[k],LUT[t]);
		  j = j-powOur(10,k)*t;
	  }
}

//interrupt handling for the buttons
static void handle_button_interrupts(void* context, alt_u32 id)
{
	volatile int* edge_capture_ptr = (volatile int*) context;
	*edge_capture_ptr = IORD_ALTERA_AVALON_PIO_EDGE_CAP(KEY_BASE);
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP (KEY_BASE, 0);
	IORD_ALTERA_AVALON_PIO_EDGE_CAP (KEY_BASE);
}


static void init_button_pio()
{
	void* edge_capture_ptr = (void*) &edge_capture;
	/* Enable all 4 button interrupts. */
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK (KEY_BASE, 0xf);
	/* Reset the edge capture register. */
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP (KEY_BASE, 0x0);
	/* Register the ISR. */
	alt_irq_register( KEY_IRQ, edge_capture_ptr, handle_button_interrupts );
}

int countOnes(int decimal) {
    int count = 0;
    while (decimal) {
        if (decimal & 1) {
            count++;
        }
        decimal >>= 1;
    }
    return count;
}


void OSTaskSwHook(void)
{
	INT16U taskStopTimestamp, time;
	TASK_USER_DATA *puser;
	taskStopTimestamp = OSTimeGet();
	time =(taskStopTimestamp - taskStartTimestamp) / (OS_TICKS_PER_SEC / 1000); // in ms
	puser = OSTCBCur->OSTCBExtPtr;
	if (puser != (TASK_USER_DATA *)0) {
		puser->TaskCtr++;
		puser->TaskExecTime = time;
		puser->TaskTotExecTime += time;
	}
	taskStartTimestamp = OSTimeGet();
}
void OSInitHookBegin(void)
{
	 OSTmrCtr = 0;
	 taskStartTimestamp = OSTimeGet();
}

void OSTimeTickHook (void)
{
	OSTmrCtr++;
	if (OSTmrCtr >= (OS_TICKS_PER_SEC / OS_TMR_CFG_TICKS_PER_SEC)) {
		OSTmrCtr = 0;
		OSTmrSignal();
	}
}

void task1(void* pdata)
{
    INT8U err;
    //the variable containing the average, to keep updated once a new one is computed
    int avg = 0; 
    for (;;) {
      
      	//posting the button reading from the interrupt
		OSMboxPost(PostButton, &edge_capture);
      
        //posting the number of switches raised
    	int data = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE);
    	int ones = countOnes(data);
    	OSMboxPost(PostSwitch, &ones);
		
      	//if button 2 is pressed, we also show the average in the display
    	if (edge_capture == 2) {
    		edge_capture==0;
    		printf("\n Updated AVG!\n");
    		printBCD(avg);
    	}
		
       //once we get the AVG, we save it in the local variable
    	void* result = OSMboxPend(PostAvg, TIMEOUT, &err);
		if (result != NULL){
			avg = *(int *)result;

		}

    }

}



void task2(void* pdata)
{
	INT8U err;
	srand(time(NULL));

	for (;;) {
      
      // we read the switches and set the leds (we do it here cause otherwise the  
      // control of the switches overlaps with other tasks)
    	int data = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE);
    	IOWR_ALTERA_AVALON_PIO_DATA(LEDS_BASE,data);

		//read the button mailbox
		void *result = OSMboxPend(PostButton, TIMEOUT, &err);
		int button = *(int *)result;

		//if we receive 1 we start
		if (button==1){ 
			  edge_capture=0;

          	//here we filter the null pointers in order to retrieve the 
            //scalar corresponding to the switches up
			  do{
				  result = OSMboxPend(PostSwitch, TIMEOUT, &err);
			  }
			  while (result==NULL) ;
          
			  int ones = *(int *)result;
			  printf("Value received! ");

			  int max_time = ones+1; //int this wy we avoid to have a waiting time = 0
			  int random = rand() % max_time +1;
			  printf("random number: %i\n", random);

			  int counter=0;
			  while (counter/(1000000/200000) < random){
				  //here we switch between the 2 configurations (341 and 682)
				  IOWR_ALTERA_AVALON_PIO_DATA(LEDS_BASE, 341*powOur(2,counter%2==0) );
				  counter++;
				  usleep(200000);
			  }

			  //turns on the leds and takes the starting time
			  IOWR_ALTERA_AVALON_PIO_DATA(LEDS_BASE,1023);
			  int start_time = OSTimeGet();

			  //Send the starting time
			  OSMboxPost(PostTimerRandom, &start_time);
			  printf("\ninitial time dent: %i\n", start_time); 

		}

	}
}

void task3(void* pdata)
{
	INT8U err;

	while(1){
		void *result = OSMboxPend(PostButton, TIMEOUT, &err);
		int button = *(int *)result;
      
    //simply waits for the 4th button to be pressed
		if (button==8){
			edge_capture=0;
			
          	// Takes the timestamp and posts it
			int stop_time = OSTimeGet();
			OSMboxPost(PostTimerPlayer, &stop_time);
			printf("\nfinal time: %i\n", stop_time);
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_BASE,0);
		}
	}


}

void task4(void* pdata)
{
	INT8U err;
	int sum = 0; //To keep track of the average
	int try = 0;
	void* result1;
	void* result2;
	int timeOld = 0;
	
   /* 
   In this task, we need to continuosly count the average once the game is
   played. We need both starting time and stop time to be received. To do so,
   we wait for the stop time to arrive, and once we receive that we know that
   in the other mailbox we will find the value corresponding the start time.
   In this way, we only read the starting time mailbox once we receive the stop time.
   */
  
	while(1){
		
        //We wait for the stop time
		do{
			result1 = OSMboxPend(PostTimerRandom, TIMEOUT, &err);
		}while (result1==NULL) ;
		
        //We read the start time
		do{
			result2 = OSMboxPend(PostTimerPlayer, TIMEOUT, &err);
		}while (result2==NULL) ;

		int timeStart = *(int *)result1;
		int timeStop = *(int *)result2;

		if (timeOld!=timeStop){ //once we get a new stop time...
          
				printf("\nTime to do calculations!!\n");
				long int delay = (timeStop - timeStart) / (OS_TICKS_PER_SEC / 1000);
				printf("time delay: %i\n", delay);
				printBCD(delay);
  				
            // In this task we keep track of the average and we print 
            // it on the PostAvg mailbox
				sum+=delay;
				try++;
				int avg = (int)(sum/try);
				OSMboxPost(PostAvg, &avg);

		}
		timeOld=timeStop;

	}
}

void task5(void* pdata)
{
	INT8U err;

	while(1){
		  OSTimeDlyHMSM(0, 0, 5, 0);
		  int i;
		  for (i=0; i<NB_TASKS;i++){
            printf("n. %i: TaskCtr:%i, TaskExecTime:%i, TaskTotExecTime:%i\n",
                  i, OurTaskUserData[i].TaskCtr,OurTaskUserData[i].TaskExecTime,
                   OurTaskUserData[i].TaskTotExecTime);
          }

	}
}

/* The main function creates four tasks and starts multi-tasking */
int main(void)
{
  init_button_pio();
  resetBCD();
  
  //here we create the different mailboxes
  PostButton = OSMboxCreate(NULL);
  PostSwitch = OSMboxCreate(NULL);
  PostTimerRandom = OSMboxCreate(NULL);
  PostTimerPlayer = OSMboxCreate(NULL);
  PostAvg = OSMboxCreate(NULL);
  
  //we instanciate the tasks, each one with its priority and OS_STK
  OSTaskCreateExt(task1,
                  NULL,
                  (void *)&task1_stk[TASK_STACKSIZE-1],
                  TASK1_PRIORITY,
                  TASK1_PRIORITY,
                  task1_stk,
                  TASK_STACKSIZE,
                  &OurTaskUserData[0],
                  OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
              
               
  OSTaskCreateExt(task2,
                  NULL,
                  (void *)&task2_stk[TASK_STACKSIZE-1],
                  TASK2_PRIORITY,
                  TASK2_PRIORITY,
                  task2_stk,
                  TASK_STACKSIZE,
                  &OurTaskUserData[1],
                  OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);

  OSTaskCreateExt(task3,
                  NULL,
                  (void *)&task3_stk[TASK_STACKSIZE-1],
                  TASK3_PRIORITY,
                  TASK3_PRIORITY,
                  task3_stk,
                  TASK_STACKSIZE,
                  &OurTaskUserData[2],
                  OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);

  OSTaskCreateExt(task4,
                   NULL,
                   (void *)&task4_stk[TASK_STACKSIZE-1],
                   TASK4_PRIORITY,
                   TASK4_PRIORITY,
                   task4_stk,
                   TASK_STACKSIZE,
                   &OurTaskUserData[3],
                   OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);

  OSTaskCreateExt(task5,
                   NULL,
                   (void *)&task5_stk[TASK_STACKSIZE-1],
                   TASK5_PRIORITY,
                   TASK5_PRIORITY,
                   task5_stk,
                   TASK_STACKSIZE,
                   &OurTaskUserData[4],
                   OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);


  OSStart();
  return 0;
}