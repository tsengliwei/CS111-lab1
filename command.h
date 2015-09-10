// UCLA CS 111 Lab 1 command interface

#include <stdbool.h>
#include <unistd.h>

typedef struct command *command_t;
typedef struct command_stream *command_stream_t;
typedef struct node *node_t;
typedef struct graph_node *graph_node_t;
typedef struct list_node *list_node_t;
typedef struct dependency_graph *dependency_graph_t;

struct node
{
	command_t comm;
	node_t next;
};

struct command_stream
{
	node_t head;
	node_t tail;
	node_t cur;
};

struct graph_node {
	command_t command;
	graph_node_t *before;
	pid_t pid; // initialized to 1
	graph_node_t next;
};

struct list_node {
	graph_node_t node;
	char **read_list;
	char **write_list;
	int rsize; // size of read_list
	int wsize; // size of write_list
	list_node_t next;
};

struct dependency_graph {
	graph_node_t no_dependencies;
	graph_node_t dependencies;
};

/* Create a command stream from GETBYTE and ARG.  A reader of
the command stream will invoke GETBYTE (ARG) to get the next byte.
GETBYTE will return the next input byte, or a negative number
(setting errno) on failure.  */
command_stream_t make_command_stream(int(*getbyte) (void *), void *arg);

/* Read a command from STREAM; return it, or NULL on EOF.  If there is
an error, report the error and exit instead of returning.  */
command_t read_command_stream(command_stream_t stream);

/* Print a command to stdout, for debugging.  */
void print_command(command_t);

/* Execute a command.  Use "time travel" if the flag is set.  */
void execute_command(command_t, bool);

/* Return the exit status of a command, which must have previously
been executed.  Wait for the command, if it is not already finished.  */
int command_status(command_t);

int execute_graph(dependency_graph_t);

dependency_graph_t create_graph(command_stream_t);