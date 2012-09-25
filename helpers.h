/* Misc. utility functions and C-library extensions to simplify finit system setup.
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2012  Joachim Nilsson <troglobit@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef FINIT_HELPERS_H_
#define FINIT_HELPERS_H_

int	makepath	(char *);
void	ifconfig	(char *, char *, char *, int);
void	copyfile	(char *, char *, int);
void    print_descr     (char *action, char *descr);
int     print_result    (int fail);
int     start_process   (char *cmd, char *args[], int console);
void    cls             (void);
void	chomp		(char *);
int     getuser         (char *);
int	getgroup	(char *);
void    set_procname    (char *args[], char *name);
char   *get_pidname     (pid_t pid, char *name, size_t len);
int     kill_procname   (const char *name, int signo);
void    set_hostname    (char *hostname);

int     run             (char *cmd);
int     run_interactive (char *cmd, char *fmt, ...);
pid_t   run_getty       (char *cmd, char *argv[]);
int	run_parts	(char *dir, ...);

#endif /* FINIT_HELPERS_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
