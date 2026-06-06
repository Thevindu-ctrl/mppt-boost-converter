#ifndef _USART_H
#define _USART_H

void init_usart(unsigned int baud);
void send_usart(unsigned char d);
void print_terminal(const char *s);

#endif
