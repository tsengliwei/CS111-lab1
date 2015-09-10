// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"

#include <error.h>

/* FIXME: You may need to add #include directives, macro definitions,
static function definitions, etc.  */
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include "alloc.h"
#include <string.h> 
#include <stdlib.h>

/* FIXME: Define the type 'struct command_stream' here.  This should
complete the incomplete type declaration in command.h.  */

int lineNumber = 1;
int wd_size = 0;
char *wd = NULL; // as a string
command_t cmd = NULL;
int word_size = 0; // for cmd->u.word

enum op_type
{
	SEMICOLON,
	AND,
	OR,
	PIPE,
	PARENTHESIS,
	EXCEPTION
};

typedef struct stack_op
{
	int size;
	int capacity;
	enum op_type *elements;
} stack_op;

stack_op *
initOpStack(int cap)
{
	stack_op *stk = (stack_op *)checked_malloc(sizeof(stack_op));
	stk->size = 0;
	stk->capacity = cap;
	stk->elements = (enum op_type *)checked_malloc(sizeof(enum op_type) * cap);
	return stk;
}

void
pushOp(stack_op *stk, char *str)
{
	if (stk->size == stk->capacity)
	{
		stk->capacity *= 2;
		stk->elements = (enum op_type *)checked_realloc(stk->elements, sizeof(enum op_type) * stk->capacity);
	}

	enum op_type opType;
	if (strcmp(str, ";") == 0)
		opType = SEMICOLON;
	else if (strcmp(str, "&&") == 0)
		opType = AND;
	else if (strcmp(str, "||") == 0)
		opType = OR;
	else if (strcmp(str, "|") == 0)
		opType = PIPE;
	else if (strcmp(str, "(") == 0)
		opType = PARENTHESIS;
	else
		opType = EXCEPTION;

	stk->elements[stk->size++] = opType;
}

enum op_type
	topOp(stack_op *stk)
{
	if (stk->size == 0)
		return EXCEPTION;

	return stk->elements[stk->size - 1];
}

enum op_type
	popOp(stack_op *stk)
{
	if (stk->size == 0)
		return EXCEPTION;

	stk->size--;
	return stk->elements[stk->size];
}

typedef struct stack_cmd
{
	int size;
	int capacity;
	command_t *elements;
} stack_cmd;

stack_cmd *
initStackCmd(int cap)
{
	stack_cmd *stk = (stack_cmd *)checked_malloc(sizeof(stack_cmd));
	stk->size = 0;
	stk->capacity = cap;
	stk->elements = (command_t *)checked_malloc(sizeof(command_t) * cap);
	return stk;
}

void
pushCmd(stack_cmd *stk, command_t cmd)
{
	if (stk->size == stk->capacity)
	{
		stk->capacity *= 2;
		stk->elements = (command_t *)checked_realloc(stk->elements, sizeof(command_t) * stk->capacity);
	}

	stk->elements[stk->size++] = cmd;
}

command_t
topCmd(stack_cmd *stk)
{
	if (stk->size == 0)
		return NULL;

	return stk->elements[stk->size - 1];
}

command_t
popCmd(stack_cmd *stk)
{
	if (stk->size == 0)
		return NULL;

	return stk->elements[--stk->size];
}


bool
isValidWordChar(char c)
{
	return (isalpha(c)
		|| isdigit(c)
		|| (c == '!')
		|| (c == '%')
		|| (c == '+')
		|| (c == ',')
		|| (c == '-')
		|| (c == '.')
		|| (c == '/')
		|| (c == ':')
		|| (c == '@')
		|| (c == '^')
		|| (c == '_'));
}

/* || and && needs further treatment.  */
bool
isSpecialToken(char c)
{
	return ((c == ';')
		|| (c == '|')
		|| (c == '&')
		|| (c == '(')
		|| (c == ')')
		|| (c == '<')
		|| (c == '>'));
}

/* '\n' needs special treatment  */
bool
isWhiteSpace(char c)
{
	return ((c == ' ')
		|| (c == '\n')
		|| (c == '\t'));
}

