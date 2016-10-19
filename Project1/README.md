# NP_Project1

## Input spec:

- The length of a single-line input will not exceed 15000 characters.
  There may be huge number of commands in a single-line input.
  Each command will not exceed 256 characters.

- There must be one or more spaces between commands and symbols (or arguments.), but no spaces between pipe and numbers.

<pre>
	eg. cat hello.txt | number
	    cat hello.txt |4
</pre>

- There will not be any `/` character in demo input.

- Pipe (`|`) will not come with "printenv" and "setenv".

- Use `% ` as the command line prompt.

## About server:

- The welcome message MUST been shown as follows:

<pre>
****************************************
** Welcome to the information server. **
****************************************
</pre>

- Close the connection between the server and the client immediately when the server recieve "exit".

- Note that the forked process of server MUST be killed when the connection to the client is closed.
	Otherwise, there may be lots zombie processes.


## About a numbered-pipe

- `|N` means the stdout of last command should be piped to the first command of next Nth line, where 1 <= N <= 1000.

- `!N` means both stdout and stderr of last command should be piped to the first command of next Nth line, where 1 <= N <= 1000.

- If there is any error in a input line, the line number still counts.

<pre>
    eg. cat hello.txt |2
        ctt	          <= unknown command, but the line number still counts
        number
</pre>


## About parsing:

- If there is command not found, print as follow:s
Unknown command: [command].

<pre>
    e.g. % ctt
        Unknown command: [ctt].
</pre>


## Other proposed:

1. There must be `ls`, `cat`, `removetag`, `removetag0`, `number` in `bin/` of `ras/`.

2. You have to execute the files in `bin/` with an `exec()`-based function.(eg. execvp() or execlp() ...)

3. Two of the commands (`ls` and `cat`) used in the homework are placed in the folder `/bin`.
   Please copy them in the folder `~/ras/bin/`.
   (Example: `cp /bin/ls /bin/cat ~/ras/bin/`)

4. Other commands, such as noop, number, removetag and remoetag0 are all packaged in the commands.rar.
   Please compile them by yourself and put the executable file into folder `~/ras/bin/`.
   (Compile example: `g++ noop.cpp -o ~/ras/bin/noop`)

5. After completing the server , you can test how it works by using telnet.
    (Command: `telnet nplinux1.cs.nctu.edu.tw [port]`)
    By the way, if the OS of your computer is Win7, the telnet is closed in default, you can follow this link to open it:
    http://goo.gl/kd51Sa
