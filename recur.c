#define _POSIX_C_SOURCE 199309L

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>

#define WRITE_STR(FD, STRING) write(FD, STRING, strlen(STRING))

static void printUsage() {
    WRITE_STR(2, "Usage: ./recur -i <interval in seconds> [-f <file to store last execution time>] -- <command>\n");
}

static int stringToTime(char* input, time_t* output) {
    *output = 0;
    char inputChar;
    while ((inputChar = *input) != '\0') {
        if (inputChar < '0' || inputChar > '9') {
            return -1;
        }
        *output = (*output * 10) + (inputChar - '0');
        input++;
    }
    return 0;
}

static char* timeToString(time_t input, char* buf, size_t bufsize) {
    if (bufsize <= 2) {
        // Not enough space, even for a single-digit number
        return NULL;
    }
    size_t lastPrintedChar = bufsize - 1;
    // add null terminator
    buf[lastPrintedChar] = '\0';
    // add digits one by one
    while (lastPrintedChar > 0) {
        lastPrintedChar--;
        buf[lastPrintedChar] = '0' + (input % 10);
        input /= 10;
        if (input == 0) {
            // done, return pointer to last printed digit
            return &buf[lastPrintedChar];
        }
    }
    // ran out of space
    return NULL;
}

static int readFile(char* filename, char* buf, size_t bufsize) {
    int fileFd = open(filename, O_RDONLY | O_CREAT, 0660);
    if (fileFd < 0) { 
        return -1; 
    }
    unsigned int result = 0;
    while (result >= 0 && result < bufsize) {
        int readResult = read(fileFd, &(buf[result]), bufsize - result);
        if (readResult == 0) { 
            break; 
        }
        if (readResult < 0) { 
            if (errno != EINTR) {
                result = -1; 
                break;
            }
        } else {
            result += readResult;
        }
    }
    close(fileFd);
    return result;
}

static int writeFile(char* filename, char* buf, size_t bufsize) {
    int fileFd = open(filename, O_WRONLY | O_CREAT, 0660);
    if (fileFd < 0) { 
        return -1; 
    }
    unsigned int result = 0;
    while (result >= 0 && result < bufsize) {
        int writeResult = write(fileFd, &(buf[result]), bufsize - result);
        if (writeResult <= 0) { 
            if (errno != EINTR) {
                result = -1; 
                break;
            }
        } else {
            result += writeResult;
        }
    }
    close(fileFd);
    return result;
}

int main(int argc, char** argv, char** envp) {
    // parse command-line options
    char** programArgv = NULL; // full argv for the program we want to execute
    time_t interval = 0; // interval (in seconds) at which we want to execute the program
    time_t lastExecutionTime = 0; // last time (in terms of UNIX time) that the program was executed
    char* lastExecutionTimeFile = NULL; // file where the last execution time should be saved

    // Parse command line arguments
    char **workingArgv = (argv + 1);
    while (1) {
        char* option = *workingArgv;
        workingArgv++;
        if (option == NULL) {
            printUsage();
            return 1;
        } else if (strcmp(option, "--") == 0) {
            if (interval == 0) {
                WRITE_STR(2, "Error: interval parameter (-i) missing.\n");
                printUsage();
                return 1;
            }
            programArgv = workingArgv;
            break;
        } else if (strcmp(option, "-i") == 0) {
            char* intervalStr = *workingArgv;
            if (intervalStr == NULL) {
                WRITE_STR(2, "Error: -i requires a parameter.\n");
                printUsage();
                return 1;
            }
            if (stringToTime(*workingArgv, &interval) != 0) {
                WRITE_STR(2, "Error: invalid value for -i.\n");
                printUsage();
                return 1;
            }
            workingArgv++;
        } else if (strcmp(option, "-f") == 0) {
            lastExecutionTimeFile = *workingArgv;
            if (lastExecutionTimeFile == NULL) {
                WRITE_STR(2, "Error: -f requires a parameter.\n");
                printUsage();
                return 1;
            }
            char fileContents[128] = { 0 };
            if (readFile(lastExecutionTimeFile, fileContents, sizeof(fileContents) - 1) < 0) {
                WRITE_STR(2, "Error: Cannot read file ");
                WRITE_STR(2, lastExecutionTimeFile);
                WRITE_STR(2, ", ");
                WRITE_STR(2, strerror(errno));
                WRITE_STR(2, "\n");
            }
            if (stringToTime(fileContents, &lastExecutionTime) != 0) {
                WRITE_STR(2, "Error: file contents of ");
                WRITE_STR(2, lastExecutionTimeFile);
                WRITE_STR(2, " are invalid.\n");
                return 1;
            }
            workingArgv++;
        } 
    }

    time_t nextExecutionTime = lastExecutionTime + interval;

    while(1) {
        time_t currentTime = time(NULL);
        if (currentTime < nextExecutionTime) {
            // Wait until it's time to run the command
            sleep(nextExecutionTime - currentTime);
        } else {
            // Run the command in a subprocess
            int childPid = fork();
            if (childPid < 0) {
                WRITE_STR(2, "Error: fork() failed, ");
                WRITE_STR(2, strerror(errno));
                WRITE_STR(2, "\n");
            } else if (childPid == 0) {
                execve(programArgv[0], programArgv, envp);
                // We only get here if execve() fails
                WRITE_STR(2, "Error: execve() failed, ");
                WRITE_STR(2, strerror(errno));
                WRITE_STR(2, "\n");
                _exit(1);
            } else {
                // wait for the first child to exit, and then also reap any remaining children
                int childStatus;
                while (waitpid(childPid, &childStatus, 0) <= 0) {
                    if (errno != EINTR) {
                        WRITE_STR(2, "Error: waitpid() failed, ");
                        WRITE_STR(2, strerror(errno));
                        WRITE_STR(2, "\n");
                        return 1;
                    }
                }
                while (waitpid(-1, NULL, 0) > 0) {}

                // exit if the child exited unsuccessfully
                if (!(WIFEXITED(childStatus) && WEXITSTATUS(childStatus) == 0)) {
                    WRITE_STR(2, "Error: Child did not exit successfully.\n");
                    return WIFEXITED(childStatus) ? WEXITSTATUS(childStatus) : 1;
                }

                // Remember when we last executed the command
                lastExecutionTime = currentTime;
                nextExecutionTime = lastExecutionTime + interval;
                if (lastExecutionTimeFile != NULL) {
                    char buf[128] = { 0 };
                    char* lastExecutionTimeStr = timeToString(lastExecutionTime, buf, sizeof(buf));
                    if (lastExecutionTimeStr == NULL) {
                        WRITE_STR(2, "Error: timeToString() failed.");
                        return 1;
                    }
                    if (writeFile(lastExecutionTimeFile, lastExecutionTimeStr, strlen(lastExecutionTimeStr)) < 0) {
                        WRITE_STR(2, "Error: Could not write to ");
                        WRITE_STR(2, lastExecutionTimeFile);
                        WRITE_STR(2, ", ");
                        WRITE_STR(2, strerror(errno));
                        WRITE_STR(2, "\n");
                        return 1;
                    }
                }
            }
        }
    }
}