# User Level Thread Scheduling
In this homework user level threads are scheduled in a round robin manner to count from 0 to the inputed numbers. 

To observe the scheduling effect each thread has time to only print 10 numbers at its' turn. 

If the thread finishes before its' turn has ended it signals and a context switch occurs.
If the thread doens't finish it is inserted to the end of the queue. 

In this homework instead of a queue an array is to be used because of limitations. The finished threads are replaced with newly created threads.

Example execution: 

![img](https://i.imgur.com/2tVkIz9.png)
