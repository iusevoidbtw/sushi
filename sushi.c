/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 */

/*
 * ===========================================================================
 * togglable configuration options.
 * undefine (comment out) a macro to disable that option.
 */

/* #define ENABLE_PLEDGE */  /* enable usage of OpenBSD's pledge(2). */
/* #define REPORT_SIGINT */  /* report if a process was killed by SIGINT. */
#define REPORT_SIGPIPE       /* report if a process was killed by SIGPIPE. */

/*
 * ===========================================================================
 * numeric configuration options.
 */

/*
 * child process exit status when an error happens that isn't a
 * [ENOEXEC] error or a failure to find an executable file.
 *
 * must not be 0, 126 or 127 for POSIX reasons.
 */
#define MISC_FAILURE_STATUS 125

/*
 * how much to add to the process exit status if it was killed by
 * a signal, e.g SIGINT (signal 2) = exit status of (SIGNAL_EXITSTATUS + 2)
 *
 * must be greater than 128 for POSIX reasons.
 */
#define SIGNAL_EXITSTATUS 384

/*
 * how much elements to malloc for a command's argv array initially.
 * if this is exceeded the array size will be increased by this amount
 * using realloc.
 */
#define ARGV_ALLOC_SIZE 256

/*
 * ===========================================================================
 * compatibility stuff with some platforms
 */
#if defined(__dietlibc__)
/* dietlibc needs _GNU_SOURCE for strsignal() and getline() */
#define _GNU_SOURCE

#if defined(__x86_64__)
/* needed to work around a bug in dietlibc */
#include <stdint.h>
typedef uint64_t __u64;
#endif /* __x86_64__ */

#endif /* __dietlibc__ */

#if defined(ENABLE_PLEDGE)
/* OpenBSD needs _BSD_SOURCE for pledge() */
#define _BSD_SOURCE
#endif /* ENABLE_PLEDGE */

/*
 * ===========================================================================
 * includes
 */
#define _POSIX_C_SOURCE 200809L
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * ===========================================================================
 * types
 */
enum opt {
	OPT_CLOBBER   = 1,
	OPT_CMDLINE   = 1 << 1,
	OPT_GLOB      = 1 << 2,
	OPT_IGNOREEOF = 1 << 3,
	OPT_PIPEFAIL  = 1 << 4,
	OPT_STDIN     = 1 << 5,
	OPT_VERBOSE   = 1 << 6
};

struct command {
	char **argv;
	size_t argc;
	int *dynallocinfo;
};

struct cmdinfo {
	int canexpandpath;
	int redirfds[3];
};

struct builtin {
	int (*fn)(const struct command *, const struct cmdinfo *);
	const char *name;
};


/*
 * ===========================================================================
 * function declarations
 */

/* builtins */
static int builtin_cd(const struct command *cmd,
		const struct cmdinfo *info);
static int builtin_exit(const struct command *cmd,
		const struct cmdinfo *info);
static int builtin_set(const struct command *cmd,
		const struct cmdinfo *info);
static int builtin_type(const struct command *cmd,
		const struct cmdinfo *info);
static int end_builtin_redir(const struct cmdinfo *info,
		const int savefds[2]);
static int start_builtin_redir(const struct cmdinfo *info,
		int savefds[2]);
static int try_exec_builtin(const struct command *cmd,
		const struct cmdinfo *info);

/* command execution */
static int exec(char *s);
static pid_t pipechain(char *s, pid_t *pgid, int *rpipe, int *wpipe,
		int *closethis);
static int pipeline(char *s);
static void takecmd(char *s);
static void update_laststatus(int status);

/* command parsing */
static int parsecmd(char *s, struct command *cmd, struct cmdinfo *info);
static int parseredir(struct command *cmd, struct cmdinfo *info);

/* memory allocation for commands */
static int alloccmd(size_t slots, struct command *cmd);
static int realloccmd(size_t slots, struct command *cmd);
static void freecmd(const struct command *cmd);

/* pathname expansion */
static int expand_path(const struct command *cmd, struct command *newcmd);
static char *expand_lone_tilde(const char *s);
static char *expand_tilde(const char *s, int *dynalloc);

/* option parsing */
static void optcmdlineset(int initialized, const char *arg0, char *arg1,
		char **cmdline);
static void optlist(int plus);
static int optparse(int initialized, int argc, char *argv[], char **cmdline);
static void opttoggle(int enable, int opt);

/* functions used by builtins */
static int executable(int dirfd, const char *name);
static int which(const char *pathenv, const char *name);

/* utility functions */
static char *delimit(char *str, char delim);
static char *optstrsignal(int sig);
static void popchar(char *ptr);
static void report(pid_t pid);
static int xstrtoint(int *res, const char *s, int base);

/* error checking */
static int weclose(int fd);
static int weglob(const char *pattern, int flags,
		int (*errfunc)(const char *epath, int eerrno), glob_t *pglob);
static void *wemalloc(size_t size);
static void *wemallocarray(size_t nmemb, size_t size);
static void *werealloc(void *ptr, size_t size);
static void *wereallocarray(void *ptr, size_t nmemb, size_t size);
static char *westrdup(const char *s);
static char *westrndup(const char *s, size_t n);
static int wexasprintf(char **strp, const char *fmt, ...);
static int wexstrtoint(int *res, const char *s, int base);

/* error logging */
static void logerr(const char *fmt, ...);
static void vlogerr(const char *fmt, va_list ap);

/*
 * ===========================================================================
 * global variables
 */
static const struct builtin builtins[] = {
	{builtin_cd, "cd"},
	{builtin_exit, "exit"},
	{builtin_set, "set"},
	{builtin_type, "type"},
	{NULL, NULL}
};

static char defaultprompt[] = "$ ";

static const char *argv0 = NULL;
static char *prompt = defaultprompt;
static int opts = OPT_GLOB | OPT_STDIN;

static int laststatus = 0;
static int lastfail = 0; /* used for pipefail */

static int term = -1;
static pid_t shell_pgid = -1;

/*
 * ===========================================================================
 * builtins
 */
