# μC/OS-II Task Scheduling

The aim of this project is to implement a Reflex testing game using the task scheduling functions of μC/OS-II. \
Please refer to [this repository](https://github.com/federaspa/Reflex-game "Reflex game repo") for the basic implementation and details about the game.\
Note that this type of game does not make full use of the real-time capabilities of the OS, but provided us a solid scheleton on which to experiment.

## Let us recall how a task is created in μC/OS-II

Within μC/OS-II there are two functions to create a task:

-   **OSTaskCreate** is the standard function, with 4 parameters;

| Parameter                         	| Type   	|
|------------------------------------------------	|:--------:	|
| (task)(void\*pd): pointer on the task function 	| void *   	|
| pdata: payload pointer used at the creation   	| void *   	|
| ptos: pointer on top of stack                 	| OS_STK * 	|
| prio: priority of the task (limited to 64)     	| INT8U  	|

-   **OSTaskCreateExt** is the extended version, with 9 parameters.

|  Parameter                                                          	| Type   	|
|---------------------------------------------------------------------------------	|:--------:	|
| (task)(void\*pd): pointer on the task function                                  	| void *   	|
| pdata: payload pointer used at the creation                                    	| void *   	|
| ptos: pointer on top of stack                                                  	| OS_STK * 	|
| prio: priority of the task (limited to 64)                                      	| INT8U  	|
| id: task ID. Extension of the previous limitation to 64 values. If NULL, id = prio  	| INT16U 	|
| pbos: Bottom of stack                                                          	| OS_STK * 	|
| stk_size: stack size (for stack checking)                                      	| INT32U 	|
| pext: user data extension                                                      	| void   	|
| opt: uCOS_ii.h contains the list of options. Each constant is a binary flag.    	| INT16U * 	|



# A first code with the RTOS

## Let us create two communicating tasks
Our implementation uses MailBox.

An alternative to a MailBox would be a MessageQueue.

We first would need to instantiate the pointer to the queue and define its size, to be passed to `OSQCreate`.

    #define SIZE 100  
    OS_EVENT *MessageQueuePtr;

    MessageQueuePtr = OSQCreate(NULL, SIZE);

Posting is very similar to the MailBox, it is just a matter of changing the syntax.

    OSQPost(MessageQueuePtr, (int)counter);

With OSQPend we can retrieve the value directly from the queue, without the need for a pointer

    int result = (int)OSQPend(MessageQueuePtr, TIMEOUT, &err);

## First experiments

We experimented with the time delays in order to understand how the scheduling timing works., by setting
$$D1=kD2$$
Where $D1$ is the delay of the first task, $D2$ the delay for the second task and $k$ is a multiplicative factor taking various values.
### MailBox
We noticed that when we set $k = 5$, meaning $D1 > D2$, the counter
on the BCD increased by 5 at a time. \
This happened because while the first task was running the second one had to wait for its (bigger) delay to expire, at which point the timer already increased by 5. 
If instead
we kept  $D1 < D2$, the counter updated by 1 at a time.

### MessageQueue
We noticed that when keeping $k = 5$, the counter
was increasing by 1 at time, but 5 times slower. This happened because we were reading the first element of the queue every 5 seconds.\
Meanwhile, the queue was
cumulating values, and since we were extracting much slower than we were inserting, if we waited a long time the counter started increasing by 5 at a time, as the new values were overwriting the old ones once the queue was full.

# Reflex game with multitasking


## Tasks communication diagram 

We wanted to exploit the potential of the RTOS system, so we created 4 tasks in order to split and parallelize the total computational power.\
Each task has simple assignments, such as waiting for the pressing of a button or computing the average once a new value is received.

# Monitoring the execution

## Let us add some monitoring functions to our code

In order to implement the monitoring we had to 

Note: We managed to configure everything correctly and to implement and test the readings of the stack size and the CPU utilization rate, but we couldn't test the Hooking functions due to time constraints with our board's usage.

To inspect the stack size and processor utilization rate, we created a new low priority task to get some information about our other tasks, such as `OSFree` and `OSUsed`. We started by adding to our code

    OS_STK    task5_stk[TASK_STACKSIZE];
    #define TASK5_PRIORITY      5


In order to allow then the execution of `OS_TaskStat()`, we had to set the constant `OS_TASK_STAT_EN` to 1 in `OS_CFG.H`, which can be found in the `UCOSII/INC` subfolder of the `BSP` project folder:

    #define OS_TASK_STAT_EN      1

When creating this task, we had to pass additional optional parameters:

      OSTaskCreateExt(task5,
                       NULL,
                       (void *)&task5_stk[TASK_STACKSIZE-1],
                       TASK5_PRIORITY,
                       TASK5_PRIORITY,
                       task5_stk,
                       TASK_STACKSIZE,
                       NULL,
                       OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);

To access the information we defined the following data structure:

    typedef struct os_stk_data {
        INT32U  OSFree;                    /* Number of free bytes on the stack       */
        INT32U  OSUsed;                    /* Number of bytes used on the stack       */
    } OS_STK_DATA;

After reading the values in the pointer to the `os_stk_data` structure we printed them for every task:

    void task5(void* pdata)
    {
        INT8U err;

        while(1){
              OSTimeDlyHMSM(0, 0, 5, 0);

              OS_STK_DATA *p1;
              OSTaskStkChk(TASK1_PRIORITY, p1);
              printf("1 - first: %i, second: %i\n",p1->OSFree,p1->OSUsed);
              OS_STK_DATA *p2;
              OSTaskStkChk(TASK2_PRIORITY, p2);
              printf("2 - first: %i, second: %i\n",p2->OSFree,p2->OSUsed);
              OS_STK_DATA *p3;
              OSTaskStkChk(TASK3_PRIORITY, p3);
              printf("3 - first: %i, second: %i\n",p3->OSFree,p3->OSUsed);
              OS_STK_DATA *p4;
              OSTaskStkChk(TASK4_PRIORITY, p4);
              printf("4 - first: %i, second: %i\n\n",p4->OSFree,p4->OSUsed);


        }
    }

We noticed that we were getting the same values for each task, since we were not giving any information about when the tasks were switching.
To do so we had to enable the Hook for the task switches. We found the prototypes defined in `BSP/HAL/src/os_cpu_c.c`, and added the following functions:


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

The structure containing the other information about the execution of each task is then the following type:

    Typedef struct {
        Char TaskName[30] ;
        INT16U TaskCtr ;
        INT16U TaskExecTime ;
        INT16U TaskTotExecTime;
    } TASK_USER_DATA;

 And we could call it, creating an array of instantiations of the structure:

    TASK_USER_DATA OurTaskUserData[NB_TASKS] ;

Once we added each call to the functions in the right places in the code (such as`OSInitHookBegin(void)` in the main), we had to change the creation by passing the address of our structure element:

      OSTaskCreateExt(task2,
                      NULL,
                      (void *)&task2_stk[TASK_STACKSIZE-1],
                      TASK2_PRIORITY,
                      TASK2_PRIORITY,
                      task2_stk,
                      TASK_STACKSIZE,
                      &OurTaskUserData[1], //like here in this task creation
                      OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);

 We could finally print what is contained in the array of structures in the modified `task5`:

    void task5(void* pdata)
    {
        INT8U err;

        while(1){
              OSTimeDlyHMSM(0, 0, 5, 0);
              int i;
              for (i=0; i<NB_TASKS;i++){
                printf("%i: TaskCtr:%i, TaskExecTime:%i, TaskTotExecTime:%i",
                      i, OurTaskUserData[i].TaskCtr,OurTaskUserData[i].TaskExecTime,
                       OurTaskUserData[i].TaskTotExecTime)
              }

        }
    }

One last important aspect that caused some confusion was changing the variable `OS\_CPU\_HOOKS\_EN` in the `system.h` file:

    #define OS_CPU_HOOKS_EN 0

## Final measurements

Despite our efforts with the code, we did not have time to finish debugging it and to collect the data. However, we can analyze the code and make some assumptions.

### Number of executions 

-   Task1: The task includes a loop that executes indefinitely and has
    multiple operations inside it. The first operation reads a value
    from some memory location, which we can assume is relatively fast.
    The second operation posts a message to a mailbox, which should also
    be relatively fast. The third operation counts the number of ones in
    the read data, which is also a fast operation. The fourth operation
    posts another message to a different mailbox. The last part of the
    loop contains a conditional statement that prints some information
    if a condition is met and then waits on a mailbox to receive a
    value.\
    Based on this analysis, we can assume that the task will execute
    relatively quickly, especially since it is spending most of its time
    waiting on mailboxes to receive messages. We can estimate that this
    task may execute several times per second, depending on the speed of
    the system and how often it is scheduled.

-   Task2: It's a bit more complicated to estimate the number of
    executions of this task since it contains both variable delays and
    random numbers.

    The loop inside the task contains a call to rand(), which generates
    a random number between 0 and max_time (where max_time is equal to
    ones + 1). Assuming a uniform distribution of rand() values, we can
    estimate that on average, this task will generate a random number
    equal to half of max_time.

    After generating a random number, the task enters a loop that runs
    for a duration equal to the random number, with an iteration time of
    200 milliseconds. Therefore, the loop will execute random_number /
    0.2 times (i.e., 5 \* random_number times) before exiting.

    Assuming that the rand() values are uniformly distributed and that
    the ones value is equally likely to be any number between 0 and 8
    (the maximum value that can be read from the switches), we can
    estimate the expected number of loop iterations as
    follows:$$expected\_iterations = (8 + 1) * (5 * (8 / 2 + 1)) = 225$$
    So we can expect this task to be executed approximately 225 times
    per second, on average. However, it's worth noting that this is just
    an estimate and that the actual number of executions will vary
    depending on the randomness of the rand() function and the specific
    values of the ones data.

-   Task3: this task is waiting for the value of the PostButton mailbox.
    When the value is 8, it will print the final time and turn off the
    LEDs.

    Since the value of 8 is only posted once after button 0 is pressed,
    we can assume that this task will execute only once after button 0
    is pressed. Therefore, we can estimate the expected number of
    executions to be 1.

-   Task4: This task waits for two messages from tasks 2 and 3 before
    performing some calculations and posting the result to the mailbox
    PostAvg. The task waits for messages indefinitely until they are
    received, so we can expect this task to execute every time a message
    is received from both tasks 2 and 3. The frequency of messages from
    task 2 is not very predictable as it depends on the button presses,
    but we can estimate that the average time between messages is
    approximately 200ms. Task 3 waits for the button 8 press, which we
    know only happens once after button 0 is pressed in task 2, so we
    can estimate this task to execute once per button press in task 2.
    Based on these assumptions, we can estimate the expected number of
    executions to be around 5-10 times per minute.

-   Task5: This task runs an infinite loop with a 5-second delay between
    iterations, and each iteration it prints the stack usage of four
    other tasks. The execution count of task 5 would depend on how long
    the system runs, but assuming the system runs continuously, task 5
    would execute approximately 12 times per minute

### Mean execution time 

-   Task 1 appears to be a simple loop that sets the value of a
    register, so it would likely execute very quickly, on the order of
    microseconds.

-   Task 2 performs some calculations and prints out the results, so it
    would likely take longer than Task 1, perhaps on the order of
    milliseconds.

-   Task 3 waits for a message from another task and performs some
    calculations when it receives the message. The execution time of
    this task depends on how often it receives messages and how long the
    calculations take to complete. It's possible that this task could
    take anywhere from a few microseconds to several seconds to
    complete.

-   Task 4 waits for messages from two other tasks and performs some
    calculations when it receives them. Like Task 3, the execution time
    of this task depends on how often it receives messages and how long
    the calculations take to complete. It's possible that this task
    could take longer to complete than Task 3, as it performs more
    calculations.

-   Task 5 periodically checks the stack usage of each of the four other
    tasks and prints out the results. This task likely executes very
    quickly, on the order of microseconds.

### Free stack 

-   task1: around 1788 bytes free

-   task2: around 1996 bytes free

-   task3: around 2000 bytes free

-   task4: around 1976 bytes free

-   task5: around 1988 bytes free

Note that these values are based on rough estimates of the bytes size of
each variable in the task and on the assumption that each task uses
roughly the same amount of stack space.

### Used stack 

-   task1: around 260 bytes free

-   task2: around 52 bytes free

-   task3: around 48 bytes free

-   task4: around 72 bytes free

-   task5: around 60 bytes free

