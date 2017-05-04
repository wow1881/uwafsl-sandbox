/****************************************************************************
 * examples/elf/tests/signal/signal.c
 *
 *   Copyright (C) 2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/****************************************************************************
 * Definitions
 ****************************************************************************/

#define USEC_PER_MSEC 1000
#define MSEC_PER_SEC  1000
#define USEC_PER_SEC  (USEC_PER_MSEC * MSEC_PER_SEC)
#define SHORT_DELAY   (USEC_PER_SEC / 3)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int sigusr1_rcvd = 0;
static int sigusr2_rcvd = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: siguser_action
 ***************************************************************************/

/* NOTE: it is necessary for functions that are referred to by function pointers
 *  pointer to be declared with global scope (at least for ARM).  Otherwise,
 * a relocation type that is not supported by ELF is generated by GCC.
 */

void siguser_action(int signo, siginfo_t *siginfo, void *arg)
{
  printf("siguser_action: Received signo=%d siginfo=%p arg=%p\n",
         signo, siginfo, arg);

  if (signo == SIGUSR1)
    {
      printf("  SIGUSR1 received\n");
      sigusr2_rcvd = 1;
    }
  else if (signo == SIGUSR2)
    {
      printf("  SIGUSR2 received\n");
      sigusr1_rcvd = 2;
    }
  else
    {
      printf("  ERROR: Unexpected signal\n");
    }

  if (siginfo)
    {
      printf("siginfo:\n");
      printf("  si_signo  = %d\n",  siginfo->si_signo);
      printf("  si_code   = %d\n",  siginfo->si_code);
      printf("  si_value  = %d\n",  siginfo->si_value.sival_int);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 ****************************************************************************/

int main(int argc, char **argv)
{
  struct sigaction act;
  struct sigaction oact1;
  struct sigaction oact2;
  pid_t mypid = getpid();
  union sigval sigval;
  int status;

  printf("Setting up signal handlers from pid=%d\n", mypid);

  /* Set up so that siguser_action will respond to SIGUSR1 */

  memset(&act, 0, sizeof(struct sigaction));
  act.sa_sigaction = siguser_action;
  act.sa_flags     = SA_SIGINFO;

  (void)sigemptyset(&act.sa_mask);

  status = sigaction(SIGUSR1, &act, &oact1);
  if (status != 0)
    {
      fprintf(stderr, "Failed to install SIGUSR1 handler, errno=%d\n",
              errno);
      exit(2);
    }

  printf("Old SIGUSR1 sighandler at %p\n", oact1.sa_handler);
  printf("New SIGUSR1 sighandler at %p\n", siguser_action);

  /* Set up so that siguser_action will respond to SIGUSR2 */

  status = sigaction(SIGUSR2, &act, &oact2);
  if (status != 0)
    {
      fprintf(stderr, "Failed to install SIGUSR2 handler, errno=%d\n",
              errno);
      exit(2);
    }

  printf("Old SIGUSR2 sighandler at %p\n", oact2.sa_handler);
  printf("New SIGUSR2 sighandler at %p\n", siguser_action);
  printf("Raising SIGUSR1 from pid=%d\n",  mypid);

  fflush(stdout); usleep(SHORT_DELAY);

  /* Send SIGUSR1 to ourselves via kill() */

  printf("Kill-ing SIGUSR1 from pid=%d\n", mypid);
  status = kill(mypid, SIGUSR1);
  if (status != 0)
    {
      fprintf(stderr, "Failed to kill SIGUSR1, errno=%d\n", errno);
      exit(3);
    }

  usleep(SHORT_DELAY);
  printf("SIGUSR1 raised from pid=%d\n", mypid);

  /* Verify that we received SIGUSR1 */

  if (sigusr1_rcvd == 0)
    {
      fprintf(stderr, "SIGUSR1 not received\n");
      exit(4);
    }

  sigusr1_rcvd = 0;

  /* Send SIGUSR2 to ourselves */

  printf("sigqueue-ing SIGUSR2 from pid=%d\n", mypid);
  fflush(stdout); usleep(SHORT_DELAY);

  /* Send SIGUSR2 to ourselves via sigqueue() */

  sigval.sival_int = 87;
  status = sigqueue(mypid, SIGUSR2, sigval);
  if (status != 0)
    {
      fprintf(stderr, "Failed to queue SIGUSR2, errno=%d\n", errno);
      exit(5);
    }

  usleep(SHORT_DELAY);
  printf("SIGUSR2 queued from pid=%d, sigval=87\n", mypid);

  /* Verify that SIGUSR2 was received */

  if (sigusr2_rcvd == 0)
    {
      fprintf(stderr, "SIGUSR2 not received\n");
      exit(6);
    }

  sigusr2_rcvd = 0;

  /* Remove the siguser_action handler and replace the SIGUSR2
   * handler with sigusr2_sighandler.
   */

  printf("Resetting SIGUSR2 signal handler from pid=%d\n", mypid);

  status = sigaction(SIGUSR2, &oact2, &act);
  if (status != 0)
    {
      fprintf(stderr, "Failed to install SIGUSR1 handler, errno=%d\n",
              errno);
      exit(2);
    }

  printf("Old SIGUSR1 sighandler at %p\n", act.sa_handler);
  printf("New SIGUSR1 sighandler at %p\n", oact1.sa_handler);

  /* Verify that the handler that was removed was siguser_action */

  if ((void*)act.sa_handler != (void*)siguser_action)
    {
      fprintf(stderr,
              "Old SIGUSR2 signal handler (%p) is not siguser_action (%p)\n",
              act.sa_handler, siguser_action);
      exit(8);
    }

  /* Send SIGUSR2 to ourselves via kill() */

  printf("Killing SIGUSR2 from pid=%d\n", mypid);
  fflush(stdout); usleep(SHORT_DELAY);

  status = kill(mypid, SIGUSR2);
  if (status != 0)
    {
      fprintf(stderr, "Failed to kill SIGUSR2, errno=%d\n", errno);
      exit(9);
    }

  usleep(SHORT_DELAY);
  printf("SIGUSR2 killed from pid=%d\n", mypid);

  /* Verify that SIGUSR2 was received */

  if (sigusr2_rcvd == 0)
    {
      fprintf(stderr, "SIGUSR2 not received\n");
      exit(10);
    }

  sigusr2_rcvd = 0;

  return 0;
}