static int
builtin_cd(const struct command *cmd, const struct cmdinfo *info)
{
	const char *oldargv0 = argv0;
	int savefds[2];
	int ret = 0;
	size_t arg = 1;

	if (start_builtin_redir(info, savefds) < 0)
		return MISC_FAILURE_STATUS;
	argv0 = cmd->argv[0];

	if (cmd->argc > 1 && !strcmp(cmd->argv[1], "--"))
		++arg;

	if (cmd->argc > arg) {
		if (cmd->argc > arg + 1) {
			logerr("too many operands specified");
			ret = 1;
		} else if (chdir(cmd->argv[arg]) < 0) {
			logerr("chdir:");
			ret = 1;
		}
	} else {
		const char *home = getenv("HOME");
		if (home && chdir(home) < 0) {
			logerr("chdir:");
			ret = 1;
		}
	}

	argv0 = oldargv0;
	if (end_builtin_redir(info, savefds) < 0)
		return MISC_FAILURE_STATUS;
	return ret;
}

static int
builtin_exit(const struct command *cmd, const struct cmdinfo *info)
{
	const char *oldargv0 = argv0;
	int savefds[2];
	int ret = 0;
	size_t arg = 1;

	if (start_builtin_redir(info, savefds) < 0)
		return MISC_FAILURE_STATUS;
	argv0 = cmd->argv[0];

	if (cmd->argc > 1 && !strcmp(cmd->argv[1], "--"))
		++arg;

	if (cmd->argc > arg) {
		int status;
		if (cmd->argc > arg + 1) {
			logerr("too many operands specified");
			ret = 1;
		} else if (wexstrtoint(&status, cmd->argv[arg], 10) < 0
				|| status > 255) {
			ret = 1;
		} else {
			exit(status);
		}
	} else {
		exit(0);
	}

	argv0 = oldargv0;
	if (end_builtin_redir(info, savefds) < 0)
		return MISC_FAILURE_STATUS;
	return ret;
}

static int
builtin_set(const struct command *cmd, const struct cmdinfo *info)
{
	int savefds[2];
	int ret = 0;

	if (start_builtin_redir(info, savefds) < 0)
		return MISC_FAILURE_STATUS;

	if (cmd->argc > 1 && strcmp(cmd->argv[1], "--") != 0)
		if (optparse(1, (int)(cmd->argc), cmd->argv, NULL) < 0)
			ret = 1;

	if (end_builtin_redir(info, savefds) < 0)
		return MISC_FAILURE_STATUS;
	return ret;
}

static int
builtin_type(const struct command *cmd, const struct cmdinfo *info)
{
	const char *oldargv0 = argv0;
	const char *pathenv;
	size_t i, j;
	int savefds[2];
	int found, ret = 0;

	if (start_builtin_redir(info, savefds) < 0)
		return MISC_FAILURE_STATUS;
	argv0 = cmd->argv[0];

	pathenv = getenv("PATH");
	if (!pathenv)
		logerr("$PATH is not set");

	for (i = 1; i < cmd->argc; ++i) {
		found = 0;
		if (!strcmp(cmd->argv[i], "--"))
			continue;
		for (j = 0; builtins[j].name; ++j) {
			if (!strcmp(cmd->argv[i], builtins[j].name)) {
				printf("%s: a builtin\n", cmd->argv[i]);
				found = 1;
				break;
			}
		}
		if (!found && !which(pathenv, cmd->argv[i])) {
			logerr("no such command '%s'", cmd->argv[i]);
			ret = 1;
		}
	}

	argv0 = oldargv0;
	if (end_builtin_redir(info, savefds) < 0)
		return MISC_FAILURE_STATUS;
	return ret;
}

static int
end_builtin_redir(const struct cmdinfo *info, const int savefds[2])
{
	if (info->redirfds[0] >= 0 && info->redirfds[1] >= 0) {
		if (dup2(savefds[0], info->redirfds[1]) < 0) {
			logerr("dup2:");
			return -1;
		}
		if (weclose(savefds[0]) < 0)
			return -1;
	}
	if (info->redirfds[2] >= 0) {
		if (dup2(savefds[1], info->redirfds[2]) < 0) {
			logerr("dup2:");
			return -1;
		}
	}
	return 0;
}

static int
start_builtin_redir(const struct cmdinfo *info, int savefds[2])
{
	if (info->redirfds[0] >= 0 && info->redirfds[1] >= 0) {
		savefds[0] = dup(info->redirfds[1]);
		if (savefds[0] < 0) {
			logerr("dup:");
			return -1;
		}
		if (dup2(info->redirfds[0], info->redirfds[1]) < 0) {
			logerr("dup2:");
			return -1;
		}
	}
	if (info->redirfds[2] >= 0) {
		errno = 0;
		savefds[1] = dup(info->redirfds[2]);
		if (savefds[1] < 0 && errno != EBADF) {
			logerr("dup:");
			return -1;
		} else if (savefds[1] >= 0) {
			close(info->redirfds[2]);
		}
	}
	return 0;
}

static int
try_exec_builtin(const struct command *cmd, const struct cmdinfo *info)
{
	if (!cmd->argv[0]) {
		return 127;
	} else {
		size_t i;
		int ret;
		for (i = 0; builtins[i].name; ++i) {
			if (!strcmp(cmd->argv[0], builtins[i].name)) {
				ret = builtins[i].fn(cmd, info);
				laststatus = ret;
				if (ret > 0)
					lastfail = ret;
				return ret;
			}
		}
		return 127;
	}
}

/*
 * ===========================================================================
 * command execution functions
 */
