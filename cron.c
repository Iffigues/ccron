#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include "cron.h"

char *trim(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Exécution avec paramètre `silent` (redirige stdout/stderr vers /dev/null si actif)
int execute_task_silenced(const CronTask *task, int silent) {
    if (!silent) {
        fprintf(stderr, "\n[+] Exécution de la tâche (Ligne %d)%s%s%s%s...\n", 
                task->line_number, 
                task->name[0] ? " [Nom: " : "", 
                task->name[0] ? task->name : "",
                task->user[0] ? " [User: " : "",
                task->user[0] ? task->user : "");
        if (task->user[0]) {
            fprintf(stderr, "[+] Utilisateur cible : %s\n", task->user);
        }
        fprintf(stderr, "[+] Commande : %s\n", task->command);

        if (task->env_count > 0) {
            fprintf(stderr, "[+] Environnement injecté (%d variables) :\n", task->env_count);
            for (int i = 0; i < task->env_count; i++) {
                fprintf(stderr, "    -> %s\n", task->env_vars[i]);
            }
        } else {
            fprintf(stderr, "[+] Aucun environnement spécifique (utilisation des valeurs par défaut)\n");
        }
        fprintf(stderr, "----------------------------------------\n");
    }

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        if (silent) {
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
        }

        // Gestion optionnelle du changement d'utilisateur si spécifié et si on est root
        if (task->user[0] != '\0') {
            struct passwd *pw = getpwnam(task->user);
            if (pw != NULL) {
                initgroups(task->user, pw->pw_gid);
                setgid(pw->pw_gid);
                setuid(pw->pw_uid);
            }
        }

        int has_path = 0;
        int has_shell = 0;

        for (int i = 0; i < task->env_count; i++) {
            if (strncmp(task->env_vars[i], "PATH=", 5) == 0) has_path = 1;
            if (strncmp(task->env_vars[i], "SHELL=", 6) == 0) has_shell = 1;
        }

        int total_env = task->env_count + (!has_path ? 1 : 0) + (!has_shell ? 1 : 0) + 1;
        
        char **envp = malloc(total_env * sizeof(char *));
        if (!envp) {
            _exit(1);
        }

        int idx = 0;
        for (int i = 0; i < task->env_count; i++) {
            envp[idx++] = (char *)task->env_vars[i];
        }
        if (!has_path) {
            envp[idx++] = (char *)"PATH=/usr/bin:/bin";
        }
        if (!has_shell) {
            envp[idx++] = (char *)"SHELL=/bin/sh";
        }
        envp[idx] = NULL;

        char *args[] = { (char *)"sh", (char *)"-c", (char *)task->command, NULL };
        
        execve("/bin/sh", args, envp);
        free(envp);
        _exit(127);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (!silent) {
            fprintf(stderr, "----------------------------------------\n");
        }
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (!silent) {
                fprintf(stderr, "[+] Processus terminé avec le code de retour : %d\n", exit_code);
            }
            return exit_code;
        } else {
            return -1;
        }
    }
}

int parse_cron_task(const char *line, int line_num, CronTask *task) {
    // Nettoyage complet de la structure pour éviter les résidus des lignes précédentes
    memset(task, 0, sizeof(CronTask));

    task->line_number = line_num;

    if (line[0] == '@') {
        char macro[32];
        char cmd[768];
        if (sscanf(line, "%31s %[^\n]", macro, cmd) == 2) {
            strcpy(task->schedule, macro); // Stocke bien la macro
            if (strcmp(macro, "@yearly") == 0 || strcmp(macro, "@annually") == 0) {
                strcpy(task->minute, "0"); strcpy(task->hour, "0"); strcpy(task->day_month, "1"); strcpy(task->month, "1"); strcpy(task->day_week, "*");
            } else if (strcmp(macro, "@monthly") == 0) {
                strcpy(task->minute, "0"); strcpy(task->hour, "0"); strcpy(task->day_month, "1"); strcpy(task->month, "*"); strcpy(task->day_week, "*");
            } else if (strcmp(macro, "@weekly") == 0) {
                strcpy(task->minute, "0"); strcpy(task->hour, "0"); strcpy(task->day_month, "*"); strcpy(task->month, "*"); strcpy(task->day_week, "0");
            } else if (strcmp(macro, "@daily") == 0 || strcmp(macro, "@midnight") == 0) {
                strcpy(task->minute, "0"); strcpy(task->hour, "0"); strcpy(task->day_month, "*"); strcpy(task->month, "*"); strcpy(task->day_week, "*");
            } else if (strcmp(macro, "@hourly") == 0) {
                strcpy(task->minute, "0"); strcpy(task->hour, "*"); strcpy(task->day_month, "*"); strcpy(task->month, "*"); strcpy(task->day_week, "*");
            } else if (strcmp(macro, "@reboot") == 0) {
                task->is_reboot = 1;
                strcpy(task->minute, "@reboot");
            } else {
                return 0;
            }
            strcpy(task->command, cmd);
            return 1;
        }
        return 0;
    }

    // ... le reste de ta fonction inchangé ...

    // 2. Format système (5 champs de temps + 1 nom d'utilisateur valide + commande)
    char f1[16], f2[16], f3[16], f4[16], f5[16], token6[64], cmd[768];
    int ret = sscanf(line, "%15s %15s %15s %15s %15s %63s %[^\n]", f1, f2, f3, f4, f5, token6, cmd);

    if (ret >= 7) {
        struct passwd *pw = getpwnam(token6);
        if (pw != NULL) {
            strcpy(task->minute, f1);
            strcpy(task->hour, f2);
            strcpy(task->day_month, f3);
            strcpy(task->month, f4);
            strcpy(task->day_week, f5);
            strcpy(task->user, token6);
            // On stocke l'expression temporelle complète dans schedule
            snprintf(task->schedule, sizeof(task->schedule), "%s %s %s %s %s", f1, f2, f3, f4, f5);
            strcpy(task->command, cmd);
            return 1;
        }
    }

    // 3. Format standard à 5 champs (utilisateur courant implicite)
    ret = sscanf(line, "%15s %15s %15s %15s %15s %[^\n]",
                 task->minute,
                 task->hour,
                 task->day_month,
                 task->month,
                 task->day_week,
                 task->command);

    if (ret < 6) {
        return 0;
    }

    // Stocke l'expression temporelle complète
    snprintf(task->schedule, sizeof(task->schedule), "%s %s %s %s %s", 
             task->minute, task->hour, task->day_month, task->month, task->day_week);

    return 1;
}