void
checkValidInput(char c)
{
	if (!isValidWordChar(c) && !isSpecialToken(c) && !isWhiteSpace(c))
	{
		fprintf(stderr, "%d: syntax error\n", lineNumber);
		exit(1);
	}
}

//functions that will remove all spaces, while keeping track of line number 
void
consumeWhiteSpaces(void *stream)
{
	char c;
	while ((c = getc(stream)) != EOF)
	{
		//check for whitespaces, which includes newline
		if (isWhiteSpace(c))
		{
			//if newline, update the count
			if (c == '\n')
				lineNumber++;
			continue;
		}
		ungetc(c, stream);
		return;
	}
	ungetc(c, stream);
}

void
consumeSpaces(void *stream)
{
	char c;
	while ((c = getc(stream)) != EOF)
	{
		//check for space, excluding newline
		if (c == ' ' || c == '\t')
			continue;
		ungetc(c, stream);
		return;
	}
	ungetc(c, stream);
}

void
makeSimpleCommand(stack_cmd *cmdStack)
{
	if (wd != NULL)
	{
		if (cmd == NULL)
		{
			cmd = (command_t)checked_malloc(sizeof(struct command));
			cmd->type = SIMPLE_COMMAND;
			cmd->status = -1;
			cmd->input = cmd->output = NULL;
			cmd->u.word = NULL;
			pushCmd(cmdStack, cmd);
		}
		cmd->u.word = (char **)checked_realloc(cmd->u.word, sizeof(char *) * (1 + word_size));
		wd = (char *)checked_realloc(wd, sizeof(char) * (1 + wd_size));
		wd[wd_size] = 0; // Null byte
		cmd->u.word[word_size++] = wd;

		wd = NULL;
		wd_size = 0;
	}
}

void
nullifyCmd()
{
	if (cmd != NULL)
	{
		cmd->u.word = (char **)checked_realloc(cmd->u.word, sizeof(char *) * (1 + word_size));
		cmd->u.word[word_size] = 0;
		word_size = 0;
		cmd = NULL;
	}
}

void
combineCommand(command_t newCmd, command_t cmdRight, command_t cmdLeft, enum op_type op)
{
	switch (op)
	{
	case SEMICOLON:
		newCmd->type = SEQUENCE_COMMAND;
		break;
	case AND:
		newCmd->type = AND_COMMAND;
		break;
	case OR:
		newCmd->type = OR_COMMAND;
		break;
	case PIPE:
		newCmd->type = PIPE_COMMAND;
		break;
	default:
		//  '(', ')' would be used up before cleaning up
		// fprintf(stderr, "%d: syntax error\n", lineNumber);
		fprintf(stderr, "%d: combineCommand error\n", lineNumber);
		exit(1);
	}
	newCmd->u.command[1] = cmdRight;
	newCmd->u.command[0] = cmdLeft;
}

// for each operator, pop 2 commands off command stack
// combine into a new command, push it onto command stack   
void
parseCommand(stack_op *opStack, stack_cmd *cmdStack)
{
	enum op_type op = popOp(opStack);
	command_t newCmd = (command_t)checked_malloc(sizeof(struct command));
	newCmd->status = -1;
	newCmd->input = newCmd->output = NULL;

	if (cmdStack->size < 2)
	{
		fprintf(stderr, "%d: parseCommand error\n", lineNumber);
		exit(1);
	}
	command_t cmdRight = popCmd(cmdStack);
	command_t cmdLeft = popCmd(cmdStack);
	combineCommand(newCmd, cmdRight, cmdLeft, op);

	pushCmd(cmdStack, newCmd);
}

