#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

static void check_error(int ret, const char *message) {
    if (ret != -1) {
        return;
    }
    int err = errno;
    perror(message);
    exit(err);
}

//static void parent(int in_pipefd[2], int out_pipefd[2], pid_t child_pid) {
//    check_error(close(in_pipefd[0]), "close");
//    check_error(close(out_pipefd[1]), "close");
//    
//    // Write to the in pipe
//    const char* message = "Testing\n";
//    int bytes_written = write(in_pipefd[1], message, strlen(message));
//    check_error(bytes_written, "write");
//    check_error(close(in_pipefd[1]), "close");
//    
//    int wstatus;
//    check_error(waitpid(child_pid, &wstatus, 0), "waitpit");
//    assert(WIFEXITED(wstatus));
//    int exit_status = WEXITSTATUS(wstatus);
//    assert(exit_status == 0);
//    
//    // Read from the out pipe
//    char buffer[4096];
//    int bytes_read = read(out_pipefd[0], buffer, sizeof(buffer));
//    check_error(bytes_read, "read");
//    printf("%.*s\n", bytes_read, buffer);
//    check_error(close(out_pipefd[0]), "close");
//}

//static void child(int in_pipefd[2], int out_pipefd[2], const char *program) {
//    // Input pipe
//    check_error(dup2(in_pipefd[0], STDIN_FILENO), "dup2");
//    check_error(close(in_pipefd[0]), "close");
//    check_error(close(in_pipefd[1]), "close");
//    
//    // Output pipe
//    check_error(dup2(out_pipefd[1], STDOUT_FILENO), "dup2"); // fd: 4 and fd: 1 point to the same thing
//    check_error(close(out_pipefd[0]), "close"); // close fd: 3
//    check_error(close(out_pipefd[1]), "close"); // close fd: 4
//    
//    execlp(program, program, NULL);
//}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        return EINVAL;
    }
    
    int pipes[argc][2]; // one pipe per subsequent child processes pair
    for(int i = 0; i < argc; i++){
        check_error(pipe(pipes[i]), "pipe");
    }
    
    int childIDs[argc - 1];
    int counter = 0;
    
    for(int i = 1; i < argc; i++){
        
        // first child process
        if(i == 1){
            pid_t pid = fork();
            
            // parent
            if(pid > 0){
                
                check_error(close(pipes[i - 1][1]), "close");
                
                childIDs[counter] = pid;
                counter++;
                
//                int wstatus;
//                check_error(waitpid(pid, &wstatus, 0), "waitpit");
//                assert(WIFEXITED(wstatus));
//                int exit_status = WEXITSTATUS(wstatus);
//                if(exit_status != 0){
//                    exit(exit_status);
//                }
            }
            
            // child
            else{
                
                check_error(dup2(pipes[i - 1][1], STDOUT_FILENO), "dup2");
                
                check_error(close(pipes[i - 1][1]), "close");
                
                execlp(argv[i], argv[i], NULL);
                
                exit(errno);
            }
        }
        
        // last child process
        else if(i == argc - 1){
            pid_t pid = fork();
            
            // parent
            if(pid > 0){
                
                check_error(close(pipes[i - 2][0]), "close");
                
                childIDs[counter] = pid;
                counter++;
                
//                int wstatus;
//                check_error(waitpid(pid, &wstatus, 0), "waitpit");
//                assert(WIFEXITED(wstatus));
//                int exit_status = WEXITSTATUS(wstatus);
//                if(exit_status != 0){
//                    exit(exit_status);
//                }
            }
            
            // child
            else{
                
                check_error(dup2(pipes[i - 2][0], STDIN_FILENO), "dup2");
                
                check_error(close(pipes[i - 2][0]), "close");
                
                execlp(argv[i], argv[i], NULL);
                
                exit(errno);
            }
        }
        
        // other child processes
        else{
            pid_t pid = fork();
            
            // parent
            if(pid > 0){
                check_error(close(pipes[i - 2][0]), "close");
                check_error(close(pipes[i - 1][1]), "close");
                
                childIDs[counter] = pid;
                counter++;
                
//                int wstatus;
//                check_error(waitpid(pid, &wstatus, 0), "waitpit");
//                assert(WIFEXITED(wstatus));
//                int exit_status = WEXITSTATUS(wstatus);
//                if(exit_status != 0){
//                    exit(exit_status);
//                }
            }
            
            // child
            else{
                
                check_error(dup2(pipes[i - 2][0], STDIN_FILENO), "dup2");
                check_error(dup2(pipes[i - 1][1], STDOUT_FILENO), "dup2");
                
                check_error(close(pipes[i - 2][0]), "close");
                check_error(close(pipes[i - 1][1]), "close");
                
                execlp(argv[i], argv[i], NULL);
                
                exit(errno);
            }
        }
    }
    
    // Have to wait outside to pass stdin
    for(int j = 0; j < argc - 1; j++){
        int wstatus;
        check_error(waitpid(childIDs[j], &wstatus, 0), "waitpit");
        assert(WIFEXITED(wstatus));
        int exit_status = WEXITSTATUS(wstatus);
        if(exit_status != 0){
            exit(exit_status);
        }
    }
    
    return 0;
}