static int
exec(char *s)
{
	char *pipechr = strchr(s, '|');
	if (pipechr && pipechr != s && *(pipechr - 1) != '>') {
		return pipeline(s);
	} else {
		struct command origcmd;
		struct command expcmd;
		struct command *cmd = &origcmd;
		struct cmdinfo info;
		int didglob = 0;
		int ret = 0;

		if (parsecmd(s, &origcmd, &info) < 0)
			return -1;

		if (info.canexpandpath && (opts & OPT_GLOB)) {
			if (expand_path(&origcmd, &expcmd) < 0) {
				freecmd(&origcmd);
				return -1;
			}
			cmd = &expcmd;
			didglob = 1;
		}
		if (parseredir(cmd, &info) < 0) {
			if (didglob)
				freecmd(&expcmd);
			freecmd(&origcmd);
			return -1;
		}

		if (try_exec_builtin(cmd, &info) == 127) {
			pid_t chpid = fork();
			switch (chpid) {
			case -1:
				logerr("fork:");
				ret = -1;
				break;
			case 0:
				/*
				 * if the shell is interactive, go into
				 * a new process group and put it into
				 * the foreground
				 */
				if (term >= 0) {
					if (setpgid(0, 0) < 0) {
						logerr("setpgid:");
						_exit(MISC_FAILURE_STATUS);
					}
					if (tcsetpgrp(term, getpgrp()) < 0) {
						logerr("tcsetpgrp:");
						_exit(MISC_FAILURE_STATUS);
					}
				}

				/* redirection */
				if (info.redirfds[2] >= 0)
					close(info.redirfds[2]);

				if (info.redirfds[0] >= 0
						&& info.redirfds[1] >= 0) {
					if (dup2(info.redirfds[0],
						info.redirfds[1]) < 0) {
						logerr("dup2:");
						return -1;
					}
				}

				/* execute the command */
				if (execvp(cmd->argv[0], cmd->argv) < 0) {
					logerr("execvp %s:", cmd->argv[0]);
					_exit((errno == ENOENT) ? 127 :
						((errno == ENOEXEC) ? 126 :
						 MISC_FAILURE_STATUS));
				}
				/* unreachable */
				break;
			default:
				report(chpid);

				/* put ourselves back into the foreground */
				if (term >= 0)
					if (tcsetpgrp(term, shell_pgid) < 0)
						logerr("tcsetpgrp:");

				update_laststatus(laststatus);
			}
		}

		if (info.redirfds[0] > STDERR_FILENO)
			if (weclose(info.redirfds[0]) < 0)
				return -1;
		if (didglob)
			freecmd(&expcmd);
		freecmd(&origcmd);
		return ret;
	}
}

static pid_t
pipechain(char *s, pid_t *pgid, int *rpipe, int *wpipe, int *closethis)
{
	struct command origcmd;
	struct command expcmd;
	struct command *cmd = &origcmd;
	struct cmdinfo info;
	int didglob = 0;
	int forkfailed = 0;

	pid_t chpid = 0;
	*closethis = -1;

	if (parsecmd(s, &origcmd, &info) < 0)
		return -1;
	if (info.canexpandpath && (opts & OPT_GLOB)) {
		if (expand_path(&origcmd, &expcmd) < 0) {
			freecmd(&origcmd);
			return -1;
		}
		cmd = &expcmd;
		didglob = 1;
	}
	if (parseredir(cmd, &info) < 0) {
		if (didglob)
			freecmd(&expcmd);
		freecmd(&origcmd);
		return -1;
	}

	if (try_exec_builtin(cmd, &info) == 127) {
		chpid = fork();
		switch (chpid) {
		case -1:
			logerr("fork:");
			forkfailed = 1;
			break;
		case 0:
			if (term >= 0) {
				if (*pgid < 0 || (kill(*pgid, 0 < 0)
							&& errno == ESRCH)) {
					/* set new PGID for the pipeline */
					if (setpgid(0, 0) < 0) {
						logerr("setpgid:");
						_exit(MISC_FAILURE_STATUS);
					}
					if (tcsetpgrp(term, getpgrp()) < 0) {
						logerr("tcsetpgrp:");
						_exit(MISC_FAILURE_STATUS);
					}
				} else if (setpgid(0, *pgid) < 0) {
					/*
					 * PGID of the pipeline set already,
					 * join that group
					 */
					logerr("setpgid:");
					_exit(MISC_FAILURE_STATUS);
				}
			}

			/* pipe redirection */
			if (rpipe) {
				/* there's a pipe from the previous process */
				if (dup2(rpipe[0], STDIN_FILENO) < 0) {
					logerr("dup2:");
					return -1;
				}
				if (weclose(rpipe[0]) < 0
						|| weclose(rpipe[1]) < 0)
					return -1;
			}
			if (wpipe) {
				/* there's a pipe to the next process */
				if (dup2(wpipe[1], STDOUT_FILENO) < 0) {
					logerr("dup2:");
					return -1;
				}
				if (weclose(wpipe[0]) < 0
						|| weclose(wpipe[1]))
					return -1;
			}

			/* redirection */
			if (info.redirfds[2] >= 0)
				close(info.redirfds[2]);
			if (info.redirfds[0] >= 0
					&& info.redirfds[1] >= 0) {
				if (dup2(info.redirfds[0],
						info.redirfds[1]) < 0) {
					logerr("dup2:");
					return -1;
				}
			}

			/* execute the command */
			if (execvp(cmd->argv[0], cmd->argv) < 0) {
				logerr("execvp %s:", cmd->argv[0]);
				_exit((errno == ENOENT) ? 127 :
					((errno == ENOEXEC) ? 126 :
					 MISC_FAILURE_STATUS));
			}
			/* unreachable */
			break;
		}
	}

	if (rpipe)
		if (weclose(rpipe[0]) < 0 || weclose(rpipe[1]) < 0)
			return -1;
	if (info.redirfds[0] > STDERR_FILENO)
		*closethis = info.redirfds[0];
	if (didglob)
		freecmd(&expcmd);
	freecmd(&origcmd);
	if ((*pgid = tcgetpgrp(term)) < 0) {
		logerr("tcgetpgrp:");
		return -1;
	}
	if (forkfailed)
		return -1;
	return chpid;
}

