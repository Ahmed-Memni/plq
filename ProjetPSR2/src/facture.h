// --- src/facture.h ---
#ifndef FACTURE_H
#define FACTURE_H
typedef struct {
    int agence; // Réf Agence
    double somme; // Somme à payer
} Facture;
int charger_facture(const char *chemin, Facture **f, int *n);
int sauvegarder_facture(const char *chemin, Facture *f, int n);
#endif // FACTURE_H
