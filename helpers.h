#ifndef __FINIT_HELPERS_H
#define __FINIT_HELPERS_H

int	makepath	(char *);
void	ifconfig	(char *, char *, char *, int);
void	copyfile	(char *, char *, int);
void	chomp		(char *);

#ifdef BUILTIN_RUNPARTS
int	run_parts	(char *, ...);
#endif

#endif
