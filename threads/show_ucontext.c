#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "interrupt.h"

#ifndef __x86_64__
#error "Do this project on a 64-bit x86-64 linux machine"
#endif /* __x86_64__ */

#if __WORDSIZE != 64
#error "word size should be 64 bits"
#endif

static void call_setcontext(ucontext_t * context);
static void show_interrupt(void);

/* zero out the context */
ucontext_t mycontext = { 0 };

void
get_start_end(long *startp, long *endp)
{
	char filename[128];
	char line[256];
	FILE *f;
	char ex = 0;
        int matches = 0;
	
	sprintf(filename, "/proc/%d/maps", getpid());
	f = fopen(filename, "r");
	assert(f);
	do {
		/* find line that has executable region */
		if (fgets(line, sizeof(line), f) != NULL) {
		        matches = sscanf(line, "%lx-%lx %*c%*c%c%*c ", startp,
					 endp, &ex);
		} else {
			matches = EOF;
		}
	} while (ex != 'x' && matches > 0);
}

int
main(int argc, char **argv)
{
	long start, end;
	/* we declare this variable to be volatile so that the compiler will
	 * make sure to store it on the stack and not in a register. This is
	 * ESSENTIAL, or else the code in this function may run in an infinite
	 * loop (you can try this by removing the volatile keyword, and
	 * compiling this file with the "-O2" flag to the gcc compiler, by
	 * changing CFLAGS in the Makefile).
	 *
	 * QUESTION: why must setcontext_called be stored on the stack, and not
	 * in a register? You will need to look at the code below, and
	 * understand how getcontext and setcontext work to answer this
	 * question. */
	volatile int setcontext_called = 0;
	int err;

	/*
	 * DO NOT CHANGE/ADD ANY CODE UNTIL BELOW TBD(). SEE BELOW.
	 */

	/* Get context: make sure to read the man page of getcontext in detail,
	 * or else you will not be able to answer the questions below. */
	err = getcontext(&mycontext);
	assert(!err);

	/* QUESTION: which of the fields of mycontext changed due to the call
	 * above? Hint: It will help to run the program using gdb and put a
	 * breakpoint at entry to main and before and after the calls to
	 * getcontext().
	 * - Use "info registers" to see the values of the registers.
	 * - Use "next"/"step" to advance to the next line of code.
	 * - Use "print mycontext" to see the values stored in mycontext.
	 *   Compare them with the output of "info registers".
	 * - Use "ptype mycontext" so see the type/fields of mycontext */

	printf("%s: setcontext_called = %d\n", __FUNCTION__, setcontext_called);
	if (setcontext_called == 1) {
		/* QUESTION: will we get here? Why or why not? */
		show_interrupt();
		exit(0);
	}

	get_start_end(&start, &end);
	printf("start = 0x%lx\n", start);
	printf("end = 0x%lx\n", end);
	/*
	 * comment out TBD, and replace the -1 in each printf() with what is
	 * expected to be printed.
	 */
	//TBD();

	/* show size of ucontext_t structure. Hint: use sizeof(). */
	printf("ucontext_t size = %ld bytes\n", (long int)sizeof(ucontext_t));

	/* now, look inside of the context you just saved. */

	/* first, think about code */
	/* the program counter is called rip in x86-64 */
	printf("memory address of main() = 0x%lx\n", (unsigned long)main);
	printf("memory address of the program counter (RIP) saved "
	       "in mycontext = 0x%lx\n",
	       (unsigned long)mycontext.uc_mcontext.gregs[REG_RIP]);

	/* Now, think about parameters. */
	printf("argc = %d\n", argc);
	printf("argv = %p\n", (void *)argv);
	/* QUESTIONS: How are these parameters passed into the main function? 
	 * Are there any saved registers in mycontext that store the parameter
	 * values above. Why or why not? Hint: Use gdb, and then run
	 * "disassemble main" in gdb, and then scroll up to see the beginning of
	 * the main function. 
	 */ 

	/* Now, think about the stack. */
	/* QUESTIONS: Are setcontext_called and err stored on the stack? 
	 * Does the stack grow up or down? What stack-related data is stored in
	 * mycontext.uc_mcontext.gregs[]? 
	 */
	printf("memory address of the variable setcontext_called = %p\n",
	       (void *)&setcontext_called);
	printf("memory address of the variable err = %p\n",
	       (void *)&err);
	printf("number of bytes pushed to the stack between setcontext_called "
	       "and err = %ld\n", (unsigned long)(&setcontext_called - &err));

	printf("stack pointer register (RSP) stored in mycontext = 0x%lx\n",
	       (unsigned long)mycontext.uc_mcontext.gregs[REG_RSP]);

	printf("number of bytes between err and the saved stack in mycontext "
	       "= %ld\n", (unsigned long)&err - (unsigned long)mycontext.uc_mcontext.gregs[REG_RSP]);

	/* QUESTION: What is the value of the uc_stack field in mycontext?
	 * Note that this field is used to store an alternate stack for use
	 * during signal handling, and is NOT the stack of the running thread. 
	 */
	printf("value of uc_stack.ss_sp = 0x%lx\n",
	       (unsigned long)mycontext.uc_stack.ss_sp);

	/* Don't move on to the next part of the lab until you know how to
	 * change the stack in a context when you manipulate a context to create
	 * a new thread. 
	 */

	/* Now we will try to understand how setcontext works. */
	setcontext_called = 1;
	call_setcontext(&mycontext);
	
	/* QUESTION: why does the program not fail at the assert below? */
	assert(0);
}

static void
call_setcontext(ucontext_t * context)
{
	int err = setcontext(context);
	assert(!err);
}

static void
show_interrupt(void)
{
	int err;

	/* QUESTION: how did we get here if there was an assert above? */

	/* now think about interrupts. you will need to understand how they
	 * work, and how they interact with get/setcontext for implementing
	 * preemptive threading. */

	/* QUESTION: what does interrupts_on() do? see interrupt.c */
	interrupts_on();

	/* getcontext stores the process's signal mask */
	err = getcontext(&mycontext);
	assert(!err);

	/* QUESTION: Are interrupts masked (i.e., disabled) in mycontext?
	 * HINT: use sigismember below. */
	printf("interrupt is disabled = %d\n",
	       (unsigned int)sigismember(&mycontext.uc_sigmask, SIG_TYPE));

	interrupts_off();

	err = getcontext(&mycontext);
	assert(!err);

	/* QUESTION: which fields of mycontext changed as a result of the
	 * getcontext call above? */
	printf("interrupt is disabled = %d\n",
	       (unsigned int)sigismember(&mycontext.uc_sigmask, SIG_TYPE));
}