void
processStacks(command_stream_t cst, stack_cmd *cmdStack, stack_op *opStack)
{
	makeSimpleCommand(cmdStack);

	while (opStack->size > 0)
		parseCommand(opStack, cmdStack);

	// add command into command stream
	if (cmdStack->size != 1)
	{
		// we should only have one command tree in command stack
		//   fprintf(stderr, "%d: syntax error\n", lineNumber);
		fprintf(stderr, "%d: processStacks 2 error\n", lineNumber);
		exit(1);
	}

	// make node
	node_t node = (node_t)checked_malloc(sizeof(struct node));
	node->comm = popCmd(cmdStack);
	node->next = NULL;

	if (cst->head == NULL)
	{
		// the linked list is  empty
		cst->cur = cst->tail = cst->head = node;
	}
	else
	{
		// the linked list is not empty
		cst->tail->next = node;
		cst->tail = cst->tail->next;
	}
}

void
processRedirection(char **str_ptr, void *stream)
{
	consumeSpaces(stream);
	char c;
	int size = 0;
	while ((c = getc(stream)) != EOF)
	{
		if (isWhiteSpace(c) || isSpecialToken(c))
		{
			if (c != ' ' && c != '\t')
				ungetc(c, stream);
			if (size == 0)
			{
				fprintf(stderr, "%d: syntax error\n", lineNumber);
				exit(1);
			}
			*str_ptr = (char *)checked_realloc(*str_ptr, sizeof(char) * (1 + size));
			(*str_ptr)[size] = 0;
			return;
		}
		*str_ptr = (char *)checked_realloc(*str_ptr, sizeof(char) * (1 + size));
		(*str_ptr)[size++] = c;
	}
	ungetc(c, stream);
}

