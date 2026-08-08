#ifndef CONSOLE_H
#define CONSOLE_H

message_type get_message_type(char* command);
void communication_with_coord(int writefd, int readfd, char* line);
void console(int writefd, int readfd, char* operations_file);

#endif