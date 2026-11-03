#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include "cron.h"

#define MAX_TASKS 100

// Fonction pour exécuter une tâche via fork / execve avec son environnement dédié
void execute_task(const CronTask *task) {
    printf("\n[+] Exécution de la tâche (Ligne %d)%s%s...\n", 
           task->line_number, 
           task->name[0] ? " [Nom: " : "", 
           task->name[0] ? task->name : "");
    printf("[+] Commande : %s\n", task->command);

    if (task->env_count > 0) {
        printf("[+] Environnement injecté (%d variables) :\n", task->env_count);
        for (int i = 0; i < task->env_count; i++) {
            printf("    -> %s\n", task->env_vars[i]);
        }
    } else {
        printf("[+] Aucun environnement spécifique (utilisation des valeurs par défaut)\n");
    }
    printf("----------------------------------------\n");

    pid_t pid = fork();
    if (pid < 0) {
        perror("Erreur lors du fork");
        return;
    }

    if (pid == 0) {
        int has_path = 0;
        int has_shell = 0;

        for (int i = 0; i < task->env_count; i++) {
            if (strncmp(task->env_vars[i], "PATH=", 5) == 0) has_path = 1;
            if (strncmp(task->env_vars[i], "SHELL=", 6) == 0) has_shell = 1;
        }

        int total_env = task->env_count + (!has_path ? 1 : 0) + (!has_shell ? 1 : 0) + 1;
        
        char **envp = malloc(total_env * sizeof(char *));
        if (!envp) {
            perror("Erreur allocation mémoire envp");
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
        
        perror("Erreur d'exécution de execve");
        free(envp);
        _exit(127);
    } else {
        int status;
        waitpid(pid, &status, 0);
        printf("----------------------------------------\n");
        if (WIFEXITED(status)) {
            printf("[+] Processus terminé avec le code de retour : %d\n", WEXITSTATUS(status));
        } else {
            printf("[!] Processus terminé anormalement.\n");
        }
    }
}

// Fonction utilitaire pour ajouter ou mettre à jour une variable d'environnement (style cron)
void set_env(char env_vars[MAX_ENV_VARS][MAX_ENV_LEN], int *env_count, const char *env_line) {
    char line_copy[MAX_ENV_LEN];
    strncpy(line_copy, env_line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';

    char *eq = strchr(line_copy, '=');
    if (!eq) return;

    *eq = '\0';
    char *key = trim(line_copy);
    char *val = trim(eq + 1);

    size_t val_len = strlen(val);
    if (val_len >= 2) {
        if ((val[0] == '"' && val[val_len - 1] == '"') || 
            (val[0] == '\'' && val[val_len - 1] == '\'')) {
            val[val_len - 1] = '\0';
            val++;
        }
    }

    char formatted_env[MAX_ENV_LEN];
    snprintf(formatted_env, sizeof(formatted_env), "%s=%s", key, val);

    int found = -1;
    size_t key_len = strlen(key);
    for (int i = 0; i < *env_count; i++) {
        if (strncmp(env_vars[i], key, key_len) == 0 && env_vars[i][key_len] == '=') {
            found = i;
            break;
        }
    }

    if (found != -1) {
        strncpy(env_vars[found], formatted_env, MAX_ENV_LEN - 1);
        env_vars[found][MAX_ENV_LEN - 1] = '\0';
    } else if (*env_count < MAX_ENV_VARS) {
        strncpy(env_vars[*env_count], formatted_env, MAX_ENV_LEN - 1);
        env_vars[*env_count][MAX_ENV_LEN - 1] = '\0';
        (*env_count)++;
    }
}

int main(int argc, char *argv[]) {
    FILE *file = NULL;
    int use_popen = 0;
    char *crontab_source = "crontab -l (défaut)";
    char *target_arg = NULL;
    int dry_run = 0;

    // Analyse des arguments pour détecter le mode --next / -n
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--next") == 0 || strcmp(argv[i], "-n") == 0) {
            dry_run = 1;
        }
    }

    // Filtrage des arguments pour récupérer le fichier et la cible (index ou nom)
    int non_flag_argc = 0;
    char *non_flag_argv[3];
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--next") != 0 && strcmp(argv[i], "-n") != 0) {
            if (non_flag_argc < 3) {
                non_flag_argv[non_flag_argc++] = argv[i];
            }
        }
    }

    if (non_flag_argc == 0) {
        file = popen("crontab -l 2>/dev/null", "r");
        use_popen = 1;
    } else if (non_flag_argc == 1) {
        if (access(non_flag_argv[0], F_OK) == 0) {
            crontab_source = non_flag_argv[0];
            file = fopen(non_flag_argv[0], "r");
            use_popen = 0;
        } else {
            target_arg = non_flag_argv[0];
            file = popen("crontab -l 2>/dev/null", "r");
            use_popen = 1;
        }
    } else if (non_flag_argc >= 2) {
        crontab_source = non_flag_argv[0];
        file = fopen(non_flag_argv[0], "r");
        use_popen = 0;
        target_arg = non_flag_argv[1];
    }

    if (!file) {
        fprintf(stderr, "Erreur : Impossible d'accéder au crontab ou d'ouvrir le fichier.\n");
        fprintf(stderr, "Usage: %s [fichier_crontab] [numero_ou_nom] [--next|-n]\n", argv[0]);
        return 1;
    }

    CronTask tasks[MAX_TASKS];
    int task_count = 0;
    char line[MAX_LINE_LENGTH];
    int line_num = 0;
    
    char current_name[64] = "";
    char current_env[MAX_ENV_VARS][MAX_ENV_LEN];
    int current_env_count = 0;

    while (fgets(line, sizeof(line), file) && task_count < MAX_TASKS) {
        line_num++;
        char *trimmed = trim(line);

        if (trimmed[0] == '\0') continue;

        // 1. Nom personnalisé (# @nom)
        if (strncmp(trimmed, "# @", 3) == 0) {
            strncpy(current_name, trim(trimmed + 3), sizeof(current_name) - 1);
            current_name[sizeof(current_name) - 1] = '\0';
            continue;
        }

        // 2. Commentaires classiques
        if (trimmed[0] == '#') continue;

        // 3. Variables d'environnement (contient '=' et ne commence ni par un chiffre, ni par '*', ni par '@')
        if (strchr(trimmed, '=') && !isdigit((unsigned char)trimmed[0]) && trimmed[0] != '*' && trimmed[0] != '@') {
            set_env(current_env, &current_env_count, trimmed);
            continue;
        }

        // 4. Tâche cron (standard ou macro @...)
        if (parse_cron_task(trimmed, line_num, &tasks[task_count])) {
            strcpy(tasks[task_count].name, current_name);
            current_name[0] = '\0'; 

            tasks[task_count].env_count = current_env_count;
            for (int i = 0; i < current_env_count; i++) {
                strcpy(tasks[task_count].env_vars[i], current_env[i]);
            }

            task_count++;
        }
    }

    if (use_popen) pclose(file);
    else fclose(file);

    if (task_count == 0) {
        printf("Aucune tâche cron valide trouvée.\n");
        return 0;
    }

    // Traitement si une cible (index ou nom) est demandée
    if (target_arg != NULL) {
        int found_index = -1;

        if (isdigit((unsigned char)target_arg[0])) {
            int idx = atoi(target_arg);
            if (idx > 0 && idx <= task_count) {
                found_index = idx - 1;
            }
        } else {
            for (int i = 0; i < task_count; i++) {
                if (strcmp(tasks[i].name, target_arg) == 0) {
                    found_index = i;
                    break;
                }
            }
        }

        if (found_index != -1) {
            if (dry_run) {
                printf("[Dry-Run] Analyse de la tâche : [%s] %s\n",
                       tasks[found_index].is_reboot ? "@reboot" : tasks[found_index].minute,
                       tasks[found_index].command);
                calculate_next_run(&tasks[found_index]);
            } else {
                execute_task(&tasks[found_index]);
            }
        } else {
            fprintf(stderr, "Erreur : Tâche '%s' introuvable.\n", target_arg);
            return 1;
        }
        return 0;
    }

    // Mode listing général (avec ou sans dry-run global)
    printf("=== Analyse de la source : %s ===\n", crontab_source);
    for (int i = 0; i < task_count; i++) {
        printf("[%d] Ligne %d %s-> [%s] %s\n",
               i + 1,
               tasks[i].line_number,
               tasks[i].name[0] ? tasks[i].name : "",
               tasks[i].is_reboot ? "@reboot" : tasks[i].minute,
               tasks[i].command);
        if (tasks[i].name[0]) {
            printf("      Nom : @%s\n", tasks[i].name);
        }
        if (dry_run) {
            calculate_next_run(&tasks[i]);
        }
    }

    if (dry_run) return 0;

    // Menu interactif
    int choice;
    printf("\nEntrez le numéro de la tâche à tester (1-%d, ou 0 pour quitter) : ", task_count);
    if (scanf("%d", &choice) == 1) {
        if (choice > 0 && choice <= task_count) {
            execute_task(&tasks[choice - 1]);
        } else {
            printf("Sortie.\n");
        }
    }

    return 0;
}