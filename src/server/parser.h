/*
 * parser.h
 *
 *  Created on: 2008-08-16
 *      Author: rotunda
 */

#ifndef PARSER_H_
#define PARSER_H_

int32_t Tune_option(char *name, char *val);
bool Parser(int32_t argc, char **argv);
int32_t Parser_list_option(int32_t *index, char *buf);
int32_t Get_option_value(const char *name, char *value, uint32_t size);


#endif /* PARSER_H_ */
