// --- src/histo_io.c ---
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "histo.h"
int charger_histo(const char *chemin, Histo **h, int *n) {
    FILE *fp = fopen(chemin, "r");
    if (!fp) {
        perror("charger_histo: fopen");
        return -1;
    }
    char line[256];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        *n = 0;
        *h = NULL;
        return 0;
    }
    Histo *tab = NULL;
    int cap = 0, cnt = 0;
    while (fgets(line, sizeof(line), fp)) {
        Histo rec;
        char temp_transaction[16], temp_resultat[16];
        if (sscanf(line, "%d %d %15s %d %15s",
                   &rec.reference, &rec.agence, temp_transaction,
                   &rec.valeur, temp_resultat) == 5) {
            if (cnt >= cap) {
                cap = cap ? cap * 2 : 4;
                Histo *new_tab = realloc(tab, cap * sizeof(Histo));
                if (!new_tab) {
                    free(tab);
                    fclose(fp);
                    perror("charger_histo: realloc");
                    return -1;
                }
                tab = new_tab;
            }
            strncpy(rec.transaction, temp_transaction, sizeof(rec.transaction));
            rec.transaction[sizeof(rec.transaction)-1] = '\0';
            strncpy(rec.resultat, temp_resultat, sizeof(rec.resultat));
            rec.resultat[sizeof(rec.resultat)-1] = '\0';
            tab[cnt++] = rec;
        }
    }
    fclose(fp);
    *h = tab;
    *n = cnt;
    return 0;
}
int sauvegarder_histo(const char *chemin, Histo *h, int n) {
    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", chemin);
    FILE *fp = fopen(temp_path, "w");
    if (!fp) {
        perror("sauvegarder_histo: fopen");
        return -1;
    }
    fprintf(fp, "Référence Vol Agence Transaction Valeur Résultat\n");
    for (int i = 0; i < n; i++)
        fprintf(fp, "%d %d %s %d %s\n",
                h[i].reference, h[i].agence, h[i].transaction,
                h[i].valeur, h[i].resultat);
    fclose(fp);
    if (rename(temp_path, chemin) != 0) {
        remove(temp_path);
        perror("sauvegarder_histo: rename");
        return -1;
    }
    return 0;
}
