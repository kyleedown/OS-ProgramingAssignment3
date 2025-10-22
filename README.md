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
<img width="1426" height="666" alt="Screenshot 2025-10-21 at 11 38 18 PM" src="https://github.com/user-attachments/assets/277b3847-f7bd-4083-863c-35cf7caf366c" />

Running: 

<img width="1430" height="707" alt="Screenshot 2025-10-21 at 11 38 45 PM" src="https://github.com/user-attachments/assets/b61863a9-16aa-40b1-97b6-b7438abd9f42" />
