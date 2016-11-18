# NP_Project 2: remote working ground (rwg)

In this network programming homework, we are asked to build a remote working ground (RWG), which is actually a combination of Project1 and chat & named pipe function.

According to the spec, we need to write two programs:

  1. Use the **single-process concurrent** paradigm.
  2. Use the **concurrent connection-oriented** paradigm with **shared memory**.

After running up the program, you can compile delayclient.c and test it by

```
$ gcc delayclient.c -o delayclient
$ ./delayclient ip port /path/to/testdata
```
