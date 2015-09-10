// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"

/* FIXME: You may need to add #include directives, macro definitions,
static function definitions, etc.  */
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <error.h>
#include "alloc.h"

void
exec_simple_command(command_t c, bool time_travel)
{
	int saved_stdin = dup(0), saved_stdout = dup(1);

	if (c->input != NULL)
	{
		int inputFd = open(c->input, O_RDONLY);
		if (inputFd < 0)
			error(1, 0, "Error input file");
		if (dup2(inputFd, 0) < 0)
			error(1, 0, "Error reading stream");
		close(inputFd);
	}
	if (c->output != NULL)
	{
		int outputFd = open(c->output, O_CREAT | O_WRONLY | O_TRUNC, 0666);
		if (outputFd < 0)
			error(1, 0, "Error output file");
		if (dup2(outputFd, 1) < 0)
			error(1, 0, "Error writing stream");
		close(outputFd);
	}

	pid_t pid = fork();
	if (pid < 0)
		error(1, 0, "Unable to bear children for command %s", c->u.word[0]);
	else if (pid == 0)
	{
		if (strcmp(c->u.word[0], "exec") == 0)
			execvp(c->u.word[1], &c->u.word[1]);
		else
			execvp(c->u.word[0], c->u.word);
		_exit(127);
	}
	else
	{
		int status;
		waitpid(pid, &status, 0);
		c->status = WEXITSTATUS(status);
	}

	// restore stdin and stdout
	if (c->input != NULL)
		dup2(saved_stdin, 0);
	if (c->output != NULL)
		dup2(saved_stdout, 1);
	close(saved_stdin);
	close(saved_stdout);
}

void
exec_sequence_command(command_t c, bool time_travel)
{
	execute_command(c->u.command[0], time_travel);
	execute_command(c->u.command[1], time_travel);
	c->status = command_status(c->u.command[1]);
}

void
exec_and_command(command_t c, bool time_travel)
{
	execute_command(c->u.command[0], time_travel);
	if (c->u.command[0]->status == 0) // if successful
	{
		execute_command(c->u.command[1], time_travel);
		c->status = command_status(c->u.command[1]);
	}
	else
		c->status = command_status(c->u.command[0]);
}

void
exec_or_command(command_t c, bool time_travel)
{
	execute_command(c->u.command[0], time_travel);
	if (c->u.command[0]->status != 0) // if unsuccessful
	{
		execute_command(c->u.command[1], time_travel);
		c->status = command_status(c->u.command[1]);
	}
	else
		c->status = command_status(c->u.command[0]);
}

void
exec_subshell_command(command_t c, bool time_travel)
{
	if (c->input != NULL)
		c->u.subshell_command->input = c->input;
	if (c->output != NULL)
		c->u.subshell_command->output = c->output;
	execute_command(c->u.subshell_command, time_travel);
	c->status = command_status(c->u.subshell_command);
}

void
exec_pipe_command(command_t c, bool time_travel)
{
	int fd[2];
	if (pipe(fd) < 0)
		error(1, 0, "pipe fail");

	pid_t firstPid = fork();
	if (firstPid < 0)
		error(1, 0, "cannot bear child");
	else if (firstPid == 0) // execute right side of pipe
	{
		close(fd[1]);
		if (dup2(fd[0], 0) < 0) // if succeed, return second arg, else negative
			error(1, 0, "dup2 fail");

		execute_command(c->u.command[1], time_travel);
		_exit(command_status(c->u.command[1])); // _exit perform kernel cleanup
	}
	else
	{
		pid_t secondPid = fork();
		if (secondPid < 0)
			error(1, 0, "cannot bear child");
		else if (secondPid == 0)
		{
			close(fd[0]);
			if (dup2(fd[1], 1) < 0) // if succeed, return second arg, else negative
				error(1, 0, "dup2 fail");
			execute_command(c->u.command[0], time_travel);
			_exit(command_status(c->u.command[0]));
		}
		else
		{
			close(fd[0]);
			close(fd[1]);
			int status;
			pid_t returnPid = waitpid(-1, &status, 0); // -1 means any process
			if (secondPid == returnPid)
				waitpid(firstPid, &status, 0);
			if (firstPid == returnPid)
				waitpid(secondPid, &status, 0);
			c->status = WEXITSTATUS(status);
		}
	}
}

int
command_status(command_t c)
{
	return c->status;
}

void
execute_command(command_t c, bool time_travel)
{
	/* FIXME: Replace this with your implementation.  You may need to
	add auxiliary functions and otherwise modify the source code.
	You can also use external functions defined in the GNU C Library.  */

	switch (c->type)
	{
	case SIMPLE_COMMAND:
		exec_simple_command(c, time_travel);
		break;
	case SEQUENCE_COMMAND:
		exec_sequence_command(c, time_travel);
		break;
	case AND_COMMAND:
		exec_and_command(c, time_travel);
		break;
	case OR_COMMAND:
		exec_or_command(c, time_travel);
		break;
	case SUBSHELL_COMMAND:
		exec_subshell_command(c, time_travel);
		break;
	case PIPE_COMMAND:
		exec_pipe_command(c, time_travel);
		break;
	default:
		;
	}
}


//
// Timetravel
//

