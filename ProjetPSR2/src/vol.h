// --- src/vol.h ---
#ifndef VOL_H
#define VOL_H
typedef struct {
    int reference; // Référence Vol
    char destination[64]; // Destination
    int nombre_places; // Nombre Places
    float prix_place; // Prix Place
} Vol;
// Charge les vols depuis le fichier `chemin`; alloue *vols et fixe *nb_vols
// Retourne 0 si succès, -1 sinon
int charger_vols(const char *chemin, Vol **vols, int *nb_vols);
// Sauvegarde le tableau de vols dans le fichier `chemin`; retourne 0 si succès
int sauvegarder_vols(const char *chemin, Vol *vols, int nb_vols);
#endif // VOL_H
