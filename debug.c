#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "192.168.100.53"
#define SERVER_PORT 9999
#define MESSAGE "---Start---{\"linear\":1.0,\"angular\":0.0}___END___"
#define RCVBUFSIZE 1000   /* Taille du tampon pour la réception du message */

void DieWithError(char *errorMessage);

int main() {
    int sockfd;
    struct sockaddr_in serverAddr;

    // Créer le socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        DieWithError("Erreur lors de la création du socket");
    }

    // Configurer l'adresse du serveur
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Se connecter au serveur
    if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        DieWithError("Erreur lors de la connexion au serveur");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    else{
        printf("good conn");
    }

    // Envoyer le message au serveur
    printf("Envoi du message au serveur : %s\n", MESSAGE);
    ssize_t bytesSent = send(sockfd, MESSAGE, strlen(MESSAGE), 0);
    if (bytesSent != strlen(MESSAGE)) {
        fprintf(stderr, "Erreur lors de l'envoi du message. %zd octets envoyés au lieu de %zu.\n", bytesSent, strlen(MESSAGE));
        DieWithError("Erreur lors de l'envoi du message");
    }
    else{
        printf("Message envoyé avec succès.\n");
    }

    // Réception du message du serveur
    char recvBuffer[RCVBUFSIZE];
    int recvMsgSize;

    // Attendre la réception du message
    printf("Attente de la réception du message du serveur...\n");
    if ((recvMsgSize = recv(sockfd, recvBuffer, RCVBUFSIZE - 1, 0)) < 0) {
        DieWithError("Erreur lors de la réception du message");
    }
    else{
        printf("good rec");
    }

    // Ajouter un caractère de fin de chaîne au buffer
    recvBuffer[recvMsgSize] = '\0';

    // Afficher le message reçu
    printf("Message reçu du serveur : %s\n", recvBuffer);

    // Fermer le socket
    close(sockfd);

    return 0;
}
