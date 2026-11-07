#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "cron.h"

#define MAX_TASKS 100

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
            (val[0] == '\'' && val[val_len - 1] == '\'' )) {
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
    int strict_check = 0;
    int json_output = 0;
    int run_all = 0;

    // Analyse des arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--next") == 0 || strcmp(argv[i], "-n") == 0) {
            dry_run = 1;
        } else if (strcmp(argv[i], "--check") == 0) {
            strict_check = 1;
        } else if (strcmp(argv[i], "--json") == 0) {
            json_output = 1;
        } else if (strcmp(argv[i], "--all") == 0) {
            run_all = 1;
        }
    }

    // Gestion de l'option --check
    if (strict_check) {
        char *check_file = NULL;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--check") != 0 && strcmp(argv[i], "--json") != 0 && 
                strcmp(argv[i], "--next") != 0 && strcmp(argv[i], "-n") != 0 && 
                strcmp(argv[i], "--all") != 0) {
                check_file = argv[i];
                break;
            }
        }
        int success = validate_crontab_file(check_file);
        return success ? 0 : 1;
    }

    // Filtrage des arguments non-flags
    int non_flag_argc = 0;
    char *non_flag_argv[3];
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--next") != 0 && strcmp(argv[i], "-n") != 0 &&
            strcmp(argv[i], "--check") != 0 && strcmp(argv[i], "--json") != 0 &&
            strcmp(argv[i], "--all") != 0) {
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
        fprintf(stderr, "Usage: %s [fichier_crontab] [numero_ou_nom] [--next|-n] [--check] [--json] [--all]\n", argv[0]);
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

        if (strncmp(trimmed, "# @", 3) == 0) {
            strncpy(current_name, trim(trimmed + 3), sizeof(current_name) - 1);
            current_name[sizeof(current_name) - 1] = '\0';
            continue;
        }

        if (trimmed[0] == '#') continue;

        if (strchr(trimmed, '=') && !isdigit((unsigned char)trimmed[0]) && trimmed[0] != '*' && trimmed[0] != '@') {
            set_env(current_env, &current_env_count, trimmed);
            continue;
        }

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

    // Gestion du format JSON global si demandé (sans --all ni cible ni next)
    if (json_output && target_arg == NULL && !run_all && !dry_run) {
        print_tasks_json(tasks, task_count, crontab_source);
        return 0;
    }

    // Gestion de la combinaison --next --json
    if (dry_run && json_output) {
        printf("{\n");
        printf("  \"execution_mode\": \"dry_run\",\n");
        printf("  \"source\": \"%s\",\n", crontab_source);
        printf("  \"task_count\": %d,\n", task_count);
        printf("  \"tasks\": [\n");
        
        for (int i = 0; i < task_count; i++) {
            char next_run_buf[128];
            get_next_run_string(&tasks[i], next_run_buf, sizeof(next_run_buf));

            printf("    {\n");
            printf("      \"id\": %d,\n", i + 1);
            printf("      \"line_number\": %d,\n", tasks[i].line_number);
            printf("      \"name\": \"%s\",\n", tasks[i].name);
            printf("      \"schedule\": \"%s\",\n", tasks[i].schedule); // <-- Corrigé ici !
            printf("      \"command\": \"%s\",\n", tasks[i].command);
            printf("      \"env_count\": %d,\n", tasks[i].env_count);
            printf("      \"next_run\": \"%s\"\n", next_run_buf);
            printf("    }%s\n", (i < task_count - 1) ? "," : "");
        }
        printf("  ]\n}\n");
        return 0;
    }

    // Traitement d'une cible spécifique (index ou nom)
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
                return 0;
            } else {
                TaskResult res;
                res.id = found_index + 1;
                res.line_number = tasks[found_index].line_number;
                strncpy(res.name, tasks[found_index].name, sizeof(res.name) - 1);
                strncpy(res.command, tasks[found_index].command, sizeof(res.command) - 1);
                res.env_count = tasks[found_index].env_count;
                for (int j = 0; j < res.env_count; j++) {
                    strncpy(res.env_vars[j], tasks[found_index].env_vars[j], sizeof(res.env_vars[j]) - 1);
                }

                res.exit_code = execute_task_silenced(&tasks[found_index], json_output);

                if (json_output) {
                    printf("{\n");
                    printf("  \"execution_mode\": \"single_result\",\n");
                    printf("  \"source\": \"%s\",\n", crontab_source);
                    printf("  \"task\": {\n");
                    printf("    \"id\": %d,\n", res.id);
                    printf("    \"name\": \"%s\",\n", res.name);
                    printf("    \"command\": \"%s\",\n", res.command);
                    printf("    \"exit_code\": %d,\n", res.exit_code);
                    printf("    \"status\": \"%s\",\n", (res.exit_code == 0) ? "SUCCESS" : "FAILURE");
                    printf("    \"env\": [\n");
                    for (int j = 0; j < res.env_count; j++) {
                        printf("      \"%s\"%s\n", res.env_vars[j], (j < res.env_count - 1) ? "," : "");
                    }
                    printf("    ]\n");
                    printf("  }\n");
                    printf("}\n");
                }
                return res.exit_code;
            }
        } else {
            fprintf(stderr, "Erreur : Tâche '%s' introuvable.\n", target_arg);
            return 1;
        }
    }

    // Mode listing général (affiché uniquement si --json n'est pas actif)
    if (!json_output) {
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
    }

    if (dry_run) return 0;

    // Exécution de toutes les tâches avec --all
    if (run_all) {
        TaskResult *results = malloc(task_count * sizeof(TaskResult));
        if (!results) {
            perror("Erreur allocation mémoire");
            return 1;
        }

        int global_exit = 0;

        for (int i = 0; i < task_count; i++) {
            results[i].id = i + 1;
            results[i].line_number = tasks[i].line_number;
            strncpy(results[i].name, tasks[i].name, sizeof(results[i].name) - 1);
            results[i].name[sizeof(results[i].name) - 1] = '\0';

            strncpy(results[i].command, tasks[i].command, sizeof(results[i].command) - 1);
            results[i].command[sizeof(results[i].command) - 1] = '\0';

            results[i].env_count = tasks[i].env_count;
            for (int j = 0; j < results[i].env_count; j++) {
                strncpy(results[i].env_vars[j], tasks[i].env_vars[j], sizeof(results[i].env_vars[j]) - 1);
            }

            results[i].exit_code = execute_task_silenced(&tasks[i], json_output);
            if (results[i].exit_code != 0) {
                global_exit = results[i].exit_code;
            }
        }

        if (json_output) {
            printf("{\n");
            printf("  \"execution_mode\": \"all_results\",\n");
            printf("  \"source\": \"%s\",\n", crontab_source);
            printf("  \"task_count\": %d,\n", task_count);
            printf("  \"results\": [\n");
            
            for (int i = 0; i < task_count; i++) {
                printf("    {\n");
                printf("      \"id\": %d,\n", results[i].id);
                printf("      \"name\": \"%s\",\n", results[i].name);
                printf("      \"command\": \"%s\",\n", results[i].command);
                printf("      \"exit_code\": %d,\n", results[i].exit_code);
                printf("      \"status\": \"%s\",\n", (results[i].exit_code == 0) ? "SUCCESS" : "FAILURE");
                printf("      \"env\": [\n");
                for (int j = 0; j < results[i].env_count; j++) {
                    printf("        \"%s\"%s\n", results[i].env_vars[j], (j < results[i].env_count - 1) ? "," : "");
                }
                printf("      ]\n");
                printf("    }%s\n", (i < task_count - 1) ? "," : "");
            }
            printf("  ]\n}\n");
        } else {
            printf("\n[+] Exécution séquentielle terminée pour %d tâches.\n", task_count);
        }

        free(results);
        return global_exit;
    }

    // Menu interactif
    int choice;
    printf("\nEntrez le numéro de la tâche à tester (1-%d, ou 0 pour quitter) : ", task_count);
    if (scanf("%d", &choice) == 1) {
        if (choice > 0 && choice <= task_count) {
            return execute_task_silenced(&tasks[choice - 1], json_output);
        } else {
            printf("Sortie.\n");
        }
    }

    return 0;
}