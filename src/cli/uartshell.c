/*
 * 	\file 		uartshell.c
 *  \author 	Tiago Machado Gabardo
 *
 *	This file contains code for a simple command shell. This is used to provide
 *	a command interface for embedded softwares. It manages commands and
 *	parameters that are passed to an interpreter that must be provided by
 *	specific application software.
 */
#include "uartshell.h"
#include "hal_config.h"

/**
 * [[config_cli]]
 * Constants below must be adapted to fit application requirements
 */
#define LEN_RXBUFFER	51			/**< Length of receiving buffer*/
/**< Maximum arguments that can be stored for one command*/
#define MAX_ARGUMENTS	10
#define PROMPT_STR		"xga> "	/**< String used as prompt*/

#define ENDSTRING		0
#define NEWLINE			"\r\n"
#define ERASE			"\b \b"
#define PROMPT			NEWLINE PROMPT_STR

/**
 * Error indicators.
 * \attention This values are used as msgPanel indexes. Care must be taken
 * when defining new values to avoid memory leak problems.
 */
#define NO_ERROR    0   /**< No error detected */
#define ERR_BUFFER	1	/**< Error on receiving buffer*/
#define ERR_ARGS	2	/**< Error on arguments */

/**
 * Characters definition
 */
#define BKSPC	0x7F
#define TAB		0x09
#define ENTER	0x0D
#define ESC		0x1B
#define SPACE	0x20
;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-sign"
/**
 * Messages that are displayed when some errors occurs.
 */
static const int8_t *msgPanel[] =
{
NEWLINE "ERROR: Rx buffer full! Reseting the shell..." PROMPT,
NEWLINE "ERROR: Arguments overflow! Reseting the shell..." PROMPT,
};
#pragma GCC diagnostic pop


static uint8_t ix;		/**< index of rxBuffer. Points to next free position*/
static uint8_t ixArg;	/**< index of argc buffer.*/
/**< boolean value used to indicate that a new command is available*/
static uint8_t newCmd;
/**< array with last received char. last position is set with end string char*/
static int8_t lchar[2];
static int8_t *argc[MAX_ARGUMENTS];		/**< array of arguments*/
static uint8_t rxBuffer[LEN_RXBUFFER];	/**< array of received characters*/

static bool shell_validChar(uint8_t c);
static uint8_t shell_addChar(uint8_t c);
static uint8_t shell_removeChar(void);
static uint8_t shell_addArgument(uint8_t *arg);
static void shell_removeArgument(void);

/**
 * \brief Initialize shell
 */
void shell_init(void)
{
	lchar[0] = lchar[1] = 0;
	shell_reset();
	newCmd = 0;
}

/**
 * \brief Reset reception and argument buffers
 */
void shell_reset(void)
{
	for(ix = 0; ix < MAX_ARGUMENTS; ix++)
		argc[ix] = NULL;
	for(ix = 0; ix < LEN_RXBUFFER; ix++)
		rxBuffer[ix] = 0;
	ix = 0;
	ixArg = 0;
	newCmd = 0;
}

/**
 * \brief verify if a character is valid
 * \param[in] c character to verify
 * \return 0 if c is invalid, 1 otherwise
 */
bool shell_validChar(uint8_t c)
{
	return (c >= 0x20 && c <= 0x7E);
}

/**
 * \brief adds an argument to arguments buffer
 * \param[in] arg initial position of the argument inside character vector
 * \return integer indicating the status of operation
 * \see Error indicators
 */
uint8_t shell_addArgument(uint8_t *arg)
{
	uint8_t st = NO_ERROR;
	if(ixArg >= MAX_ARGUMENTS)
		st = ERR_ARGS;
	else
	{
		argc[ixArg] = (int8_t*)arg;
		ixArg++;
		st = NO_ERROR;
	}
	return st;
}

/**
 * \brief remove an argument from arguments buffer
 */
void shell_removeArgument(void)
{
	if(ixArg > 0)
		argc[--ixArg] = NULL;
}

/**
 * \brief add a character to the buffer of received characters
 * \parameter[in] c character to add
 * \return integer indicating the status of operation
 * \see Error indicators
 */
uint8_t shell_addChar(uint8_t c)
{
	uint8_t st = NO_ERROR;
	int8_t prev = (ix > 0) ? rxBuffer[ix-1] : -1; //get previous character
	if(ix >= LEN_RXBUFFER-1)	//verify if buffer is full
		st = ERR_BUFFER;
	//ignore double spaces and space as 1st char
	else if((SPACE == c)&&(ix == 0 || prev == ENDSTRING))
		st = NO_ERROR;
	else
	{
		if(prev == ENDSTRING || prev == -1) //detect new argument
			st = shell_addArgument(&rxBuffer[ix]);
		if(NO_ERROR == st) //add new char to the buffer
		{
			rxBuffer[ix] = (c == SPACE) ? ENDSTRING : c;
			ix++;
			st = NO_ERROR;
		}
	}
	return st;
}

/**
 * \brief remove character from buffer
 * \return integer indicating the status of operation 0: character not removed,
 * 1: removed
 */
uint8_t shell_removeChar(void)
{
	int8_t prev = -1;
	uint8_t removed = 0;
	if(ix > 0)
	{
		ix--;
		rxBuffer[ix] = 0;
		prev = rxBuffer[ix-1];
		removed = 1;
	}
	//detect argument and remove it from args list
	if(ix == 0 || prev == ENDSTRING)
		shell_removeArgument();
	return removed;
}

/**
 * \brief get prompt string
 * \return string
 */
int8_t *shell_getPrompt(void)
{
	return (int8_t*)PROMPT;
}

/**
 * \brief Indicates if a new command is available
 * \return true or false
 */
bool shell_newCommand(void)
{
	return newCmd;
}

/**
 * \brief Mark as command as read, without
 * erasing the arguments buffer
 */
void shell_markRead(void)
{
	newCmd = 0;
}

/**
 * \brief Read arguments
 * \param[out] args vector of arguments
 * \return number of arguments
 */
uint8_t shell_getArgs(int8_t ***args)
{
	*args = argc;
	return ixArg;
}

/**
 * \brief receive a new character to process
 * \pram[in] c new character
 * \return string to be printed as an information for the user
 */
int8_t *shell_newChar(int8_t c)
{
	int8_t *ret = NULL; //returned string
	uint8_t st;		//status flag
	*lchar = c;		//stores last received char

	if(newCmd)		//ignore new commands when there is existing one
	{
		*lchar = 0;
		ret = lchar;
	}
	//enter ends command parsing and call the interpreter
	else if(ENTER == *lchar)
	{
		rxBuffer[ix] = ENDSTRING;
		newCmd = 1;
		*lchar = 0;
		ret = lchar;
	}
	else if(BKSPC == *lchar) //removes a character
	{
		*lchar = 0;
		ret = (shell_removeChar()) ? ((int8_t*)ERASE) : lchar;
	}
	else if(shell_validChar(*lchar)) //add a new character
	{
		st = shell_addChar(*lchar);
		if(NO_ERROR == st)
			ret = lchar; //echo character
		else //error detected. reset shell and return error message
		{
			shell_reset();
			ret = (int8_t*)msgPanel[st-1];
		}
	}
	return ret;
}
