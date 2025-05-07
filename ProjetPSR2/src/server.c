// --- src/server.c ---
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "vol.h"
#include "histo.h"
#include "facture.h"
#define PORT 8055
#define BACKLOG 5
#define BUF_SIZE 512
// Cancellation penalty rate (10%)
static const double PENALTY_RATE = 0.10;
// Mutexes for thread-safe file I/O
static pthread_mutex_t vols_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t histo_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fact_mtx = PTHREAD_MUTEX_INITIALIZER;
// Check if transaction is duplicate
static int is_duplicate_transaction(int ref, int agence, const char *type, int valeur, Histo *hist, int nb_hist) {
    for (int i = 0; i < nb_hist; i++) {
        if (hist[i].reference == ref && hist[i].agence == agence &&
            strcmp(hist[i].transaction, type) == 0 && hist[i].valeur == valeur &&
            strcmp(hist[i].resultat, "succès") == 0) {
            return 1;
        }
    }
    return 0;
}
void *client_thread(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);
    char buf[BUF_SIZE];
    while (1) {
        // Receive command
        ssize_t n;
        size_t total = 0;
        buf[0] = '\0';
        while ((n = recv(client_fd, buf + total, BUF_SIZE - total - 1, 0)) > 0) {
            total += n;
            if (buf[total-1] == '\n') break;
        }
        if (n < 0) {
            perror("client_thread: recv");
            close(client_fd);
            return NULL;
        }
        if (n == 0) {
            close(client_fd);
            return NULL; // Client disconnected
        }
        buf[total] = '\0';
        if (total > 0 && buf[total-1] == '\n') buf[total-1] = '\0';
        // Handle AFF_VOL
        if (strncmp(buf, "AFF_VOL", 7) == 0) {
            pthread_mutex_lock(&vols_mtx);
            Vol *vols; int nb_vols;
            if (charger_vols("vols.txt", &vols, &nb_vols) != 0 || nb_vols == 0) {
                const char *err = "ERROR: cannot load vols.txt or no flights available\n";
                if (send(client_fd, err, strlen(err), 0) < 0) perror("AFF_VOL: send error");
                pthread_mutex_unlock(&vols_mtx);
                close(client_fd);
                return NULL;
            }
            char line[BUF_SIZE];
            snprintf(line, BUF_SIZE, "Référence Vol Destination Nombre Places Prix Place\n");
            if (send(client_fd, line, strlen(line), 0) < 0) {
                perror("AFF_VOL: send header");
                free(vols);
                pthread_mutex_unlock(&vols_mtx);
                close(client_fd);
                return NULL;
            }
            for (int i = 0; i < nb_vols; i++) {
                int len = snprintf(line, BUF_SIZE, "%d %s %d %.2f\n",
                                   vols[i].reference, vols[i].destination,
                                   vols[i].nombre_places, vols[i].prix_place);
                if (send(client_fd, line, len, 0) < 0) {
                    perror("AFF_VOL: send flight");
                    free(vols);
                    pthread_mutex_unlock(&vols_mtx);
                    close(client_fd);
                    return NULL;
                }
            }
            free(vols);
            pthread_mutex_unlock(&vols_mtx);
        }
        // Handle TRANSACTION
        else if (strncmp(buf, "TRANSACTION", 11) == 0) {
            int ref, agence, valeur;
            char type[16];
            if (sscanf(buf, "TRANSACTION %d %d %15s %d", &ref, &agence, type, &valeur) != 4) {
                const char *bad = "ERROR: bad TRANSACTION format\n";
                if (send(client_fd, bad, strlen(bad), 0) < 0) perror("TRANSACTION: send bad format");
                close(client_fd);
                return NULL;
            }
            // Check for duplicate transaction
            pthread_mutex_lock(&histo_mtx);
            Histo *hist; int nb_hist;
            if (charger_histo("histo.txt", &hist, &nb_hist) != 0) {
                const char *err = "ERROR: cannot load histo.txt\n";
                if (send(client_fd, err, strlen(err), 0) < 0) perror("TRANSACTION: send histo error");
                pthread_mutex_unlock(&histo_mtx);
                close(client_fd);
                return NULL;
            }
            if (is_duplicate_transaction(ref, agence, type, valeur, hist, nb_hist)) {
                const char *err = "ERROR: duplicate transaction\n";
                if (send(client_fd, err, strlen(err), 0) < 0) perror("TRANSACTION: send duplicate");
                free(hist);
                pthread_mutex_unlock(&histo_mtx);
                close(client_fd);
                return NULL;
            }
            free(hist);
            pthread_mutex_unlock(&histo_mtx);
            int ok = 0;
            float prix = 0;
            // Update vols
            pthread_mutex_lock(&vols_mtx);
            Vol *vols; int nb_vols;
            if (charger_vols("vols.txt", &vols, &nb_vols) != 0) {
                const char *err = "ERROR: cannot load vols.txt\n";
                if (send(client_fd, err, strlen(err), 0) < 0) perror("TRANSACTION: send vols error");
                pthread_mutex_unlock(&vols_mtx);
                close(client_fd);
                return NULL;
            }
            for (int i = 0; i < nb_vols; i++) {
                if (vols[i].reference == ref) {
                    prix = vols[i].prix_place;
                    if (strcmp(type, "Demande") == 0 && vols[i].nombre_places >= valeur) {
                        vols[i].nombre_places -= valeur;
                        ok = 1;
                    } else if (strcmp(type, "Annulation") == 0) {
                        vols[i].nombre_places += valeur;
                        ok = 1;
                    }
                    break;
                }
            }
            if (sauvegarder_vols("vols.txt", vols, nb_vols) != 0) {
                const char *err = "ERROR: cannot save vols.txt\n";
                if (send(client_fd, err, strlen(err), 0) < 0) perror("TRANSACTION: send save vols");
                free(vols);
                pthread_mutex_unlock(&vols_mtx);
                close(client_fd);
                return NULL;
            }
            free(vols);
            pthread_mutex_unlock(&vols_mtx);
            // Update history
            pthread_mutex_lock(&histo_mtx);
            if (charger_histo("histo.txt", &hist, &nb_hist) != 0) {
                const char *err = "ERROR: cannot load histo.txt\n";
                if (send(client_fd, err, strlen(err), 0) < 0) perror("TRANSACTION: send histo error");
                pthread_mutex_unlock(&histo_mtx);
                close(client_fd);
                return NULL;
            }
            Histo *new_hist = realloc(hist, (nb_hist + 1) * sizeof(Histo));
            if (!new_hist) {
                free(hist);
                const char *err = "ERROR: memory allocation failed\n";
                if (send(client_fd, err, strlen(err), 0) < 0) perror("TRANSACTION: send alloc error");
                pthread_mutex_unlock(&histo_mtx);
                close(client_fd);
                return NULL;
            }
            hist = new_hist;
            Histo new = { ref, agence, "", valeur, "" };
            strncpy(new.transaction, type, sizeof(new.transaction));
            new.transaction[sizeof(new.transaction)-1] = '\0';
            strcpy(new.resultat, ok ? "succès" : "impossible");
            hist[nb_hist++] = new;
            if (sauvegarder_histo("histo.txt", hist, nb_hist) != 0) {
                const char *err = "ERROR: cannot save histo.txt\n";
                if (send(client_fd, err, strlen(err), 0) < 0) perror("TRANSACTION: send save histo");
                free(hist);
                pthread_mutex_unlock(&histo_mtx);
                close(client_fd);
                return NULL;
            }
            free(hist);
            pthread_mutex_unlock(&histo_mtx);
            // Update invoice
            pthread_mutex_lock(&fact_mtx);
            Facture *fac; int nb_fac;
            if (charger_facture("facture.txt", &fac, &nb_fac) != 0) {
                const char *err = "ERROR: cannot load facture.txt\n";
                if (send(client_fd, err, strlen(err), 0) < 0) perror("TRANSACTION: send facture error");
                pthread_mutex_unlock(&fact_mtx);
                close(client_fd);
                return NULL;
            }
            int found = 0;
            for (int i = 0; i < nb_fac; i++) {
                if (fac[i].agence == agence) {
                    if (ok) {
                        if (strcmp(type, "Demande") == 0) {
                            fac[i].somme += valeur * prix;
                        } else {
                            fac[i].somme -= valeur * prix * (1.0 - PENALTY_RATE);
                        }
                    }
                    found = 1;
                    break;
                }
            }
            if (!found && ok) {
                double montant;
                if (strcmp(type, "Demande") == 0) {
                    montant = valeur * prix;
                } else {
                    montant = -valeur * prix * (1.0 - PENALTY_RATE);
                }
                Facture *new_fac = realloc(fac, (nb_fac + 1) * sizeof(Facture));
                if (!new_fac) {
                    free(fac);
                    const char *err = "ERROR: memory allocation failed\n";
                    if (send(client_fd, err, strlen(err), 0) < 0) perror("TRANSACTION: send alloc facture");
                    pthread_mutex_unlock(&fact_mtx);
                    close(client_fd);
                    return NULL;
                }
                fac = new_fac;
                fac[nb_fac++] = (Facture){ agence, montant };
            }
            if (sauvegarder_facture("facture.txt", fac, nb_fac) != 0) {
                const char *err = "ERROR: cannot save facture.txt\n";
                if (send(client_fd, err, strlen(err), 0) < 0) perror("TRANSACTION: send save facture");
                free(fac);
                pthread_mutex_unlock(&fact_mtx);
                close(client_fd);
                return NULL;
            }
            free(fac);
            pthread_mutex_unlock(&fact_mtx);
            // Reply
            const char *resp = ok ? "TRANSACTION OK\n" : "TRANSACTION IMPOSSIBLE\n";
            if (send(client_fd, resp, strlen(resp), 0) < 0) {
                perror("TRANSACTION: send response");
                close(client_fd);
                return NULL;
            }
        }
        // Handle AFF_HISTO
        else if (strncmp(buf, "AFF_HISTO", 9) == 0) {
            pthread_mutex_lock(&histo_mtx);
            Histo *hist; int nb_hist;
            if (charger_histo("histo.txt", &hist, &nb_hist) != 0) {
                const char *err = "ERROR: cannot load historique\n";
                if (send(client_fd, err, strlen(err), 0) < 0) perror("AFF_HISTO: send error");
                pthread_mutex_unlock(&histo_mtx);
                close(client_fd);
                return NULL;
            }
            char line[BUF_SIZE];
            snprintf(line, BUF_SIZE, "Référence Vol Agence Transaction Valeur Résultat\n");
            if (send(client_fd, line, strlen(line), 0) < 0) {
                perror("AFF_HISTO: send header");
                free(hist);
                pthread_mutex_unlock(&histo_mtx);
                close(client_fd);
                return NULL;
            }
            for (int i = 0; i < nb_hist; i++) {
                int len = snprintf(line, BUF_SIZE, "%d %d %s %d %s\n",
                                   hist[i].reference, hist[i].agence, hist[i].transaction,
                                   hist[i].valeur, hist[i].resultat);
                if (send(client_fd, line, len, 0) < 0) {
                    perror("AFF_HISTO: send history");
                    free(hist);
                    pthread_mutex_unlock(&histo_mtx);
                    close(client_fd);
                    return NULL;
                }
            }
            free(hist);
            pthread_mutex_unlock(&histo_mtx);
        }
        // Handle AFF_FACTURE
        else if (strncmp(buf, "AFF_FACTURE", 11) == 0) {
            int agence;
            if (sscanf(buf, "AFF_FACTURE %d", &agence) != 1) {
                const char *bad = "ERROR: bad AFF_FACTURE format\n";
                if (send(client_fd, bad, strlen(bad), 0) < 0) perror("AFF_FACTURE: send bad format");
                close(client_fd);
                return NULL;
            }
            pthread_mutex_lock(&fact_mtx);
            Facture *fac; int nb_fac;
            if (charger_facture("facture.txt", &fac, &nb_fac) != 0) {
                const char *err = "ERROR: cannot load facture.txt\n";
                if (send(client_fd, err, strlen(err), 0) < 0) perror("AFF_FACTURE: send error");
                pthread_mutex_unlock(&fact_mtx);
                close(client_fd);
                return NULL;
            }
            char line[BUF_SIZE];
            snprintf(line, BUF_SIZE, "Référence Agence Somme à payer\n");
            if (send(client_fd, line, strlen(line), 0) < 0) {
                perror("AFF_FACTURE: send header");
                free(fac);
                pthread_mutex_unlock(&fact_mtx);
                close(client_fd);
                return NULL;
            }
            for (int i = 0; i < nb_fac; i++) {
                if (fac[i].agence == agence) {
                    int len = snprintf(line, BUF_SIZE, "%d %.2f\n",
                                       fac[i].agence, fac[i].somme);
                    if (send(client_fd, line, len, 0) < 0) {
                        perror("AFF_FACTURE: send invoice");
                        free(fac);
                        pthread_mutex_unlock(&fact_mtx);
                        close(client_fd);
                        return NULL;
                    }
                    break;
                }
            }
            free(fac);
            pthread_mutex_unlock(&fact_mtx);
        }
        // Unknown command
        else {
            const char *unk = "UNKNOWN COMMAND\n";
            if (send(client_fd, unk, strlen(unk), 0) < 0) perror("UNKNOWN: send");
            close(client_fd);
            return NULL;
        }
    }
    close(client_fd);
    return NULL;
}
int main(void) {
    int srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0) {
        perror("socket");
        exit(1);
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    if (bind(srv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(srv_fd);
        exit(1);
    }
    if (listen(srv_fd, BACKLOG) < 0) {
        perror("listen");
        close(srv_fd);
        exit(1);
    }
    printf("Server listening on port %d\n", PORT);
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));
        if (!client_fd) {
            perror("malloc");
            continue;
        }
        *client_fd = accept(srv_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (*client_fd < 0) {
            perror("accept");
            free(client_fd);
            continue;
        }
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, client_fd) != 0) {
            perror("pthread_create");
            close(*client_fd);
            free(client_fd);
            continue;
        }
        pthread_detach(tid);
    }
    close(srv_fd);
    return 0;
}