static int
pipeline(char *s)
{
	/*
	 * handles pipelines, for instance:
	 * ps aux | grep proc | grep -v grep | awk '{print $NF}'
	 *
	 * the chain has two processes active at a time, and
	 * waits for the process in the previous slot from the
	 * current one to end before moving on. this is to avoid
	 * deadlocks when the pipe is full since the 2nd process will
	 * always be there to recieve the first one's input.
	 */
	char **cmds = wemallocarray(sizeof(char *), ARGV_ALLOC_SIZE);
	char *ptr;
	char *oldptr = s;
	size_t arrsize = ARGV_ALLOC_SIZE;
	size_t i = 0, j = 0;

	/*
	 * two pipes & two pids:
	 * one from the previous in the chain, one to the next in the chain
	 */
	int lpipe[2], rpipe[2];
	pid_t lpid, rpid;
	int ldup = -1, rdup = -1;

	/* PGID of the pipeline */
	pid_t pgid = -1;

	if (!cmds)
		return -1;
	ptr = delimit(s, '|');
	while (ptr) {
		if (i >= arrsize) {
			char **oldcmds = cmds;
			arrsize += ARGV_ALLOC_SIZE;
			if (!(cmds = wereallocarray(cmds, sizeof(char *),
							arrsize))) {
				free(oldcmds);
				return -1;
			}
		}
		cmds[i++] = oldptr;
		oldptr = ptr;
		ptr = delimit(ptr, '|');
	}
	if (i + 1 >= arrsize) {
		char **oldcmds = cmds;
		arrsize += 2;
		if (!(cmds = wereallocarray(cmds, sizeof(char *), arrsize))) {
			free(oldcmds);
			return -1;
		}
	}
	cmds[i++] = oldptr;
	cmds[i] = NULL;

	/* create the first output pipe */
	pipe(rpipe);

	/* first child takes input from wherever the shell gets it */
	if ((lpid = pipechain(cmds[j++], &pgid, NULL, rpipe, &ldup)) < 0)
		return -1;
	/* output pipe becomes input for the next process */
	lpipe[0] = rpipe[0];
	lpipe[1] = rpipe[1];

	/* chain all but the first and last children */
	while (j < i - 2) {
		pipe(rpipe); /* make the next output pipe */
		if ((rpid = pipechain(cmds[j++], &pgid, lpipe, rpipe,
						&rdup)) < 0)
			return -1;
		report(lpid); /* wait for previous process in chain */
		if (ldup > 0)
			weclose(ldup);
		lpid = rpid; /* make this process the "previous in chain" */
		ldup = rdup;

		lpipe[0] = rpipe[0]; /* output pipe becomes input pipe */
		lpipe[1] = rpipe[1];
	}

	/*
	 * fork the last one, its output goes to wherever the shells stdout
	 * is
	 */
	if ((rpid = pipechain(cmds[j++], &pgid, lpipe, NULL, &rdup)) < 0)
		return -1;
	report(lpid); /* wait for previous process in chain */
	report(rpid); /* wait for current (last) process in chain */

	/* put ourselves back into the foreground */
	if (term >= 0) {
		if (tcsetpgrp(term, shell_pgid) < 0) {
			logerr("tcsetpgrp:");
			return -1;
		}
	}

	if (opts & OPT_PIPEFAIL) {
		update_laststatus(lastfail);
		lastfail = 0;
	} else {
		update_laststatus(laststatus);
	}

	if (ldup > 0)
		weclose(ldup);
	if (rdup > 0)
		weclose(rdup);
	return 0;
}

static void
takecmd(char *s)
{
	if (opts & OPT_VERBOSE) {
		fputs(s, stderr);
		/* print a trailing newline if we didn't print one already */
		if (s[0] && s[strlen(s) - 1] != '\n')
			putc('\n', stderr);
	}

	/* ignore empty commands (e.g user just pressing enter) */
	if (s[0] && s[0] != '\n') {
		char *ptr = delimit(s, ';');

		if (!ptr) {
			if (exec(s) < 0) {
				laststatus = lastfail = MISC_FAILURE_STATUS;
				update_laststatus(laststatus);
			}
		} else {
			char *oldptr = s;
			while (ptr) {
				if (exec(oldptr) < 0) {
					laststatus = lastfail =
						MISC_FAILURE_STATUS;
					update_laststatus(laststatus);
				}
				oldptr = ptr;
				ptr = delimit(ptr, ';');
			}
			if (exec(oldptr) < 0) {
				laststatus = lastfail = MISC_FAILURE_STATUS;
				update_laststatus(laststatus);
			}
		}
	}
}

static void
update_laststatus(int status)
{
	if (prompt != defaultprompt)
		free(prompt);
	if (status > 0) {
		if (wexasprintf(&prompt, "%d %s", status, defaultprompt) < 0) {
			prompt = defaultprompt;
		}
	} else {
		prompt = defaultprompt;
	}
}

/*
 * ===========================================================================
 * command parsing functions
 */
