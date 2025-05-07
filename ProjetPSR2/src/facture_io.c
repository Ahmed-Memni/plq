// --- src/facture_io.c ---
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "facture.h"
int charger_facture(const char *chemin, Facture **f, int *n) {
    FILE *fp = fopen(chemin, "r");
    if (!fp) {
        perror("charger_facture: fopen");
        return -1;
    }
    char line[256];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        *n = 0;
        *f = NULL;
        return 0;
    }
    Facture *tab = NULL;
    int cap = 0, cnt = 0;
    while (fgets(line, sizeof(line), fp)) {
        Facture rec;
        if (sscanf(line, "%d %lf", &rec.agence, &rec.somme) == 2) {
            if (cnt >= cap) {
                cap = cap ? cap * 2 : 4;
                Facture *new_tab = realloc(tab, cap * sizeof(Facture));
                if (!new_tab) {
                    free(tab);
                    fclose(fp);
                    perror("charger_facture: realloc");
                    return -1;
                }
                tab = new_tab;
            }
            tab[cnt++] = rec;
        }
    }
    fclose(fp);
    *f = tab;
    *n = cnt;
    return 0;
}
int sauvegarder_facture(const char *chemin, Facture *f, int n) {
    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", chemin);
    FILE *fp = fopen(temp_path, "w");
    if (!fp) {
        perror("sauvegarder_facture: fopen");
        return -1;
    }
    fprintf(fp, "Référence Agence Somme à payer\n");
    for (int i = 0; i < n; i++)
        fprintf(fp, "%d %.2f\n", f[i].agence, f[i].somme);
    fclose(fp);
    if (rename(temp_path, chemin) != 0) {
        remove(temp_path);
        perror("sauvegarder_facture: rename");
        return -1;
    }
    return 0;
}
