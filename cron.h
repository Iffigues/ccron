#ifndef CRON_H
#define CRON_H

#define MAX_LINE_LENGTH 1024
#define MAX_ENV_VARS 32
#define MAX_ENV_LEN 256

typedef struct {
    int id;
    int line_number;
    char name[64];
    char schedule[128];
    char minute[16];
    char hour[16];
    char day_month[16];
    char month[16];
    char day_week[16];
    char user[32];
    char command[768];
    int env_count;
    char env_vars[MAX_ENV_VARS][MAX_ENV_LEN];
    int is_reboot;
    char tz[64];
} CronTask;

typedef struct {
    int id;
    int line_number;
    char name[64];
    char command[768];
    int env_count;
    char env_vars[MAX_ENV_VARS][MAX_ENV_LEN];
    int exit_code;
} TaskResult;

int parse_cron_task(const char *line, int line_num, CronTask *task);
int validate_crontab_file(const char *filename);
int execute_task_silenced(const CronTask *task, int silent);
void calculate_next_run(const CronTask *task);
void get_next_run_string(const CronTask *task, char *dest, size_t dest_size);
char *trim(char *str);
void print_tasks_json(CronTask tasks[], int task_count, const char *source);

#endif