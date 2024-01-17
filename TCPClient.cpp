#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>

#define RCVBUFSIZE 10000
#define MAX_ENTRIES 20
#define MAX_NAME_LENGTH 50
#define MAX_VALUE_LENGTH 50
#define MAX_IP_LENGTH 16

int sock;                        /* Descripteur de socket */
struct sockaddr_in echoServAddr; /* Adresse du serveur Echo */
unsigned short echoServPort;     /* Port du serveur Echo */

int is_Test = 1;                   /* Choisissez entre 0 et 1 si vous voulez tester (0) ou utiliser le robot (1) */

char servIP[MAX_IP_LENGTH];

void initializeIP() {
    if (is_Test == 0) {
        snprintf(servIP, sizeof(servIP), "127.0.0.1");      /* IP du serveur pour le test */
    } else if (is_Test == 1) {
        snprintf(servIP, sizeof(servIP), "192.168.100.53");  /* IP du serveur du robot, changez X en fonction de votre turtlebot */
    }
}

const char *echoString = "connection working";                /* Chaîne à envoyer au serveur Echo */
const char *echoScan = "scan data";                /* Chaîne à envoyer au serveur Echo */ 
char *echoBuffer;               /* Tampon pour la chaîne Echo */
unsigned int echoStringLen;      /* Longueur de la chaîne à envoyer */
int bytesRcvd, totalBytesRcvd;   /* Octets lus dans un seul recv() et total d'octets lus */

void DieWithError(const char *errorMessage) {  /* Fonction de gestion des erreurs */
    perror(errorMessage);
    exit(EXIT_FAILURE);
}

sem_t *sem;  // Sémaphore pour la synchronisation

void connection(int port) {
    echoServPort = port;
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        DieWithError("socket() failed");
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);
    echoServAddr.sin_port = htons(echoServPort);

    if (connect(sock, reinterpret_cast<struct sockaddr *>(&echoServAddr), sizeof(echoServAddr)) < 0){
         DieWithError("connect() failed");
    }
}

void message(const char *argv) {
    echoStringLen = strlen(argv);
    if (send(sock, argv, echoStringLen, 0) != static_cast<ssize_t>(echoStringLen)){
        DieWithError("send() sent a different number of bytes than expected");
    }
}

char *receive() {
    char *receivedData = static_cast<char *>(malloc(RCVBUFSIZE * sizeof(char)));
    if (receivedData == nullptr) {
        DieWithError("Memory allocation failed");
    }

    totalBytesRcvd = 0;
    if (is_Test==0){
        while (totalBytesRcvd < echoStringLen) {
            if ((bytesRcvd = recv(sock, receivedData + totalBytesRcvd, RCVBUFSIZE - totalBytesRcvd - 1, 0)) <= 0) {
                DieWithError("recv() failed or connection closed prematurely");
            }
        totalBytesRcvd += bytesRcvd;
        }

        receivedData[totalBytesRcvd] = '\0';  // Ajout de la terminaison de la chaîne
    }

    if (is_Test==1){
        // Boucle pour recevoir les données en continu
        while (1) {
            if ((bytesRcvd = recv(sock, receivedData + totalBytesRcvd, RCVBUFSIZE - totalBytesRcvd - 1, 0)) <= 0) {
                // Si la connexion est fermée ou une erreur se produit, sortir de la boucle
                break;
            }
            totalBytesRcvd += bytesRcvd;
            receivedData[totalBytesRcvd] = '\0';
        }
    }

    return receivedData;
}