// --- Moteur Dry-Run ---

static int match_single_val(const char *expr, int val) {
    if (strcmp(expr, "*") == 0) return 1;

    int step = 1;
    char expr_copy[32];
    strncpy(expr_copy, expr, sizeof(expr_copy) - 1);
    expr_copy[sizeof(expr_copy) - 1] = '\0';

    char *slash = strchr(expr_copy, '/');
    if (slash) {
        *slash = '\0';
        step = atoi(slash + 1);
        if (step <= 0) step = 1;
    }

    if (strcmp(expr_copy, "*") == 0) {
        return (val % step) == 0;
    }

    char *dash = strchr(expr_copy, '-');
    if (dash) {
        *dash = '\0';
        int start = atoi(expr_copy);
        int end = atoi(dash + 1);
        if (val >= start && val <= end) {
            return ((val - start) % step) == 0;
        }
        return 0;
    }

    return atoi(expr_copy) == val;
}

static int match_cron_field(const char *field, int val) {
    char field_copy[64];
    strncpy(field_copy, field, sizeof(field_copy) - 1);
    field_copy[sizeof(field_copy) - 1] = '\0';

    char *token = strtok(field_copy, ",");
    while (token != NULL) {
        if (match_single_val(token, val)) {
            return 1;
        }
        token = strtok(NULL, ",");
    }
    return 0;
}

void calculate_next_run(const CronTask *task) {
    if (task->is_reboot) {
        printf("[+] Prochaine exécution : Au prochain redémarrage du système (@reboot)\n");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    t->tm_sec = 0;
    t->tm_min += 1;
    mktime(t);

    for (int i = 0; i < 525600; i++) {
        int min = t->tm_min;
        int hour = t->tm_hour;
        int day = t->tm_mday;
        int month = t->tm_mon + 1;
        int dow = t->tm_wday;

        if (match_cron_field(task->minute, min) &&
            match_cron_field(task->hour, hour) &&
            match_cron_field(task->day_month, day) &&
            match_cron_field(task->month, month) &&
            match_cron_field(task->day_week, dow)) {
            
            char buffer[80];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", t);
            printf("[+] Prochaine exécution prévue le : %s (dans ~%d minutes)\n", buffer, i + 1);
            return;
        }

        t->tm_min += 1;
        mktime(t);
    }
    printf("[!] Aucune exécution trouvée dans l'année à venir.\n");
}

void get_next_run_string(const CronTask *task, char *dest, size_t dest_size) {
    if (task->is_reboot) {
        snprintf(dest, dest_size, "Au prochain redémarrage du système (@reboot)");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    t->tm_sec = 0;
    t->tm_min += 1;
    mktime(t);

    for (int i = 0; i < 525600; i++) {
        int min = t->tm_min;
        int hour = t->tm_hour;
        int day = t->tm_mday;
        int month = t->tm_mon + 1;
        int dow = t->tm_wday;

        if (match_cron_field(task->minute, min) &&
            match_cron_field(task->hour, hour) &&
            match_cron_field(task->day_month, day) &&
            match_cron_field(task->month, month) &&
            match_cron_field(task->day_week, dow)) {
            
            char buffer[80];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", t);
            snprintf(dest, dest_size, "%s (dans ~%d minutes)", buffer, i + 1);
            return;
        }

        t->tm_min += 1;
        mktime(t);
    }
    snprintf(dest, dest_size, "Aucune exécution trouvée dans l'année à venir");
}