#ifndef CRON_H
#define CRON_H

#include <stdio.h>

#define MAX_LINE_LENGTH 1024
#define MAX_ENV_VARS 50
#define MAX_ENV_LEN 256

typedef struct {
    char name[64];
    char minute[16];
    char hour[16];
    char day_month[16];
    char month[16];
    char day_week[16];
    char command[768];
    int line_number;
    char env_vars[MAX_ENV_VARS][MAX_ENV_LEN];
    int env_count;
    int is_reboot;
} CronTask;

typedef struct {
    int id;
    int line_number;
    char name[64];
    char command[768];
    int exit_code;
    char env_vars[MAX_ENV_VARS][MAX_ENV_LEN];
    int env_count;
} TaskResult;

char *trim(char *str);
int parse_cron_task(const char *line, int line_num, CronTask *task);
void calculate_next_run(const CronTask *task);
void get_next_run_string(const CronTask *task, char *dest, size_t dest_size);
int execute_task_silenced(const CronTask *task, int silent);
int validate_crontab_file(const char *filename);
void print_tasks_json(CronTask tasks[], int task_count, const char *source);

#endif