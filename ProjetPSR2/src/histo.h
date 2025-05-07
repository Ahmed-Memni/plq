// --- src/histo.h ---
#ifndef HISTO_H
#define HISTO_H
typedef struct {
    int reference; // Référence Vol
    int agence; // Réf Agence
    char transaction[16]; // "Demande" ou "Annulation"
    int valeur; // Nombre places
    char resultat[16]; // "succès" ou "impossible"
} Histo;
int charger_histo(const char *chemin, Histo **h, int *n);
int sauvegarder_histo(const char *chemin, Histo *h, int n);
#endif // HISTO_H
