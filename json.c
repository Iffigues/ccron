#include <stdio.h>
#include "cron.h"

void print_tasks_json(CronTask tasks[], int task_count, const char *source) {
    printf("{\n");
    printf("  \"source\": \"%s\",\n", source);
    printf("  \"task_count\": %d,\n", task_count);
    printf("  \"tasks\": [\n");
    for (int i = 0; i < task_count; i++) {
        printf("    {\n");
        printf("      \"id\": %d,\n", i + 1);
        printf("      \"line_number\": %d,\n", tasks[i].line_number);
        printf("      \"name\": \"%s\",\n", tasks[i].name);
        printf("      \"schedule\": \"%s %s %s %s %s\",\n", 
               tasks[i].minute, tasks[i].hour, tasks[i].day_month, tasks[i].month, tasks[i].day_week);
        printf("      \"command\": \"%s\",\n", tasks[i].command);
        printf("      \"env_count\": %d\n", tasks[i].env_count);
        printf("    }%s\n", (i < task_count - 1) ? "," : "");
    }
    printf("  ]\n");
    printf("}\n");
}