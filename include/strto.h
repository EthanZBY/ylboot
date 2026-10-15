/*
 * header for strto utilities by Raymond
 * 
 * */
#ifndef _STRTO_H__
#define _STRTO_H__

#include <common.h>
#include <errno.h>
#include <linux/ctype.h>

ulong dectoul(const char *cp, char **endp);

ulong hextoul(const char *cp, char **endp);
unsigned long long hextoull(const char *cp, char **endp);
int strict_strtoul(const char *cp, unsigned int base, unsigned long *res);
unsigned long ustrtoul(const char *cp, char **endp, unsigned int base);
unsigned long long ustrtoull(const char *cp, char **endp, unsigned int base);
unsigned long long simple_strtoull(const char *cp, char **endp,unsigned int base);
long trailing_strtol(const char *str);

#endif

