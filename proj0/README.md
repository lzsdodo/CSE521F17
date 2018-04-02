# Problem 1

1. Part 1 
    - The program should have exactly 2 threads. 
    - Use the standard pthreads library for thread creation and management.
    - Includes the main thread.
    - Spaw 1 additional thread after the program starts.
    - Main thread print odd numbers on the console with 1 number on each line.
    - Second thread print even numbers on the console again with 1 number on each line.
    - Stop after printing 1000 numbers (from both threads).
    - The output is jumbled up and the numbers are not in order.

2. Part 2
    - Use thread synchronization mechanics (ex. mutex, semaphores and conditional variables etc.) to synchronize the output.
    - The numbers printed by both threads should be in order.
    - Bonus: Repeat the assignment using a diﬀerent synchronization technique.
    