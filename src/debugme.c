/*
 * Copyright 2016-2022 Yury Gribov
 * 
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE.txt file.
 */

#include <debugme.h>
#include "common.h"
#include "gdb.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include <unistd.h>

unsigned dbg_flags;
const char *dbg_opts;
int init_done;
int debug;
int disabled;
int quiet;

// Interface with debugger
EXPORT volatile int __debugme_go;

static void sighandler(int sig) {
  const char *signame = sigabbrev_np(sig);
  SAFE_MSG("\x1B[1;31mlibdebugme: caught signal ");
  SAFE_MSG(signame);
  SAFE_MSG("\x1B[0m\n");
  debugme_debug(dbg_flags, dbg_opts);
  exit(1);
}

// TODO: optionally preserve existing handlers
// TODO: init if not yet (here and in debugme_debug)
EXPORT int debugme_install_sighandlers(unsigned dbg_flags_, const char *dbg_opts_) {
  if(disabled)
    return 0;

  dbg_flags = dbg_flags_;
  dbg_opts = dbg_opts_;

  int bad_signals[] = { SIGILL, SIGABRT, SIGFPE, SIGSEGV, SIGBUS, SIGSYS };

  struct sigaction sa = {};
  sa.sa_handler = sighandler;
  sigemptyset(&sa.sa_mask);

  if (dbg_flags & DEBUGME_ALTSTACK) {
    static char ALIGNED(16) stack[1024*1024*64];
    stack_t st;
    st.ss_sp = stack;
    st.ss_flags = 0;
    st.ss_size = sizeof(stack);
    if(0 != sigaltstack(&st, 0)) {
      perror("libdebugme: failed to install altstack");
    } else {
      sa.sa_flags |= SA_ONSTACK;
    }
  }

  size_t i;
  for(i = 0; i < ARRAY_SIZE(bad_signals); ++i) {
    int sig = bad_signals[i];
    const char *signame = strsignal(sig);
    if(debug) {
      fprintf(stderr, "debugme: setting signal handler for signal %d (%s)\n", sig, signame);
    }
    if(0 > sigaction(sig, &sa, NULL)) {
      fprintf(stderr, "libdebugme: failed to intercept signal %d (%s)\n", sig, signame);
    }
  }

  return 1;
}

EXPORT int debugme_debug(unsigned dbg_flags_, const char *dbg_opts_) {
  // Note that this function and it's callee's should be signal-safe

  if(disabled)
    return 0;

  static int in_debugme_debug;
  if(!in_debugme_debug)
    in_debugme_debug = 1;  // TODO: make this thread-safe
  else {
    SAFE_MSG("libdebugme: can't attach more than one debugger simultaneously\n");
    return 1;
  }

  /*
  // TODO: select from the list of frontends (gdbserver, gdb+xterm, kdebug, ddd, etc.)
  if(!run_gdb(dbg_flags_, dbg_opts_ ? dbg_opts_ : ""))
    return 0;

  // TODO: raise(SIGSTOP) and wait for gdb? But that's not signal/thread-safe...

  size_t us = 0;
  while(!__debugme_go) {  // Wait for debugger to unblock us
    usleep(10);
    us += 10;
    if(us > 5000000) {  // Allow for 5 sec. delay
      SAFE_MSG("libdebugme: debugger failed to attach\n");
      return 0;
    }
  }

  __debugme_go = 0;

#if defined(__i386__) || defined(__x86_64__)
  asm volatile("int $3;");
#else
  // TODO: ARM
  raise(SIGTRAP);
#endif
  */
  SAFE_MSG("\x1B[1;31mlibdebugme: suspending process\x1B[0m\n");
  raise(SIGSTOP);

  in_debugme_debug = 0;
  return 1;
}