void
storeInputOutput(command_t c, list_node_t lnode)
{
	if (c->input != NULL)
	{
		lnode->rsize++;
		lnode->read_list = (char **)checked_realloc(lnode->read_list, sizeof(char *) * lnode->rsize);
		lnode->read_list[lnode->rsize - 1] = c->input;
	}

	if (c->output != NULL)
	{
		lnode->wsize++;
		lnode->write_list = (char **)checked_realloc(lnode->write_list, sizeof(char *) * lnode->wsize);
		lnode->write_list[lnode->wsize - 1] = c->output;
	}
}

void
processCommand(command_t c, list_node_t lnode)
{
	if (c->type == SIMPLE_COMMAND)
	{
		storeInputOutput(c, lnode);

		if (strcmp(c->u.word[0], "exec") == 0)
			c->u.word++;

		int i = 1;
		while (c->u.word[i] != 0)
		{
			if (c->u.word[i][0] != '-')
			{
				lnode->rsize++;
				lnode->read_list = (char **)checked_realloc(lnode->read_list, sizeof(char *) * lnode->rsize);
				lnode->read_list[lnode->rsize - 1] = c->u.word[i];
			}
			i++;
		}
	}
	else if (c->type == SUBSHELL_COMMAND)
	{
		storeInputOutput(c, lnode);
		processCommand(c->u.subshell_command, lnode);
	}
	else
	{
		processCommand(c->u.command[0], lnode);
		processCommand(c->u.command[1], lnode);
	}
}

bool
compare_RW_list(char **prev, char **cur, int psize, int csize)
{
	int i = 0;
	for (; i < csize; i++)
	{
		int j = 0;
		for (; j < psize; j++)
		{
			if (strcmp(cur[i], prev[j]) == 0)
				return true;
		}
	}
	return false;
}

bool
has_dependencies(list_node_t lprev, list_node_t lnode)
{
	return compare_RW_list(lprev->write_list, lnode->write_list, lprev->wsize, lnode->wsize) ||
		compare_RW_list(lprev->write_list, lnode->read_list, lprev->wsize, lnode->rsize) ||
		compare_RW_list(lprev->read_list, lnode->write_list, lprev->rsize, lnode->wsize);
}

dependency_graph_t
create_graph(command_stream_t command_stream)
{
	list_node_t lhead = NULL;
	list_node_t ltail = NULL;

	dependency_graph_t graph = (dependency_graph_t)checked_malloc(sizeof(struct dependency_graph));
	graph->no_dependencies = NULL;
	graph_node_t no_dep_tail = NULL;
	graph->dependencies = NULL;
	graph_node_t dep_tail = NULL;

	if (command_stream == NULL)
		return NULL;

	for (; command_stream->cur != NULL; command_stream->cur = command_stream->cur->next)
	{
		command_t cmd = command_stream->cur->comm;

		graph_node_t gnode = (graph_node_t)checked_malloc(sizeof(struct graph_node));
		gnode->command = cmd;
		gnode->before = NULL;
		gnode->pid = 1;
		gnode->next = NULL;

		list_node_t lnode = (list_node_t)checked_malloc(sizeof(struct list_node));
		lnode->node = gnode;
		lnode->read_list = NULL;
		lnode->write_list = NULL;
		lnode->rsize = 0;
		lnode->wsize = 0;
		lnode->next = NULL;
		if (lhead == NULL)
			lhead = ltail = lnode;
		else
		{
			ltail->next = lnode;
			ltail = lnode;
		}


		//1.  generate RL, WL for command tree K
		processCommand(cmd, lnode);

		//2.  check if there exists dependency between this current command tree k and the command tree before it
		list_node_t lprev = lhead;
		int cnt = 0;
		while (lprev != lnode)
		{
			if (has_dependencies(lprev, lnode))
			{
				cnt++;
				gnode->before = (graph_node_t *)checked_realloc(lnode->node->before, sizeof(graph_node_t)*cnt);
				gnode->before[cnt-1] = lprev->node;
			}
			lprev = lprev->next;
		}

		//3. check if “before” list is NULL
		if (lnode->node->before == NULL)
		{
			if (graph->no_dependencies == NULL)
				graph->no_dependencies = no_dep_tail = gnode;
			else
			{
				no_dep_tail->next = gnode;
				no_dep_tail = gnode;
			}
		}
		else
		{
			if (graph->dependencies == NULL)
				graph->dependencies = dep_tail = gnode;
			else
			{
				dep_tail->next = gnode;
				dep_tail = gnode;
			}
		}
	}
	return graph;
}

void
executeNoDependencies(graph_node_t no_dep)
{
	graph_node_t cur = no_dep;
	while (cur != NULL)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			execute_command(cur->command, true);
			exit(0);
		}
		else
			cur->pid = pid;

		cur = cur->next;
	}
}

void
executeDependencies(graph_node_t dep)
{
	graph_node_t cur = dep;
	while (cur != NULL)
	{
		int status;
		if (cur->before != NULL)
		{
			int i = 0;
			graph_node_t n = cur->before[0];
			while (n != NULL)
			{
				waitpid(n->pid, &status, 0);
				n = cur->before[++i];
			}
		}
		pid_t pid = fork();
		if (pid == 0){
			execute_command(cur->command, true);
			exit(0);
		}
		else
			cur->pid = pid;

		cur = cur->next;
	}
}

int
execute_graph(dependency_graph_t graph)
{
	if (graph == NULL)
		return 0;
	executeNoDependencies(graph->no_dependencies);
	executeDependencies(graph->dependencies);
	if (graph->dependencies == NULL)
		return graph->no_dependencies->command->status;
	return graph->dependencies->command->status;
}