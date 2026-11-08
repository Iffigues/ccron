#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "cron.h"

int validate_crontab_file(const char *filename) {
    FILE *file = NULL;
    int use_popen = 0;

    if (!filename) {
        file = popen("crontab -l 2>/dev/null", "r");
        use_popen = 1;
    } else {
        file = fopen(filename, "r");
        use_popen = 0;
    }

    if (!file) {
        fprintf(stderr, "[FAIL] Impossible d'ouvrir la source crontab pour validation.\n");
        return 1;
    }

    char line[MAX_LINE_LENGTH];
    int line_num = 0;
    int error_count = 0;
    CronTask dummy_task;

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        char *trimmed = trim(line);

        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            continue;
        }

        if (strchr(trimmed, '=') && !isdigit((unsigned char)trimmed[0]) && trimmed[0] != '*' && trimmed[0] != '@') {
            continue;
        }

        if (!parse_cron_task(trimmed, line_num, &dummy_task)) {
            fprintf(stderr, "[Ligne %d] Erreur de syntaxe cron : %s\n", line_num, trimmed);
            error_count++;
        }
    }

    if (use_popen) pclose(file);
    else fclose(file);

    if (error_count > 0) {
        fprintf(stderr, "Résultat : Échec de la validation (%d erreur(s) détectée(s)).\n", error_count);
        return 2;
    }

    printf("Succès : La syntaxe du crontab est valide.\n");
    return 0;
}