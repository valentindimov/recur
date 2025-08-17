# recur
...is a tiny utility for running recurring tasks on Linux or Unix-like systems. 
It has no dependencies aside from libc and produces a static build of less than 50 KB, making it suitable for use in containers or low-resource environments.

## Features
The functionality provided by `recur` is very simple:
- It runs a given command repeatedly, at a set interval;
- It stops if the command fails at some point, and returns its exit code;
- Optionally, it can use a file to remember the last time the command successfully executed.

The last feature is useful in cases where you don't want a restart to cause the command to rerun too early, e.g. for heavier or less frequent tasks like renewing TLS certificates or performing backups.

## Building
Install `musl-gcc` (on Ubuntu, you will need the `gcc` and `musl-tools` packages) and run [./build.sh](./build.sh). 
This will produce a static binary in `build/recur`.

Alternatively, compile [recur.c](./recur.c) with a C compiler of your choice.

## Command-line interface
```
./recur -i <interval in seconds> [-f <file to store last execution time>] -- <command>
```
For example:
1. Execute `/bin/echo 123` every 30 seconds:
    ```
    ./recur -i 30 -- /bin/echo 123
    ```
1. Execute `/bin/date` every 5 minutes, and remember the last execution time in `/tmp/last_execution`:
    ```
    ./recur -i 300 -f /tmp/last_execution -- /bin/date
    ```
    If you interrupt and restart this command while it is waiting, it should still execute `/bin/date` 5 minutes after the last execution.
1. Execute a command that fails with an exit code and capture the exit code:
    ```
    ./recur -i 10 -- /bin/sh -c "exit 123"
    echo $?
    ```
    `recur` should stop and it should return with exit code `123`.

## License
Copyright (c) 2025 Valentin Dimov

This project is licensed under the MIT license, see [LICENSE](./LICENSE).

