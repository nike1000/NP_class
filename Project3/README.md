# NP_Project 3: Remote Batch System (rbs) in HTTP.

In this homework, we are asked to write a remote batch system in HTTP.

There are three parts of this project we need to finish,

  1. Write a CGI program to receive an HTTP request described in spec.
  2. Write a simple http server in Unix to support CGI.
  3. Combine Part1 & 2, Write a http server only support specific CGI in Windows

This Project need to run with our ras/rwg server in Project1/Project2.

delayserver.c and delayread.cpp is the test server to check our cgi is running on non-blocking connect.

There are many spaces in t6.txt file, run this with delayread.cpp server to check if your cgi can read and send bytes correctly.

The return the the browser will be "Totally read: 245063 bytes".
