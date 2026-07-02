#ifndef SERIAL_H
#define SERIAL_H

void serial_init(void);
void serial_write_char(char value);
void serial_write(const char *text);

#endif