command_stream_t
make_command_stream(int(*get_next_byte) (void *),
void *get_next_byte_argument)
{
	/* FIXME: Replace this with your implementation.  You may need to
	add auxiliary functions and otherwise modify the source code.
	You can also use external functions defined in the GNU C Library.  */

	FILE *script_stream = (FILE *)get_next_byte_argument;
	char c;

	bool isComment = false;
	bool hasOperator = false;

	stack_op *opStack = initOpStack(10);
	stack_cmd *cmdStack = initStackCmd(10);

	command_stream_t cmd_stream = (command_stream_t)checked_malloc(sizeof(struct command_stream));
	cmd_stream->head = NULL;
	cmd_stream->tail = NULL;
	cmd_stream->cur = cmd_stream->head;

	consumeWhiteSpaces(script_stream);

	while ((c = (*get_next_byte)(script_stream)) != EOF)
	{
		if (c == '#' || isComment) /* skip comment line */
		{
			isComment = true;
			if (c == '\n')
			{
				lineNumber++;
				isComment = false;
			}
			continue;
		}

		checkValidInput(c);

		if (c == ' ' || c == '\t')
		{
			makeSimpleCommand(cmdStack);
			continue; // consume spaces
		}

		if (c == '\n')
		{
		CHECKNEWLINE:
			lineNumber++;
			// case 1: bi-operands
			if (hasOperator)
			{
				// e.g. A &&\n\n\n B
				consumeWhiteSpaces(script_stream); // This includes all the following newlines
				hasOperator = false;
				continue;
			}

			// case 2: sequence command
			consumeSpaces(script_stream);
			if ((c = (*get_next_byte)(script_stream)) != EOF)
			{
				makeSimpleCommand(cmdStack);
				nullifyCmd();

				if (c == '\n') // case 3: the end of a command tree
				{
					lineNumber++;
					processStacks(cmd_stream, cmdStack, opStack);
				}
				else if (isSpecialToken(c) && c != '<' && c != '>' && c != '(')
				{
					// Except for <, >, and (, there expect to be a valid string before the special token
					fprintf(stderr, "%d: syntax error\n", lineNumber);
					exit(1);
				}
				else
				{
					if (cmdStack->size != 0)
						pushOp(opStack, ";");
					ungetc(c, script_stream);
				}
			}
			else
				break;

			continue;
		}

		if (isSpecialToken(c))
		{
			makeSimpleCommand(cmdStack);
			nullifyCmd();

			// Except for <, >, and (, special token cannot immediately follow &&, ||, or |
			//                         and there expect to be a valid string before the special token
			if (c != '<' && c != '>' && c != '(' && (hasOperator || cmdStack->size == 0))
			{
				fprintf(stderr, "%d: syntax error\n", lineNumber);
				exit(1);
			}

			// case 1: redirection
			if (c == '<' || c == '>')
			{
				command_t tmp = topCmd(cmdStack);
				if (tmp != NULL)
				{
					if (c == '<')
						processRedirection(&tmp->input, script_stream);
					else
						processRedirection(&tmp->output, script_stream);
				}
				else
				{
					fprintf(stderr, "%d: syntax error\n", lineNumber);
					exit(1);
				}
				continue;
			}

			// case 2: subshell
			if (c == ')')
			{
				command_t subShellCmd = (command_t)checked_malloc(sizeof(struct command));
				subShellCmd->status = -1;
				subShellCmd->input = subShellCmd->output = NULL;
				subShellCmd->type = SUBSHELL_COMMAND;
				while (opStack->size != 0 && topOp(opStack) != PARENTHESIS)
					parseCommand(opStack, cmdStack);

				if (opStack->size == 0)
				{
					fprintf(stderr, "%d: processStacks error\n", lineNumber);
					exit(1);
				}
				subShellCmd->u.subshell_command = popCmd(cmdStack);
				popOp(opStack);
				pushCmd(cmdStack, subShellCmd);
				continue;
			}

			// case 3: for AND, OR, PIPE
			if (c == '&')
			{
				if ((c = (*get_next_byte)(script_stream)) != EOF)
				{
					if (c == '&') // AND
					{
						hasOperator = true;
						while (opStack->size != 0 && topOp(opStack) != SEMICOLON) // precedence rule
							parseCommand(opStack, cmdStack);
						pushOp(opStack, "&&");
					}
					else
					{
						fprintf(stderr, "%d: AND error\n", lineNumber);
						exit(1);
					}
				}
				else
					break;
				continue;
			}
			else if (c == '|')
			{
				if ((c = (*get_next_byte)(script_stream)) != EOF)
				{
					hasOperator = true;

					if (c == '|') // OR
					{
						while (opStack->size != 0 && topOp(opStack) != SEMICOLON) // precedence rule
							parseCommand(opStack, cmdStack);
						pushOp(opStack, "||");
					}
					else // PIPE
					{
						while (opStack->size != 0 && topOp(opStack) == PIPE) // precedence rule
							parseCommand(opStack, cmdStack);
						pushOp(opStack, "|");
						ungetc(c, script_stream);
					}
				}
				else
					break;
				continue;
			}

			// case 4: ';' may be ignored before multiple newlines
			if (c == ';')
			{
				consumeSpaces(script_stream);
				if ((c = (*get_next_byte)(script_stream)) != EOF)
				{
					if (c == '\n')
						goto CHECKNEWLINE;
					else
					{
						while (opStack->size != 0) // precedence rule
							parseCommand(opStack, cmdStack);
						pushOp(opStack, ";");
						ungetc(c, script_stream);
					}
				}
				else
					break;
				continue;
			}

			// '('
			if (cmd != NULL)
			{
				fprintf(stderr, "%d: syntax error\n", lineNumber);
				exit(1);
			}
			pushOp(opStack, "(");
			continue;
		}

		hasOperator = false;
		wd = (char *)checked_realloc(wd, sizeof(char) * (1 + wd_size));
		wd[wd_size++] = c;

	}

	makeSimpleCommand(cmdStack);
	nullifyCmd();
	if (cmdStack->size != 0 || opStack->size != 0)
		processStacks(cmd_stream, cmdStack, opStack);

	return cmd_stream;
}

command_t
read_command_stream(command_stream_t cs)
{
	/* FIXME: Replace this with your implementation too.  */
	if (cs == NULL || cs->cur == NULL)
		return NULL;

	command_t tmp = cs->cur->comm;
	cs->cur = cs->cur->next;
	return tmp;
}