static int
parsecmd(char *s, struct command *cmd, struct cmdinfo *info)
{
	int canexpandpath = !!(strpbrk(s, "?*["));
	s[strcspn(s, "\n")] = '\0';
	if (!strchr(s, ' ')) {
		/*
		 * the string has no separators, put everything into
		 * argv[0]
		 */
		if (alloccmd(2, cmd) < 0)
			return -1;
		cmd->argv[0] = s;
		cmd->dynallocinfo[0] = 0;
		cmd->argv[1] = NULL;
		cmd->dynallocinfo[1] = -1;
		cmd->argc = 1;
		info->canexpandpath = canexpandpath;
		return 0;
	} else {
		char *ptr = s;
		char *end = s + strlen(s);
		char *prev_start = s;

		/*
		 * this variable is set to the character representing
		 * the delimiter between tokens, e.g
		 *
		 * with delim = ' ', "one 'two three'" will be split as
		 * {"one", "'two", "three'"}
		 * with delim = '\'', that same string will be split as
		 * {"one", "two three"}
		 */
		char delim = ' ';
		int searching = 0;

		size_t currsize = ARGV_ALLOC_SIZE;
		size_t i = 0;

		char *tilde_exp;
		int tilde_dynalloc;

		if (alloccmd(ARGV_ALLOC_SIZE, cmd) < 0)
			return -1;

		/*
		 * loop until we can't find any more separators or are
		 * at the end of the string
		 */
		while (ptr && ptr < end) {
			if (*ptr == '\\') {
				/*
				 * remove the \ character and pretend that
				 * the next char doesn't exist by going
				 * over it
				 */
				popchar(ptr++);
			} else if (*ptr == delim) {
				if (delim != ' ') {
					/* stop searching for closing quote */
					searching = 0;
					delim = ' ';
				}
				*ptr = '\0';
				if (i >= currsize) {
					currsize += ARGV_ALLOC_SIZE;
					if (realloccmd(currsize, cmd) < 0)
						return -1;
				}
				tilde_exp = expand_tilde(prev_start,
						&tilde_dynalloc);
				if (tilde_exp) {
					cmd->argv[i] = tilde_exp;
					cmd->dynallocinfo[i] = tilde_dynalloc;
				} else {
					cmd->argv[i] = prev_start;
					cmd->dynallocinfo[i] = 0;
				}
				++i;
				prev_start = ++ptr;
			} else if (delim == ' '
					&& (*ptr == '\'' || *ptr == '"')) {
				delim = *ptr;
				popchar(ptr);
				searching = 1;
			} else {
				++ptr;
			}
		}
		if (searching) {
			fputs("syntax error: unclosed quotation\n", stderr);
			freecmd(cmd);
			return -1;
		} else {
			if (*prev_start) {
				/*
				 * i can only be equal to currsize and we only need
				 * one more slot
				 */
				if (i >= currsize)
					if (realloccmd(++currsize, cmd) < 0)
						return -1;
				tilde_exp = expand_tilde(prev_start,
						&tilde_dynalloc);
				if (tilde_exp) {
					cmd->argv[i] = tilde_exp;
					cmd->dynallocinfo[i] = tilde_dynalloc;
				} else {
					cmd->argv[i] = prev_start;
					cmd->dynallocinfo[i] = 0;
				}
				++i;
			}
			if (i >= currsize) {
				/* same as earlier */
				if (realloccmd(++currsize, cmd) < 0)
					return -1;
			}
			cmd->argv[i] = NULL;
			cmd->dynallocinfo[i] = -1;
			cmd->argc = i;
			info->canexpandpath = canexpandpath;
			return 0;
		}
	}
}

static int
parseredir(struct command *cmd, struct cmdinfo *info)
{
	size_t argend = 0, i;
	char *ptr = NULL;
	int flags = 0, target_fd = 0;
	char *redir_target;

	info->redirfds[0] = -1;
	info->redirfds[1] = -1;
	info->redirfds[2] = -1;
	for (i = 1; i < cmd->argc; ++i) {
		ptr = strpbrk(cmd->argv[i], "<>");
		if (ptr) {
			int doclose = 0;
			argend = i;
			switch (*ptr) {
			case '<':
				flags = O_RDONLY;
				target_fd = STDIN_FILENO;
				break;
			case '>':
				if (*(ptr + 1) == '|') {
					ptr++;
					flags = O_WRONLY | O_CREAT
						| O_TRUNC;
				} else if (opts & OPT_CLOBBER) {
					flags = O_WRONLY | O_CREAT
						| O_TRUNC;
				} else {
					flags = O_WRONLY | O_CREAT
						| O_EXCL;
				}
				target_fd = STDOUT_FILENO;
			}
			if (*(ptr + 1) == '\0') {
				if (i + 1 >= cmd->argc) {
					fputs("syntax error: missing "
						"redirection target\n",
						stderr);
					return -1;
				}
				redir_target = cmd->argv[i + 1];
			} else {
				redir_target = ptr + 1;
			}
			if (*redir_target == '&') {
				if (*(redir_target + 1) == '\0') {
					fputs("syntax error: missing "
						"redirection target\n",
						stderr);
					return -1;
				} else if (!strcmp(redir_target, "&-")) {
					doclose = 1;
				} else if (wexstrtoint(&info->redirfds[0],
							redir_target + 1,
							10) < 0) {
					/*
					 * wexstrtoint already prints a
					 * message on failure
					 */
					return -1;
				}
			} else {
				if (flags & O_CREAT) {
					info->redirfds[0] = open(redir_target,
							flags,
							S_IRUSR | S_IWUSR |
							S_IRGRP | S_IWGRP |
							S_IROTH | S_IWOTH);
				} else {
					info->redirfds[0] = open(redir_target,
							flags);
				}
				if (info->redirfds[0] < 0) {
					logerr("open %s:", redir_target);
					return -1;
				}
			}

			if (doclose)
				info->redirfds[2] = target_fd;
			else
				info->redirfds[1] = target_fd;

			/*
			 * if this is not the beginning of the argument
			 * string, e.g:
			 *
			 * will be false for 'cmd > file'
			 * will be true for 'cmd 2>file'
			 */
			if (ptr != cmd->argv[i]) {
				/*
				 * if xstrtoint fails target_fd will be
				 * left unchanged
				 */
				char c = *ptr;
				*ptr = '\0';
				xstrtoint(&info->redirfds[1 + doclose],
						cmd->argv[i], 10);
				*ptr = c;
			}
		}
	}

	if (argend > 0) {
		cmd->argv[argend] = NULL;
		cmd->argc = argend;
	}
	return 0;
}

/*
 * ===========================================================================
 * memory allocation functions for commands
 */
static int
alloccmd(size_t slots, struct command *cmd)
{
	if (!(cmd->argv = wemallocarray(sizeof(char *), slots)))
		return -1;
	if (!(cmd->dynallocinfo = wemallocarray(sizeof(int), slots)))
		return -1;
	return 0;
}

static int
realloccmd(size_t slots, struct command *cmd)
{
	char **oldargv = cmd->argv;
	int *olddynallocinfo = cmd->dynallocinfo;
	cmd->argv = wereallocarray(cmd->argv, sizeof(char *), slots);
	if (!cmd->argv) {
		free(oldargv);
		return -1;
	}
	cmd->dynallocinfo = wereallocarray(
			cmd->dynallocinfo, sizeof(int), slots);
	if (!cmd->dynallocinfo) {
		free(olddynallocinfo);
		return -1;
	}
	return 0;
}

static void
freecmd(const struct command *cmd)
{
	size_t i;
	for (i = 0; i < cmd->argc; ++i)
		if (cmd->dynallocinfo[i])
			free(cmd->argv[i]);
	free(cmd->argv);
	free(cmd->dynallocinfo);
}

