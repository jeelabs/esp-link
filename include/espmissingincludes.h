#ifndef ESPMISSINGINCLUIDES_H
#define ESPMISSINGINCLUIDES_H

#include <ets_sys.h>

//Missing function prototypes in include folders. Gcc will warn on these if we don't define 'em anywhere.
//MOST OF THESE ARE GUESSED! but they seem to swork and shut up the compiler.
void ets_isr_attach(int routine, void* something, void *buff);
void uart_div_modify(int no, int div);
void ets_isr_unmask(int something);
void ets_install_putc1(void* routine);
void *pvPortMalloc(size_t xWantedSize);
void pvPortFree(void *ptr);
void *vPortMalloc(size_t xWantedSize);
void vPortFree(void *ptr);
void *ets_memcpy(void *dest, const void *src, size_t n);
void *ets_memset(void *s, int c, size_t n);
void ets_timer_arm_new(ETSTimer *a, int b, int c, int isMstimer);
void ets_timer_setfn(ETSTimer *t, ETSTimerFunc *fn, void *parg);
void ets_timer_disarm(ETSTimer *a);
int atoi(const char *nptr);
int ets_strncmp(const char *s1, const char *s2, int len);
int ets_strcmp(const char *s1, const char *s2);
int ets_strlen(const char *s);
char *ets_strcpy(char *dest, const char *src);
char *ets_strncpy(char *dest, const char *src, size_t n);
char *ets_strstr(const char *haystack, const char *needle);
int ets_sprintf(char *str, const char *format, ...)  __attribute__ ((format (printf, 2, 3)));
int os_printf(const char *format, ...)  __attribute__ ((format (printf, 1, 2)));
void uart_div_modify(int no, int freq);
void ets_isr_unmask(int intr);
void ets_install_putc1(void *routine);
void ets_isr_attach(int intr, void *handler, void *arg);

#endif