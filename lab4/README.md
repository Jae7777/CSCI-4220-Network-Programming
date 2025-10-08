Team: Justin Chen, Suyash Amatya
Date: 10/07/2025
CSCI-4220 Network Programming Lab 4


This lab implements a multi-client, multi-threaded TCP server that computes Fibonacci numbers concurrently using worker threads.

The fibonacci range for each thread is divided equally among the threads.

i=0: pthread_join(thread[0]) $\rightarrow$ returns Thread 1's data
     total_sum = 0 + 4 = 4
     response += "T1: [1–3] -> 1 1 2\n"

i=1: pthread_join(thread[1]) $\rightarrow$ returns Thread 2's data
     total_sum = 4 + 16 = 20
     response += "T2: [4–6] -> 3 5 8\n"

i=2: pthread_join(thread[2]) $\rightarrow$ returns Thread 3's data
     total_sum = 20 + 68 = 88
     response += "T3: [7–9] -> 13 21 34\n"

i=3: pthread_join(thread[3]) $\rightarrow$ returns Thread 4's data
     total_sum = 88 + 287 = 375
     response += "T4: [10–12] -> 55 89 144\n"

total_sum = 376

Each thread computes the fibonacci range from scratch, which is not optimal and ultimately not as scalable as it could be.

AI was used to help explain and break down the problem into smaller digestible parts