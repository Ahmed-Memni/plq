// --- src/file_io.c ---
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vol.h"
int charger_vols(const char *chemin, Vol **vols, int *nb_vols) {
    FILE *fp = fopen(chemin, "r");
    if (!fp) {
        perror("charger_vols: fopen");
        return -1;
    }
    char ligne[256];
    if (!fgets(ligne, sizeof(ligne), fp)) {
        fclose(fp);
        perror("charger_vols: fgets header");
        return -1;
    }
    // Validate header
    if (strncmp(ligne, "Référence Vol Destination Nombre Places Prix Place", 46) != 0) {
        fclose(fp);
        fprintf(stderr, "charger_vols: invalid header in %s\n", chemin);
        return -1;
    }
    Vol *tab = NULL;
    int capacite = 0, n = 0;
    while (fgets(ligne, sizeof(ligne), fp)) {
        int ref, places;
        char dest[64]; float prix;
        if (sscanf(ligne, "%d %63s %d %f", &ref, dest, &places, &prix) == 4) {
            if (n >= capacite) {
                capacite = capacite ? capacite * 2 : 4;
                Vol *new_tab = realloc(tab, capacite * sizeof(Vol));
                if (!new_tab) {
                    free(tab);
                    fclose(fp);
                    perror("charger_vols: realloc");
                    return -1;
                }
                tab = new_tab;
            }
            tab[n].reference = ref;
            strcpy(tab[n].destination, dest);
            tab[n].nombre_places = places;
            tab[n].prix_place = prix;
            n++;
        }
    }
    fclose(fp);
    if (n == 0) {
        free(tab);
        fprintf(stderr, "charger_vols: no valid data lines in %s\n", chemin);
        return -1;
    }
    *vols = tab;
    *nb_vols = n;
    return 0;
}
int sauvegarder_vols(const char *chemin, Vol *vols, int nb_vols) {
    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", chemin);
    FILE *fp = fopen(temp_path, "w");
    if (!fp) {
        perror("sauvegarder_vols: fopen");
        return -1;
    }
    fprintf(fp, "Référence Vol Destination Nombre Places Prix Place\n");
    for (int i = 0; i < nb_vols; i++) {
        fprintf(fp, "%d %s %d %.2f\n",
                vols[i].reference, vols[i].destination,
                vols[i].nombre_places, vols[i].prix_place);
    }
    fclose(fp);
    if (rename(temp_path, chemin) != 0) {
        remove(temp_path);
        perror("sauvegarder_vols: rename");
        return -1;
    }
    return 0;
}
