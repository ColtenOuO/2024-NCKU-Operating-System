#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *cmd) {
    if (cmd->in_file) { // check 是否有特定的輸入文件
        int fd = open(cmd->in_file, O_RDONLY);
        if (fd == -1) {
            perror(cmd->in_file);
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO); // 將 file read 轉成 stdin
        close(fd);
    } else if (cmd->in != STDIN_FILENO) { // pipe
        dup2(cmd->in, STDIN_FILENO); // pipe to stdin
    }

    if (cmd->out_file) {
        int fd = open(cmd->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            perror(cmd->out_file);
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO); // 將 file write -> stdout
        close(fd);
    } else if (cmd->out != STDOUT_FILENO) { // pipe
        dup2(cmd->out, STDOUT_FILENO); // 將 output pipe 出去 轉 stdout
    }
}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p) {
    pid_t pid = fork();

    if (pid == 0) {  // pid == 0 表示現在是 child process
        redirection(p);
        int status = execvp(p->args[0], p->args);
        if (status == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else {  // parent process
        waitpid(pid, NULL, 0);
    }
    return 0;
}
// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd) {
    struct cmd_node *p = cmd->head;
    while (p) {
        if (p->next) {
            int fd[2]; // fd[0] 讀取 fd[1] 寫入
            pipe(fd);

            p->out = fd[1];
            p->next->in = fd[0];
        }

        spawn_proc(p);

        if (p != cmd->head) close(p->in); // cat input.txt | grep owo
        if (p->next != NULL) close(p->out);

        p = p->next;
    }

    return 0;
}
// ===============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 | out == -1)
					perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status,temp);

				// recover shell stdin and stdout
				if (temp->in_file)  dup2(in, 0);
				if (temp->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		if (status != 0)
			break;
	}
}
