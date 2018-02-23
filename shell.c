// Shell starter file
// You may make any changes to any part of this file.

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define COMMAND_LENGTH 1024
#define NUM_TOKENS (COMMAND_LENGTH / 2 + 1)
#define HISTORY_DEPTH 10

int history_count = 0;
char history[HISTORY_DEPTH][COMMAND_LENGTH];

// adds commands to history
void add_to_history(char *buff) 
{
	strcpy(history[history_count % HISTORY_DEPTH], buff);	// copy from buff to history
	history_count++;
}

// print the history that is the last 10 commands
void print_history() 
{
	char history_count_str[COMMAND_LENGTH];
	if (history_count <= HISTORY_DEPTH)	// if less than or equal to 10 commands are entered
	{
		for (int i = 0; i < HISTORY_DEPTH; i++) {
			snprintf(history_count_str, COMMAND_LENGTH, "%d\t", i+1);
			write(STDOUT_FILENO, history_count_str, strlen(history_count_str));
			write(STDOUT_FILENO, history[i], strlen(history[i]));
			write(STDOUT_FILENO, "\n", strlen("\n"));
		}
	}
	else	// if more than 10 commands were entered, print only the last 10
	{
		for (int i = (history_count - HISTORY_DEPTH); i < history_count; i++) 
		{
			snprintf(history_count_str, COMMAND_LENGTH, "%d\t", i+1);
			write(STDOUT_FILENO, history_count_str, strlen(history_count_str));
			write(STDOUT_FILENO, history[i % HISTORY_DEPTH], strlen(history[i % HISTORY_DEPTH]));
			write(STDOUT_FILENO, "\n", strlen("\n"));
		}
	}
}

// signal handler, prints the history and then displays the prompt command when Ctrl-C is pressed (doesn't exit the shell)
void handle_SIGINT()
{
	write(STDOUT_FILENO, "\n", strlen("\n"));
	print_history();
}

/**
 * Command Input and Processing
 */

/*
 * Tokenize the string in 'buff' into 'tokens'.
 * buff: Character array containing string to tokenize.
 *       Will be modified: all whitespace replaced with '\0'
 * tokens: array of pointers of size at least COMMAND_LENGTH/2 + 1.
 *       Will be modified so tokens[i] points to the i'th token
 *       in the string buff. All returned tokens will be non-empty.
 *       NOTE: pointers in tokens[] will all point into buff!
 *       Ends with a null pointer.
 * returns: number of tokens.
 */

int tokenize_command(char *buff, char *tokens[])
{
	int token_count = 0;
	_Bool in_token = false;
	int num_chars = strnlen(buff, COMMAND_LENGTH);
	for (int i = 0; i < num_chars; i++) {
		switch (buff[i]) {
		// Handle token delimiters (ends):
		case ' ':
		case '\t':
		case '\n':
			buff[i] = '\0';
			in_token = false;
			break;

		// Handle other characters (may be start)
		default:
			if (!in_token) {
				tokens[token_count] = &buff[i];
				token_count++;
				in_token = true;
			}
		}
	}
	tokens[token_count] = NULL;
	return token_count;
}

/**
 * Read a command from the keyboard into the buffer 'buff' and tokenize it
 * such that 'tokens[i]' points into 'buff' to the i'th token in the command.
 * buff: Buffer allocated by the calling code. Must be at least
 *       COMMAND_LENGTH bytes long.
 * tokens[]: Array of character pointers which point into 'buff'. Must be at
 *       least NUM_TOKENS long. Will strip out up to one final '&' token.
 *       tokens will be NULL terminated (a NULL pointer indicates end of tokens).
 * in_background: pointer to a boolean variable. Set to true if user entered
 *       an & as their last token; otherwise set to false.
 */
