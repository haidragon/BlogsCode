#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>

#define eprintf(...) fprintf(stderr, ##__VA_ARGS__)

void loop()
{
    while(true) 
    {
        sleep(60);
    }
}

bool may_cause_group_stop(int signo)
{
    switch(signo) 
    {
        case SIGSTOP:
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            return true;
            break;
        default:
            break;
    }

    return false;
}

void handle_events()
{
    int status = 0;
    pid_t pid = 0;

    do 
    {
        pid = TEMP_FAILURE_RETRY(waitpid(-1, &status, __WALL));
        if (pid < 0) 
        {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(status)) 
        {
            eprintf("%d exited, status=%d\n", pid,  WEXITSTATUS(status));
        } 
        else if (WIFSIGNALED(status)) 
        {
            eprintf("%d killed by signal %d\n", pid, WTERMSIG(status));
        } 
        else if (WIFSTOPPED(status)) 
        {
            int signo = WSTOPSIG(status);
            eprintf("%d stopped by signal %d\n", pid, signo);

            if (may_cause_group_stop(signo)) 
            {
                signo = 0;
            }
            
            long err = ptrace(PTRACE_CONT, pid, NULL, signo);
            if (err < 0) 
            {
                perror("PTRACE_CONT");
                exit(EXIT_FAILURE);
            }
        }

    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

}

void safe_attach(pid_t pid)
{
    long err = ptrace(PTRACE_ATTACH, pid, NULL, NULL);
    if (err < 0) 
    {
        perror("PTRACE_ATTACH");
        exit(EXIT_FAILURE);
    }

    int status = 0;
    err = TEMP_FAILURE_RETRY(waitpid(pid, &status, __WALL));
    if (err < 0) 
    {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }

    if (WIFEXITED(status)) 
    {
        eprintf("%d exited, status=%d\n", pid,  WEXITSTATUS(status));
        exit(EXIT_SUCCESS);
    } 
    else if (WIFSIGNALED(status)) 
    {
        eprintf("%d killed by signal %d\n", pid, WTERMSIG(status));
        exit(EXIT_SUCCESS);
    } 
    else if (WIFSTOPPED(status)) 
    {
        int signo = WSTOPSIG(status);
        eprintf("%d stopped by signal %d\n", pid, signo);

        if (may_cause_group_stop(signo)) 
        {
            signo = 0;
        }
        
        err = ptrace(PTRACE_CONT, pid, NULL, signo);
        if (err < 0) 
        {
            perror("PTRACE_CONT");
            exit(EXIT_FAILURE);
        }
    } 

}

void protect_father()
{
    pid_t ppid =  getppid();

    safe_attach(ppid);
    handle_events();
}

int main()
{
    eprintf("main process pid: %d\n", getpid());
    
    // 设置可ptrac
    prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);

    pid_t pid = fork();
    if (pid < 0) 
    {
        perror("fork");
    } 
    else if (pid == 0) 
    {
        eprintf("child process pid: %d\n", getpid());

        protect_father();
        exit(EXIT_SUCCESS);
    }

    eprintf("main process start loop...\n");
    loop();
    
    return 0;
}