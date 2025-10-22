# OS-ProgramingAssignment3

Authors: Haley Kothari and Kylee Down


Compile: gcc -pthread rw_log.c -o rw_log

Run: ./rw_log --capacity 1024 --readers 4 --writers 2 --writer-batch 2 --seconds 10 --rd-us 2000 --wr-us 3000

Sources:
https://man7.org/linux/man-pages/man3/getopt.3.html
https://www.geeksforgeeks.org/c/sleep-function-in-c/
https://man7.org/linux/man-pages/man3/sleep.3.html
https://man7.org/linux/man-pages/man3/pthread_join.3p.html

Proof of Success:  

Compling: 
<img width="1428" height="781" alt="Screenshot 2025-10-21 at 11 30 01 PM" src="https://github.com/user-attachments/assets/c606ff45-af79-4704-8a2f-ddb95cfb3362" />

Running: 

<img width="1428" height="774" alt="Screenshot 2025-10-21 at 11 30 44 PM" src="https://github.com/user-attachments/assets/44755fc0-48c5-430a-af07-4d941021fdc1" />