/*
 * ===========================================================================
 * pathname expansion functions
 */
static int
expand_path(const struct command *cmd, struct command *newcmd)
{
	size_t readarg;
	size_t writearg = 0;

	/*
	 * since it's guaranteed that there's something to be expanded,
	 * let's avoid unneeded reallocs and preallocate some extra space
	 */
	size_t currsize = cmd->argc * 2;

	/* temporary variables */
	glob_t globbuf;
	size_t i;
	char *sdup;

	if (alloccmd(currsize, newcmd) < 0)
		return -1;
	for (readarg = 0; readarg < cmd->argc; ++readarg) {
		if (!strpbrk(cmd->argv[readarg], "?*[")) {
			/* copy the arg if we don't need to do anything */
			if (writearg >= currsize) {
				currsize += ARGV_ALLOC_SIZE;
				if (realloccmd(currsize, newcmd) < 0)
					return -1;
			}
			newcmd->argv[writearg] = cmd->argv[readarg];

			/*
			 * always mark this as "dont need free" to avoid
			 * double free when the old cmd gets freed too
			 */
			newcmd->dynallocinfo[writearg++] = 0;
		} else {
			int g = weglob(cmd->argv[readarg], 0, NULL, &globbuf);
			if (g != 0 && g != GLOB_NOMATCH) {
				newcmd->argc = writearg;
				freecmd(newcmd);
				return -1;
			}
			for (i = 0; i < globbuf.gl_pathc; i++) {
				if (writearg >= currsize) {
					currsize += ARGV_ALLOC_SIZE;
					if (realloccmd(currsize, newcmd)
							< 0)
						return -1;
				}
				if (!(sdup = westrdup(globbuf.gl_pathv[i]))) {
					newcmd->argc = writearg;
					freecmd(newcmd);
					return -1;
				}
				newcmd->argv[writearg] = sdup;
				newcmd->dynallocinfo[writearg] = 1;
				++writearg;
			}
			globfree(&globbuf);
		}
	}
	/*
	 * writearg can only be equal to currsize and we only need
	 * one more slot
	 */
	if (writearg >= currsize)
		if (realloccmd(++currsize, newcmd) < 0)
			return -1;
	newcmd->argv[writearg] = NULL;
	newcmd->dynallocinfo[writearg] = -1;
	newcmd->argc = writearg;
	return 0;
}

static char *
expand_lone_tilde(const char *s)
{
	if (s[1] == '\0') {
		return getenv("HOME");
	} else {
		struct passwd *pw = getpwnam(s + 1);
		if (!pw)
			return NULL;
		return pw->pw_dir;
	}
}

static char *
expand_tilde(const char *s, int *dynalloc)
{
	const char *tail;
	if (s[0] != '~')
		return NULL;
	if (!(tail = strpbrk(s, "/ "))) {
		*dynalloc = 0;
		return expand_lone_tilde(s);
	} else {
		char *sdup = westrndup(s, (size_t)(tail - s));
		char *exp;
		char *result;
		size_t explen, taillen = strlen(tail);
		if (!sdup)
			return NULL;
		exp = expand_lone_tilde(sdup);
		free(sdup);
		if (!exp)
			return NULL;
		explen = strlen(exp);
		if (!(result = wemalloc(explen + taillen + 1)))
			return NULL;
		memcpy(result, exp, explen);
		memcpy(result + explen, tail, taillen + 1);
		*dynalloc = 1;
		return result;
	}
}

/*
 * ===========================================================================
 * option parsing functions
 */
static void
optcmdlineset(int initialized, const char *arg0, char *arg1, char **cmdline)
{
	if (initialized) {
		fprintf(stderr, "%s: the cmdline option cannot be changed "
				"after the shell has been initialized\n",
				arg0);
	} else if (arg1) {
		opts &= ~OPT_STDIN;
		opts |= OPT_CMDLINE;
		*cmdline = arg1;
	} else {
		fprintf(stderr, "%s: the cmdline option was specified but no "
				"command was given\n", arg0);
	}
}

static void
optlist(int plus)
{
	if (plus) {
		printf("set %co clobber\n",
				(opts & OPT_STDIN) ? '-' : '+');
		printf("set %co cmdline\n",
				(opts & OPT_CMDLINE) ? '-' : '+');
		printf("set %co glob\n",
				(opts & OPT_GLOB) ? '-' : '+');
		printf("set %co ignoreeof\n",
				(opts & OPT_IGNOREEOF) ? '-' : '+');
		printf("set %co pipefail\n",
				(opts & OPT_PIPEFAIL) ? '-' : '+');
		printf("set %co stdin\n",
				(opts & OPT_STDIN) ? '-' : '+');
		printf("set %co verbose\n",
				(opts & OPT_VERBOSE) ? '-' : '+');
	} else {
		printf("clobber    %s\n",
				(opts & OPT_CLOBBER) ? "on" : "off");
		printf("cmdline    %s\n",
				(opts & OPT_CMDLINE) ? "on" : "off");
		printf("glob       %s\n",
				(opts & OPT_GLOB) ? "on" : "off");
		printf("ignoreeof  %s\n",
				(opts & OPT_IGNOREEOF) ? "on" : "off");
		printf("pipefail   %s\n",
				(opts & OPT_PIPEFAIL) ? "on" : "off");
		printf("stdin      %s\n",
				(opts & OPT_STDIN) ? "on" : "off");
		printf("verbose    %s\n",
				(opts & OPT_VERBOSE) ? "on" : "off");
	}
}