void read_command(char *buff, char *tokens[], _Bool *in_background)
{
	*in_background = false;

	// Read input
	int length = read(STDIN_FILENO, buff, COMMAND_LENGTH-1);

	// if not able to read input
	if ((length < 0) && (errno != EINTR)) {
			perror("Unable to read command from keyboard. Terminating.\n");
			exit(-1);	//terminate with error
	}

	// Null terminate and strip \n.
	buff[length] = '\0';
	if (buff[strlen(buff) - 1] == '\n') {
		buff[strlen(buff) - 1] = '\0';
	}

	// Tokenize (saving original command string)
	char copy_buff[COMMAND_LENGTH];
	strcpy(copy_buff, buff);	// copy from buff to copy_buff
	int token_count = tokenize_command(buff, tokens);
	if (token_count == 0) {
		return;
	}

	// if a command is given that starts with '!'
	if (tokens[0][0] == '!') 
	{
 		if (strcmp(tokens[0], "!!") == 0)	// if '!!' command is given, run the previous command
 		{
 			if (history_count == 0) 
 			{
 				write(STDERR_FILENO,"Error: No previous command found\n", strlen("Error: No previous command found\n"));
 				tokens[0] = NULL;
 				return;
 			}
 			strcpy(buff, history[(history_count - 1) % HISTORY_DEPTH]);
 			strcpy(copy_buff, buff);
 			token_count = tokenize_command(buff, tokens);
 			write(STDOUT_FILENO, copy_buff, strlen(copy_buff));
        	write(STDOUT_FILENO, "\n", strlen("\n"));
        }
 	

 		else	// if '!n' command is given, where n is a number then run that command
 		{
 			int num = atoi(&tokens[0][1]);	// convert to int 
 			if ((num <= 0) || (num > history_count) || (num <= history_count - HISTORY_DEPTH))
 			{
 				write(STDERR_FILENO,"Error: Invalid command number\n", strlen("Error: Invalid command number\n"));
 				tokens[0] = NULL;
 				return;
 			}
 			strcpy(buff, history[(num - 1) % HISTORY_DEPTH]);
 			strcpy(copy_buff, buff);
 			token_count = tokenize_command(buff, tokens);
 			write(STDOUT_FILENO, copy_buff, strlen(copy_buff));
        	write(STDOUT_FILENO, "\n", strlen("\n"));
        }
 	}

	// Extract if running in background:
	if (token_count > 0 && strcmp(tokens[token_count - 1], "&") == 0) {
		*in_background = true;
		tokens[token_count - 1] = 0;
	}

	add_to_history(copy_buff);
}

/**
 * Main and Execute Commands
 */
int main(int argc, char* argv[])
{
	char input_buffer[COMMAND_LENGTH];
	char *tokens[NUM_TOKENS];
	struct sigaction handler;
	handler.sa_handler = handle_SIGINT;
	handler.sa_flags = 0;
	sigemptyset(&handler.sa_mask);
	sigaction(SIGINT, &handler, NULL);
	while (true) {

		// Get command
		// Use write because we need to use read() to work with
		// signals, and read() is incompatible with printf().
		write(STDOUT_FILENO, getcwd(NULL, 0), strlen(getcwd(NULL, 0)));	//include the current working directory in the prompt
		write(STDOUT_FILENO, "> ", strlen("> "));
		_Bool in_background = false;
		input_buffer[0] = 0;
		read_command(input_buffer, tokens, &in_background);

		if(tokens[0] != NULL)	// if not an empty line
		{
			//when exit command is given
			if (strcmp(tokens[0],"exit") == 0)	
				return 0;

			//when pwd command is given
			else if (strcmp(tokens[0],"pwd") == 0)	
			{	
				char* curr_dir = getcwd(NULL,0);
				if(curr_dir)
				{
					write(STDOUT_FILENO, "Working Directory is: ", strlen("Working Directory is: "));
					write(STDOUT_FILENO, curr_dir, strlen(curr_dir));
					write(STDOUT_FILENO, "\n", strlen("\n"));
				}
				else	//if error
					perror("getcwd error");	
				free(curr_dir);
				continue;
			}

			//when cd 'something' command is given
			else if (strcmp(tokens[0],"cd") == 0)	
			{
				if (chdir(tokens[1]) != 0)	 
					perror("chdir error");	//if only 'cd'
				continue;
			}

			//when history command is given
			else if (strcmp(tokens[0], "history") == 0) {
 				print_history();
 				continue;
 			}

			else
			{
				pid_t child_pid;	
				child_pid = fork();
				if (child_pid < 0)	//if error 
				{
					perror("fork failed");
					exit(-1);
				}

				else if (child_pid == 0)	//child process
				{
					if(execvp(tokens[0], tokens) < 0)	//execute the command
						perror("execvp error");
				}

				else	//parent process
				{ 
					if (!in_background)	//when '&' is not given at the end
						waitpid(child_pid, NULL, 0);	//wait for child to finish
					else
						read_command(input_buffer, tokens, &in_background);	//when '&' is given at the end
				}
				while(waitpid(-1, NULL, WNOHANG) > 0)	//wait for all zombie child processes to finish
							;	//do nothing
			}
		}
	}
	return 0;
}