void init_shared_memory() {
    int fd = shm_open("/my_shared_memory", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    ftruncate(fd, RCVBUFSIZE);
    echoBuffer = static_cast<char *>(mmap(NULL, RCVBUFSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    close(fd);

    sem = sem_open("/my_semaphore", O_CREAT, S_IRUSR | S_IWUSR, 1);
    if (sem == SEM_FAILED) {
        DieWithError("sem_open() failed");
    }
}

void close_shared_memory() {
    munmap(echoBuffer, RCVBUFSIZE);
    sem_close(sem);
    shm_unlink("/my_shared_memory");
    sem_unlink("/my_semaphore");
}

char *scan() {
    connection(9997);
    if (is_Test==0){
    }

    if (is_Test==1){
        message(".");
    }

    char *receivedData = receive();

    // Writing to shared memory
    sem_wait(sem);
    strcpy(echoBuffer, receivedData);
    sem_post(sem);

    return receivedData;
}

char *odom() {
    connection(9998);

    if (is_Test==0){
    }

    if (is_Test==1){
        message(".");
    }

    char *receivedData = receive();

    // Writing to shared memory
    sem_wait(sem);
    strcpy(echoBuffer, receivedData);
    sem_post(sem);

    return receivedData;
}

void cmd_vel(float linear, float angular) {
    sem_wait(sem); // Attendre le sémaphore

    char command[100];
    snprintf(command, sizeof(command), "---START---{\"linear\":%f , \"angular\":%f}___END___", linear, angular);
    std::string formattedCommand = std::string(command);

    // Utiliser la chaîne formatée dans la suite de votre code
    if (send(sock, formattedCommand.c_str(), formattedCommand.length(), 0) != formattedCommand.length()) {
        sem_post(sem); // Libérer le sémaphore en cas d'erreur
        DieWithError("Erreur lors de l'envoi du message");
    }

    sem_post(sem); // Libérer le sémaphore après l'envoi du message
}

struct KeyValuePair {
    char key[50];
    float value;
};

void addToOdomData(KeyValuePair *odomData, int *numEntries, const char *dataType, float x, float y, float z, float w) {
    const char *desiredData[] = {"position_x", "position_y", "orientation_z", "linear_x", "angular_z"};

    for (int i = 0; i < sizeof(desiredData) / sizeof(desiredData[0]); ++i) {
        if (strcmp(dataType, desiredData[i]) == 0) {
            float valueToAdd = 0.0;

            if (i == 0) {
                valueToAdd = x;
            } else if (i == 1) {
                valueToAdd = y;
            } else if (i == 2) {
                valueToAdd = z;
            } else if (i == 3) {
                valueToAdd = x;  // Utilisez la valeur de 'x' pour "linear_x"
            } else if (i == 4) {
                valueToAdd = z;  // Utilisez la valeur de 'z' pour "angular_z"
            }

            odomData[*numEntries].value = valueToAdd;
            strcpy(odomData[*numEntries].key, dataType);
            (*numEntries)++;
        }
    }
}

void extractOdomData(const char *message, KeyValuePair *odomData, int *numEntries) {
    const char *startPtr = strstr(message, "---START---");
    const char *endPtr = strstr(message, "___END___");

    if (startPtr != nullptr && endPtr != nullptr) {
        const char *ptr = startPtr;

        while (ptr < endPtr) {
            ptr = strstr(ptr, "\"position\": {\"x\": ");
            if (ptr != nullptr) {
                float x, y, z;
                int result = sscanf(ptr, "\"position\": {\"x\": %f, \"y\": %f, \"z\": %f},", &x, &y, &z);
                if (result == 3) {
                    addToOdomData(odomData, numEntries, "position_x", x, y, z, 0.0);
                    addToOdomData(odomData, numEntries, "position_y", x, y, z, 0.0);
                }
            }
            ptr = strstr(ptr, "\"orientation\": {\"x\": ");
            if (ptr != nullptr) {
                float x, y, z, w;
                sscanf(ptr, "\"orientation\": {\"x\": %f, \"y\": %f, \"z\": %f, \"w\": %f}", &x, &y, &z, &w);
                addToOdomData(odomData, numEntries, "orientation_z", x, y, z, w);
            }

            ptr = strstr(ptr, "\"linear\": {\"x\": ");
            if (ptr != nullptr) {
                float x, y, z;
                sscanf(ptr, "\"linear\": {\"x\": %f, \"y\": %f, \"z\": %f}", &x, &y, &z);
                addToOdomData(odomData, numEntries, "linear_x", x, y, z, 0.0);
            }

            ptr = strstr(ptr, "\"angular\": {\"x\": ");
            if (ptr != nullptr) {
                float x, y, z;
                sscanf(ptr, "\"angular\": {\"x\": %f, \"y\": %f, \"z\": %f}", &x, &y, &z);
                addToOdomData(odomData, numEntries, "angular_z", x, y, z, 0.0);
            }

            ptr = strstr(ptr, "---START---");

            if (ptr == nullptr) {
                break;
            }
        }
    } else {
        std::cout << "Délimiteurs manquants dans le message." << std::endl;
    }
}

void processOdomData(KeyValuePair *odomData, int *numEntries) {
    const char *message = odom();
    std::cout << "Données extraites :" << std::endl;

    while (message != nullptr) {
        const char *startPtr = strstr(message, "---START---");

        if (startPtr != nullptr) {
            const char *endPtr = strstr(startPtr, "___END___");

            if (endPtr != nullptr) {
                *numEntries = 0;
                extractOdomData(startPtr, odomData, numEntries);

                std::cout << std::endl;
                for (int i = 0; i < *numEntries; ++i) {
                    std::cout << odomData[i].key << ": " << odomData[i].value << std::endl;
                }

                message = endPtr + strlen("___END___");
            } else {
                std::cout << "Délimiteur '___END___' manquant dans le message." << std::endl;
                break;
            }
        } else {
            std::cout << "Délimiteur '---START---' manquant dans le message." << std::endl;
            break;
        }
    }
}

struct RangeData {
    float *ranges;
    int numRanges;
};

void extractRangeData(const char *message, RangeData *rangeData) {
    const char *startPtr = strstr(message, "ranges\": [");
    const char *endPtr = strstr(message, "], \"intensities\"");

    if (startPtr != nullptr && endPtr != nullptr) {
        size_t startLen = strlen("ranges\": [");
        startPtr += startLen;

        size_t endLen = endPtr - startPtr;

        char *rangesStr = static_cast<char *>(malloc(endLen + 1));
        strncpy(rangesStr, startPtr, endLen);
        rangesStr[endLen] = '\0';

        float *ranges = nullptr;
        int numRanges = 0;
        char *token = strtok(rangesStr, ",");
        while (token != nullptr) {
            float value = atof(token);
            ranges = static_cast<float *>(realloc(ranges, (numRanges + 1) * sizeof(float)));
            ranges[numRanges++] = value;
            token = strtok(nullptr, ",");
        }

        rangeData->ranges = ranges;
        rangeData->numRanges = numRanges;

        free(rangesStr);
    } else {
        std::cout << "Délimiteurs manquants dans le message." << std::endl;
    }
}

void addRanges(KeyValuePair *odomData, int *numEntries, const RangeData *rangeData) {
    int degree = 10;
    for (int i = 0; i < rangeData->numRanges; ++i) {
        if ((i % degree) == 0) {
            char key[20];
            sprintf(key, "%d°", i);
            odomData[*numEntries].value = rangeData->ranges[i];
            strcpy(odomData[*numEntries].key, key);
            (*numEntries)++;
        }
    }
}

void processScanData(KeyValuePair *scanData, int *numEntries) {
    RangeData rangeData;

    const char *message = scan();
    std::cout << "Extracted Scan Data:" << std::endl;

    while (message != nullptr) {
        const char *startPtr = strstr(message, "---START---");

        if (startPtr != nullptr) {
            const char *endPtr = strstr(startPtr, "___END___");

            if (endPtr != nullptr) {
                *numEntries = 0;
                extractRangeData(startPtr, &rangeData);
                addRanges(scanData, numEntries, &rangeData);

                std::cout << std::endl;
                for (int i = 0; i < *numEntries; ++i) {
                    std::cout << scanData[i].key << ": " << scanData[i].value << std::endl;
                }

                message = endPtr + strlen("___END___");
            } else {
                std::cout << "Délimiteur '___END___' manquant dans le message." << std::endl;
                break;
            }
        } else {
            std::cout << "Délimiteur '---START---' manquant dans le message." << std::endl;
            break;
        }
    }
}

void moveRobot_linear(double distance, int *isFinished) {
    double totalDistance = 0.0;
    double speed = 0.3;

    cmd_vel(speed, 0);
    sem_wait(sem);
    std::cout << echoBuffer << std::endl;
    sem_post(sem);

    while (totalDistance < distance) {

        totalDistance += speed * 0.1; // 0.1 est le temps de chaque itération

        if (totalDistance >= distance) {
            // Arrêter le robot (envoyer une commande d'arrêt)
            cmd_vel(0, 0);
            std::cout << "Stop: ";
            receive();
            sem_wait(sem);
            std::cout << echoBuffer << std::endl;
            sem_post(sem);

            // Mettre à jour le booléen pour indiquer que la fonction est terminée
            *isFinished = 1;
            break;
        }
    }
}

void moveRobot_circular(double radius, int *isFinished) {
    double speed = 1.0; // Ajuster la vitesse au besoin
    double angularSpeed = speed / radius; // Calculer la vitesse angulaire en fonction du rayon

    double circumference = 2.0 * 3.14159 * radius; // Calculer la circonférence du cercle

    // Déplacer le robot avec la vitesse angulaire calculée pour le faire suivre un chemin circulaire
    cmd_vel(speed, angularSpeed);
    receive();
    sem_wait(sem);
    std::cout << echoBuffer << std::endl;
    sem_post(sem);

    double distanceTravelled = 0.0;

    // Simuler le mouvement du robot en cercle jusqu'à ce qu'il effectue une rotation complète
    while (distanceTravelled < circumference) {
        usleep(100000); // Attendre 0.1 seconde (ajuster au besoin pour le robot et l'environnement)

        // Calculer la distance parcourue par le robot sur un chemin circulaire
        distanceTravelled += speed * 0.1; // 0.1 est le temps de chaque itération

        // Arrêter le robot lorsqu'il effectue une rotation complète (une circonférence)
        if (distanceTravelled >= circumference) {
            // Envoyer une commande d'arrêt au robot
            cmd_vel(0, 0);
            std::cout << "Stop: ";
            receive();
            sem_wait(sem);
            std::cout << echoBuffer << std::endl;
            sem_post(sem);

            // Mettre à jour le booléen pour indiquer que la fonction est terminée
            *isFinished = 1;

            break;
        }
    }
}

void move_path() {
    KeyValuePair odomData[MAX_ENTRIES];
    int odomnumEntries = 0;

    //For the robot to know at which step he is
    int step = 0;
    int isFinished = 0;

    processOdomData(odomData, &odomnumEntries);

    KeyValuePair scanData[360];
    int scannumEntries = 0;

    processScanData(scanData, &scannumEntries);

    // distance between the robot and the circle
    float app = 0.5;

    double dist;
    std::cout << "Enter linear distance: ";
    std::cin >> dist;

    double rad;
    std::cout << "Enter the radius: ";
    std::cin >> rad;

    std::cout << "Begin of the following path\n";

    connection(9999); // Establishing a connection with the robot

    //For the robot
    if (is_Test == 0) {
        if (step == 0) {
            moveRobot_linear(dist, &isFinished);
            if ((odomData[0].value == dist) && (odomData[1].value == 0) && (odomData[2].value == 0) && (isFinished == 1)) {
                step = 1;
                isFinished = 0;
            }

            if ((odomData[0].value == dist) && (odomData[1].value == 0) && (odomData[2].value != 0) && (isFinished == 1)) {
                cmd_vel(0, 0.5);
            }

            if ((odomData[0].value == dist) && (odomData[1].value != 0) && (isFinished == 1)) {
                cmd_vel(0, 0.5);
            }
        }

        if (step == 1) {
            if ((scanData[4].value == app) && (isFinished == 0)) {
                moveRobot_circular(rad + app, &isFinished);
            }
            if ((isFinished == 1) && (odomData[2].value == -180)) {
                step = 2;
                isFinished = 0;
            } else {
                cmd_vel(0, 0.5);
            }
        }

        if (step == 2) {
            moveRobot_linear(dist, &isFinished);
            if ((odomData[0].value == 0) && (odomData[1].value == 0) && (odomData[2].value == 0)) {
                step = 3;
            }
        }
        if (step == 3) {
            std::cout << "End of the following path\n";
        }
    }

    //For test
    if (is_Test == 1) {
        if (step == 0) {
            moveRobot_linear(dist, &isFinished);
            if (isFinished == 1) {
                step = 1;
                isFinished = 0;
            }
        }
        if (step == 1) {
            moveRobot_circular(rad + app, &isFinished);
            if (isFinished == 1) {
                step = 2;
                isFinished = 0;
            }
        }
        if (step == 2) {
            moveRobot_linear(dist, &isFinished);
            if (isFinished == 1) {
                step = 3;
            }
        }
        if (step == 3) {
            std::cout << "End of the following path\n";
        }
    }
}

int main() {
    initializeIP();
    std::cout << "Server IP: " << servIP << std::endl;

    init_shared_memory();

    // Uncommentary it to see the message received from the odom
    // char *odomResult = odom();
    // std::cout << "Odom Result: " << odomResult << std::endl;
    // free(odomResult);

    // Uncommentary it to see the message received from the scan
    // char *scanResult = scan();
    // std::cout << "Scan Result: " << scanResult << std::endl;
    // free(scanResult);

    // Uncomment the line below if you want to test the filter of the scan
    // KeyValuePair scanData[360];
    // int scannumEntries = 0;
    // processScanData(scanData, &scannumEntries);

    // Uncomment the line below if you want to test the filter of the odom
    // KeyValuePair odomData[MAX_ENTRIES];
    // int odomnumEntries = 0;
    // processOdomData(odomData, &odomnumEntries);

    // Uncomment the line below if you want to test the moving part, only working with test
    // if (is_Test == 0) {
    //     move_path();
    // }

    // Uncomment the line below if you want to test the moving part commander
    connection(9999);
    // cmd_vel(0.1, 0.0);

    // sem_wait(sem);
    // std::cout << "Received message: " << echoBuffer << std::endl;
    // sem_post(sem);
    int isFinished = 0;
    // moveRobot_linear(1,&isFinished);


    close_shared_memory();
    close(sock);
    exit(0);
}


