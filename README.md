# Concurrent Exam Marking System
This program simulates TAs concurrently marking exams. It uses semaphores to protect shared memory.

## Compilation
This program must be compiled inside Linux or WSL, since it uses `fork()`, System V shared memory, and POSIX semaphores.

Compile using g++ (required because the code uses C++ features):

```bash
g++ mark_101300523_101305200.cpp -o mark -pthread
```

and run it using 

```bash
./mark #TAs
```