static int
optparse(int initialized, int argc, char *argv[], char **cmdline)
{
	/* used in main() and the set builtin. */
	const char *curr;
	int plus;
	int i; /* since argc is an int let's make this an int too */

	for (i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			plus = 0;
		} else if (argv[i][0] == '+') {
			plus = 1;
		} else {
			fprintf(stderr, "%s: unrecognized option '%s'\n",
					argv[0], argv[i]);
			continue;
		}
		curr = &argv[i][1];
		while (curr) {
			switch (*curr) {
			case 'o':
				if (i + 1 < argc && !(*(curr + 1))) {
					const char *opt = argv[++i];
					int enable = !plus;
					if (!strncmp(opt, "no", 2)) {
						enable = !enable;
						opt += 2;
					}

					if (!strcmp(opt, "clobber")) {
						opttoggle(enable,
							OPT_CLOBBER);
					} else if (!strcmp(opt, "cmdline")
							&& enable) {
						optcmdlineset(initialized,
								argv[0],
								argv[++i],
								cmdline);
					} else if (!strcmp(opt, "glob")) {
						opttoggle(enable, OPT_GLOB);
					} else if (!strcmp(opt, "ignoreeof"
								)) {
						opttoggle(enable,
							OPT_IGNOREEOF);
					} else if (!strcmp(opt, "pipefail"
								)) {
						opttoggle(enable,
							OPT_PIPEFAIL);
					} else if (!strcmp(opt, "stdin")) {
						opttoggle(enable, OPT_STDIN);
					} else if (!strcmp(opt, "verbose")) {
						opttoggle(enable,
							OPT_VERBOSE);
					} else {
						fprintf(stderr, "%s: "
							"unrecognized "
							"option '%s'\n",
							argv[0], opt);
					}
				} else {
					if (initialized) {
						optlist(plus);
					} else {
						fprintf(stderr, "%s: "
							"missing argument "
							"for -o option\n",
							argv[0]);
						return -1;
					}
				}
				break;
			case 'c':
				if (!plus) {
					optcmdlineset(initialized, argv[0],
							argv[++i], cmdline);
				}
				break;
			case 'C':
				opttoggle(plus, OPT_CLOBBER);
				break;
			case 'f':
				opttoggle(plus, OPT_GLOB);
				break;
			case 's':
				if (initialized) {
					fprintf(stderr, "%s: the stdin option"
							" cannot be changed "
							"after the shell has "
							"been initialized\n",
							argv[0]);
				} else if (!plus && (opts & OPT_CMDLINE)) {
					fprintf(stderr, "%s: the stdin option"
							"cannot be used with "
							"the cmdline option"
							"\n", argv[0]);
				} else {
					opttoggle(!plus, OPT_STDIN);
				}
				break;
			case 'v':
				opttoggle(!plus, OPT_VERBOSE);
				break;
			default:
				fprintf(stderr, "usage: %s [+-Cfsv] "
						"[+-c cmdline] "
						"[+-o option]\n", argv[0]);
				return -1;
			}
			if (*(curr + 1))
				++curr;
			else
				curr = NULL;
		}
	}
	return 0;
}

static void
opttoggle(int enable, int opt)
{
	if (enable)
		opts |= opt;
	else
		opts &= ~opt;
}

/*
 * ===========================================================================
 * functions used by builtins
 */
static int
executable(int dirfd, const char *name)
{
	struct stat st;

	if (fstatat(dirfd, name, &st, 0) < 0 || !S_ISREG(st.st_mode))
		return 0;
	return faccessat(dirfd, name, X_OK, AT_EACCESS) == 0;
}

static int
which(const char *pathenv, const char *name)
{
	char *path, *searchdir;
	size_t i, l;
	int dirfd, found = 0;
	if (strchr(name, '/')) {
		if ((found = executable(AT_FDCWD, name))) {
			printf("%s: an external command at %s\n", name, name);
		}
		return found;
	}

	if (!(path = searchdir = westrdup(pathenv)))
		return -1;
	l = strlen(path);
	for (i = 0; i <= l; ++i) {
		if (path[i] && path[i] != ':')
			continue;
		path[i] = '\0';
		if ((dirfd = open(searchdir, O_RDONLY)) >= 0) {
			if ((found = executable(dirfd, name))) {
				if (i && path[i - 1] != '/')
					printf("%s: an external command at "
							"%s/%s\n", name,
							searchdir, name);
				else
					printf("%s: an external command at "
							"%s%s\n", name,
							searchdir, name);
			}
			close(dirfd);
			if (found)
				break;
		}
		searchdir = path + i + 1;
	}
	free(path);
	return found;
}

/*
 * ===========================================================================
 * utility functions
 */
static char *
delimit(char *str, char delim)
{
	/*
	 * split str at delim with an arbitrary amount of spaces
	 * before and after delim, e.g:
	 *
	 * char s[] = "echo abcd | rev";
	 * char *tail = delimit(s, '|');
	 * printf("pipe '%s' into '%s'\n", s, tail);
	 *
	 * will output "pipe 'echo abcd' into 'rev'".
	 *
	 * if delim is not found in str, NULL is returned and str
	 * is left unchanged.
	 */
	char *ptr, *space, *tail;
	if (!(ptr = strchr(str, delim)))
		return NULL;
	*ptr = '\0';
	space = ptr - 1;

	/* skip over any leading spaces in the head */
	while (*space == ' ')
		--space;

	*(space + 1) = '\0';
	tail = ptr + 1;

	/* skip over any trailing spaces in the tail */
	while (*tail == ' ')
		++tail;

	return tail;
}

static char *
optstrsignal(int sig)
{
	/*
	 * strsignal(3) that returns NULL for signals that were configured
	 * to not be reported
	 */
#if !defined(REPORT_SIGINT)
	if (sig == SIGINT)
		return NULL;
#endif /* !REPORT_SIGINT */
#if !defined(REPORT_SIGPIPE)
	if (sig == SIGPIPE)
		return NULL;
#endif /* !REPORT_SIGPIPE */
	return strsignal(sig);
}

static void
popchar(char *ptr)
{
	size_t l = strlen(ptr) - 1;
	memmove(ptr, ptr + 1, l);
	*(ptr + l) = '\0';
}

static void
report(pid_t pid)
{
	if (pid > 0) {
		int wstatus, exitstatus = 0;
		waitpid(pid, &wstatus, 0);

		if (WIFSIGNALED(wstatus)) {
			char *sigstr;
			exitstatus = WTERMSIG(wstatus) + SIGNAL_EXITSTATUS;
			if ((sigstr = optstrsignal(WTERMSIG(wstatus))))
				fprintf(stderr, "%s\n", sigstr);
		} else if (WIFEXITED(wstatus)) {
			exitstatus = WEXITSTATUS(wstatus);
		}

		laststatus = exitstatus;
		if (exitstatus > 0)
			lastfail = exitstatus;
	}
}

static int
xstrtoint(int *res, const char *s, int base)
{
	if (s && res && *s && (isdigit(*s) || *s == '+' || *s == '-')) {
		char *end;
		long l;
		errno = 0;
		l = strtol(s, &end, base);
		if (!errno && l <= INT_MAX && l >= INT_MIN
				&& *end == '\0') {
			*res = (int)(l);
			return 0;
		}
	}
	return -1;
}

/*
 * ===========================================================================
 * error checking functions
 */
static int
wexasprintf(char **strp, const char *fmt, ...)
{
	va_list ap, ap2;
	int sz, ret;

	va_start(ap, fmt);
	va_start(ap2, fmt);

	errno = 0;
	sz = vsnprintf(NULL, 0, fmt, ap2);
	va_end(ap2);

	if (sz < 0 || !(*strp = wemalloc((size_t)(sz) + 1)))
		return -1;
	ret = vsprintf(*strp, fmt, ap);
	va_end(ap);

	return ret;
}

static int
weclose(int fd)
{
	int ret = close(fd);
	if (ret < 0)
		logerr("close:");
	return ret;
}

static int
weglob(const char *pattern, int flags,
		int (*errfunc)(const char *epath, int eerrno), glob_t *pglob)
{
	int ret = glob(pattern, flags, errfunc, pglob);
	switch (ret) {
	case GLOB_NOSPACE:
		logerr("glob: out of memory");
		break;
	case GLOB_ABORTED:
		logerr("glob: read error");
		break;
	/* GLOB_NOMATCH isn't really an error */
	}
	return ret;
}

static void *
wemalloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr)
		logerr("malloc: out of memory");
	return ptr;
}

static void *
wemallocarray(size_t nmemb, size_t size)
{
	/* overflow checking taken from musl's calloc implementation */
	if (size > 0 && nmemb > SIZE_MAX / size) {
		errno = ENOMEM;
		logerr("malloc: out of memory");
		return NULL;
	}
	return wemalloc(nmemb * size);
}

static void *
werealloc(void *ptr, size_t size)
{
	void *newptr = realloc(ptr, size);
	if (!newptr)
		logerr("realloc: out of memory");
	return newptr;
}

static void *
wereallocarray(void *ptr, size_t nmemb, size_t size)
{
	/* overflow checking taken from musl's calloc implementation */
	if (size > 0 && nmemb > SIZE_MAX / size) {
		errno = ENOMEM;
		logerr("realloc: out of memory");
		return NULL;
	}
	return werealloc(ptr, nmemb * size);
}

static char *
westrdup(const char *s)
{
	char *sdup = strdup(s);
	if (!sdup)
		logerr("strdup: out of memory");
	return sdup;
}

static char *
westrndup(const char *s, size_t n)
{
	char *sdup = strndup(s, n);
	if (!sdup)
		logerr("strndup: out of memory");
	return sdup;
}

static int
wexstrtoint(int *res, const char *s, int base)
{
	if (!s) {
		logerr("converting string to integer: bad input pointer");
	} else if (!res) {
		logerr("converting string to integer: bad output pointer");
	} else if (*s == '\0' || !(isdigit(*s) || *s == '+' || *s == '-')) {
		logerr("converting string to integer: not a number");
	} else {
		char *end;
		long l;
		errno = 0;
		l = strtol(s, &end, base);
		if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX)) {
			logerr("converting string to integer: "
					"integer overflow");
		} else if (l < INT_MIN || (errno == ERANGE
					&& l == LONG_MIN)) {
			logerr("converting string to integer: "
					"integer underflow");
		} else if (*end) {
			logerr("converting string to integer: "
					"extra characters at end of input");
		} else if (errno) {
			logerr("converting string to integer:");
		} else {
			*res = (int)(l);
			return 0;
		}
	}
	return -1;
}

/*
 * ===========================================================================
 * error logging functions
 */
static void
logerr(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vlogerr(fmt, ap);
	va_end(ap);
}

static void
vlogerr(const char *fmt, va_list ap)
{
	if (fmt) {
		if (argv0)
			fprintf(stderr, "%s: ", argv0);
		vfprintf(stderr, fmt, ap);
		if (fmt[0] && fmt[strlen(fmt) - 1] == ':')
			fprintf(stderr, " %s\n", strerror(errno));
		else
			putc('\n', stderr);
	}
}

/*
 * ===========================================================================
 * the main() function
 */
int
main(int argc, char *argv[])
{
	int interactive = 0;
	char *cmdline = NULL;
	if (!argc)
		return 1;
	argv0 = argv[0];

#if defined(ENABLE_PLEDGE)
	if (pledge("stdio rpath wpath cpath tty proc exec", NULL) < 0) {
		logerr("pledge:");
		return 1;
	}
#endif /* ENABLE_PLEDGE */

	if (isatty(STDOUT_FILENO) && isatty(STDERR_FILENO)) {
		/* ignore SIGTTOU */
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_IGN;

		if (sigaction(SIGTTOU, &sa, NULL) < 0) {
			logerr("sigaction:");
			return 1;
		}

		/* make the shell interactive */
		interactive = 1;
		term = STDOUT_FILENO;
		shell_pgid = getpgrp();
	}

	if (optparse(0, argc, argv, &cmdline) < 0)
		return 1;	
	if (cmdline) {
		takecmd(cmdline);
	} else {
		char *line = NULL;
		size_t lsize = 0;
		for (;;) {
			if (interactive) {
				fputs(prompt, stdout);
				fflush(stdout);
			}
			errno = 0;
			if (getline(&line, &lsize, stdin) < 0) {
				if (!errno && interactive
						&& (opts & OPT_IGNOREEOF)) {
					fputs("use 'exit' to exit the "
							"shell.\n", stderr);

					/* clear eof indicator */
					clearerr(stdin);
					continue;
				} else {
					break;
				}
			}
			takecmd(line);
		}
		free(line);
	}
	return 0;
}
