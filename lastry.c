#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>


#define RCVBUFSIZE 10000
#define MAX_ENTRIES 20
#define MAX_NAME_LENGTH 50
#define MAX_VALUE_LENGTH 50
#define MAX_IP_LENGTH 16 

int sock;                        /* Socket descriptor */
struct sockaddr_in echoServAddr; /* Echo server address */
unsigned short echoServPort ;     /* Echo server port */

int is_Test = 0;                   /* Choose between 0 and 1 if you want to test(0) or use the robot(1) */

char servIP[MAX_IP_LENGTH];

void initializeIP() {
    if (is_Test == 0) {
        snprintf(servIP, sizeof(servIP), "127.0.0.1");      /* Server IP for the Test */
    } else if (is_Test == 1) {
        snprintf(servIP, sizeof(servIP), "192.168.100.5X");  /* Server IP of the robot change X in function of your turtlebot */
    }
}

// char *servIP = IPserv(is_Test);                    /* Server IP address for test  */
// char *servIP = "192.168.100.5X";                    /* Server IP address turtlebot(change X) */
char *echoString = "connection working";                /* String to send to echo server */
char *echoScan = "scan data";                /* String to send to echo server */ 
char *echoBuffer;               /* Buffer for echo string */
unsigned int echoStringLen;      /* Length of string to echo */
int bytesRcvd, totalBytesRcvd;   /* Bytes read in single recv() and total bytes read */

void DieWithError(char *errorMessage);  /* Error handling function */

sem_t *sem;  // Semaphore for synchronization

void DieWithError(char *errorMessage);

void connection(int port) {
    echoServPort = port;
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);
    echoServAddr.sin_port = htons(echoServPort);

    if (connect(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("connect() failed");
}

void message(char *argv) {
    echoStringLen = strlen(argv);
    if (send(sock, argv, echoStringLen, 0) != echoStringLen)
        DieWithError("send() sent a different number of bytes than expected");
}

char *receive() {
    char *receivedData = (char *) malloc(RCVBUFSIZE * sizeof(char));
    if (receivedData == NULL) {
        DieWithError("Memory allocation failed");
    }

    totalBytesRcvd = 0;
    while (totalBytesRcvd < echoStringLen) {
        if ((bytesRcvd = recv(sock, receivedData + totalBytesRcvd, RCVBUFSIZE - totalBytesRcvd - 1, 0)) <= 0) {
            DieWithError("recv() failed or connection closed prematurely");
        }
        totalBytesRcvd += bytesRcvd;
    }

    receivedData[totalBytesRcvd] = '\0';  // Ajout de la terminaison de la chaîne

    return receivedData;
}

void init_shared_memory() {
    int fd = shm_open("/my_shared_memory", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    ftruncate(fd, RCVBUFSIZE);
    echoBuffer = (char *) mmap(NULL, RCVBUFSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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
        message("---START---{\"header\": {\"seq\": 20550, \"stamp\": {\"secs\": 1677514522, \"nsecs\": 217517053}, \"frame_id\": \"base_scan\"}, \"angle_min\": 0.0, \"angle_max\": 6.2657318115234375, \"angle_increment\": 0.01745329238474369, \"time_increment\": 0.0005592841189354658, \"scan_time\": 0.20134228467941284, \"range_min\": 0.11999999731779099, \"range_max\": 3.5, \"ranges\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.3619999885559082, 0.3619999885559082, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.318000078201294, 2.249000072479248, 0.8429999947547913, 2.4260001182556152, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.8250000476837158, 0.0, 0.0, 2.6989998817443848, 0.0, 4.14300012588501, 3.8970000743865967, 3.4739999771118164, 4.199999809265137, 4.1579999923706055, 0.0, 4.197999954223633, 0.0, 4.165999889373779, 4.189000129699707, 4.164000034332275, 4.184000015258789, 4.184000015258789, 0.0, 0.0, 4.158999919891357, 0.0, 0.0, 0.503000020980835, 0.5139999985694885, 0.5260000228881836, 0.5180000066757202, 0.5139999985694885, 0.5149999856948853, 0.5199999809265137, 0.527999997138977, 0.6759999990463257, 0.6779999732971191, 0.6809999942779541, 0.6729999780654907, 0.675000011920929, 0.6679999828338623, 0.0, 0.6470000147819519, 0.6359999775886536, 0.6259999871253967, 0.6129999756813049, 0.6010000109672546, 0.5889999866485596, 0.5899999737739563, 0.5849999785423279, 0.5759999752044678, 0.5640000104904175, 0.5509999990463257, 0.5479999780654907, 0.5419999957084656, 0.5350000262260437, 0.5260000228881836, 0.5180000066757202, 0.49799999594688416, 0.4869999885559082, 0.4909999966621399, 0.4909999966621399, 0.48500001430511475, 0.4790000021457672, 0.4779999852180481, 0.47999998927116394, 0.4729999899864197, 0.4749999940395355, 0.47699999809265137, 0.4790000021457672, 0.4819999933242798, 0.4830000102519989, 0.48500001430511475, 0.4869999885559082, 0.4959999918937683, 0.5049999952316284, 0.5559999942779541, 1.534999966621399, 1.465000033378601, 1.4709999561309814, 1.1770000457763672, 1.1610000133514404, 1.1579999923706055, 1.1360000371932983, 1.1380000114440918, 1.1319999694824219, 1.13100004196167, 1.1100000143051147, 1.0839999914169312, 1.090999960899353, 1.0950000286102295, 1.0759999752044678, 1.0779999494552612, 1.062000036239624, 1.069000005722046, 1.0809999704360962, 1.0700000524520874, 1.062000036239624, 1.0549999475479126, 1.0709999799728394, 1.0470000505447388, 1.0520000457763672, 0.9459999799728394, 1.315999984741211, 1.2929999828338623, 1.315999984741211, 1.3240000009536743, 1.315000057220459, 1.315000057220459, 1.3240000009536743, 1.2990000247955322, 1.3040000200271606, 1.309999942779541, 1.3380000591278076, 1.3220000267028809, 1.3259999752044678, 1.3250000476837158, 1.3339999914169312, 1.3489999771118164, 1.3289999961853027, 1.36899995803833, 1.3589999675750732, 1.3660000562667847, 1.3639999628067017, 1.3609999418258667, 1.3960000276565552, 1.3880000114440918, 1.3980000019073486, 1.4160000085830688, 1.4140000343322754, 1.4160000085830688, 1.440000057220459, 1.4459999799728394, 1.465999960899353, 1.4730000495910645, 1.5019999742507935, 1.5080000162124634, 1.5140000581741333, 1.5470000505447388, 1.5499999523162842, 1.5750000476837158, 1.593999981880188, 1.6139999628067017, 1.6360000371932983, 1.6640000343322754, 1.6670000553131104, 1.7100000381469727, 1.746000051498413, 1.7669999599456787, 1.7890000343322754, 1.812999963760376, 1.8389999866485596, 1.8700000047683716, 1.8880000114440918, 1.9320000410079956, 1.9550000429153442, 1.9839999675750732, 2.055999994277954, 2.0840001106262207, 2.127000093460083, 2.197999954223633, 2.2360000610351562, 2.302000045776367, 2.368000030517578, 2.428999900817871, 2.502000093460083, 2.5850000381469727, 2.747999906539917, 2.8239998817443848, 2.9240000247955322, 2.9809999465942383, 3.111999988555908, 3.2279999256134033, 3.38100004196167, 3.490999937057495, 3.615999937057495, 3.819000005722046, 3.9019999504089355, 0.0, 4.065000057220459, 4.184999942779541, 0.0, 2.382999897003174, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 3.5899999141693115, 3.5280001163482666, 3.556999921798706, 3.4200000762939453, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], \"intensities\": [0.0, 0.0, 170.0, 170.0, 170.0, 0.0, 0.0, 170.0, 170.0, 0.0, 0.0, 0.0, 36.0, 105.0, 52.0, 78.0, 96.0, 0.0, 0.0, 0.0, 0.0, 0.0, 72.0, 80.0, 106.0, 0.0, 96.0, 47.0, 442.0, 104.0, 116.0, 117.0, 121.0, 138.0, 126.0, 93.0, 85.0, 62.0, 0.0, 61.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 105.0, 170.0, 105.0, 69.0, 0.0, 105.0, 150.0, 0.0, 0.0, 170.0, 170.0, 0.0, 0.0, 0.0, 170.0, 0.0, 0.0, 0.0, 0.0, 170.0, 0.0, 0.0, 0.0, 105.0, 0.0, 0.0, 0.0, 170.0, 0.0, 170.0, 0.0, 0.0, 170.0, 0.0, 0.0, 0.0, 60.0, 81.0, 2840.0, 1263.0, 79.0, 132.0, 150.0, 174.0, 343.0, 511.0, 518.0, 396.0, 397.0, 353.0, 488.0, 240.0, 351.0, 2787.0, 103.0, 170.0, 0.0, 0.0, 0.0, 130.0, 0.0, 456.0, 0.0, 0.0, 538.0, 0.0, 289.0, 185.0, 1313.0, 451.0, 567.0, 606.0, 718.0, 844.0, 950.0, 661.0, 572.0, 551.0, 525.0, 483.0, 486.0, 467.0, 450.0, 434.0, 1096.0, 3619.0, 3318.0, 3578.0, 3940.0, 3868.0, 3606.0, 4910.0, 940.0, 2799.0, 2496.0, 2996.0, 2364.0, 2618.0, 2377.0, 2619.0, 2750.0, 4141.0, 2716.0, 2313.0, 3475.0, 3649.0, 2878.0, 3614.0, 2659.0, 3078.0, 3007.0, 2897.0, 3302.0, 3693.0, 3723.0, 3330.0, 3310.0, 3681.0, 3162.0, 2888.0, 3113.0, 4428.0, 3643.0, 3486.0, 3498.0, 3608.0, 3829.0, 3731.0, 3786.0, 2977.0, 4043.0, 3736.0, 6391.0, 297.0, 1067.0, 2049.0, 2455.0, 1635.0, 1829.0, 1922.0, 2058.0, 2179.0, 1959.0, 2057.0, 1912.0, 2066.0, 2108.0, 2123.0, 2181.0, 2203.0, 2274.0, 2331.0, 2351.0, 2509.0, 2759.0, 2766.0, 2316.0, 2124.0, 2615.0, 3379.0, 1456.0, 1629.0, 1694.0, 1648.0, 1520.0, 1805.0, 1664.0, 1711.0, 1694.0, 1622.0, 1651.0, 1317.0, 1560.0, 2154.0, 1582.0, 1555.0, 1505.0, 1612.0, 1671.0, 1610.0, 1562.0, 1742.0, 1801.0, 1903.0, 1817.0, 1779.0, 1730.0, 2094.0, 1825.0, 1900.0, 2122.0, 2037.0, 2149.0, 2101.0, 2001.0, 1911.0, 1804.0, 1697.0, 1738.0, 1593.0, 1481.0, 1550.0, 1297.0, 1196.0, 1231.0, 1305.0, 1212.0, 837.0, 1040.0, 960.0, 907.0, 860.0, 817.0, 756.0, 779.0, 716.0, 640.0, 562.0, 497.0, 441.0, 396.0, 371.0, 356.0, 330.0, 250.0, 256.0, 266.0, 289.0, 279.0, 292.0, 281.0, 271.0, 245.0, 229.0, 266.0, 2208.0, 258.0, 243.0, 244.0, 2038.0, 78.0, 567.0, 162.0, 46.0, 118.0, 88.0, 154.0, 98.0, 98.0, 0.0, 0.0, 0.0, 154.0, 98.0, 98.0, 99.0, 267.0, 334.0, 295.0, 280.0, 67.0, 0.0, 154.0, 154.0, 0.0, 0.0, 154.0, 65.0, 98.0, 101.0, 154.0, 0.0, 0.0, 0.0, 92.0, 0.0, 0.0, 0.0, 154.0, 0.0, 0.0, 0.0, 0.0, 154.0, 0.0, 0.0, 0.0, 0.0, 0.0, 109.0, 0.0, 154.0, 154.0, 0.0, 0.0, 0.0, 0.0, 154.0, 0.0, 154.0, 0.0, 0.0, 154.0, 0.0, 0.0, 98.0, 154.0, 0.0, 0.0, 0.0, 98.0, 0.0, 98.0]}___END___"); 
    }

    if (is_Test==1){
        message("");
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
        // message("---START---{\"header\": {\"seq\": 96997, \"stamp\": {\"secs\": 1677514236, \"nsecs\": 463284111}, \"frame_id\": \"odom\"}, \"child_frame_id\": \"base_footprint\", \"pose\": {\"pose\": {\"position\": {\"x\": 0.23223192989826202, \"y\": 0.08237219601869583, \"z\": 0.0}, \"orientation\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.965720534324646, \"w\": 0.2595840394496918}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}, \"twist\": {\"twist\": {\"linear\": {\"x\": -0.0007909589330665767, \"y\": 0.0, \"z\": 0.0}, \"angular\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.0019147992134094238}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}}___END___"); // just for the test
        message("---START---{\"header\": {\"seq\": 96997, \"stamp\": {\"secs\": 1677514236, \"nsecs\": 463284111}, \"frame_id\": \"odom\"}, \"child_frame_id\": \"base_footprint\", \"pose\": {\"pose\": {\"position\": {\"x\": 0.45223192989826202, \"y\": 0.86337219601869583, \"z\": 0.0}, \"orientation\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.965720534324646, \"w\": 0.2595840394496918}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}, \"twist\": {\"twist\": {\"linear\": {\"x\": -0.0007909589330665767, \"y\": 0.0, \"z\": 0.0}, \"angular\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.0019147992134094238}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}}___END___---START---{\"header\": {\"seq\": 96997, \"stamp\": {\"secs\": 1677514236, \"nsecs\": 463284111}, \"frame_id\": \"odom\"}, \"child_frame_id\": \"base_footprint\", \"pose\": {\"pose\": {\"position\": {\"x\": 0.73223192989826202, \"y\": 0.28237219601869583, \"z\": 0.0}, \"orientation\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.965720534324646, \"w\": 0.2595840394496918}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}, \"twist\": {\"twist\": {\"linear\": {\"x\": -0.0007909589330665767, \"y\": 0.0, \"z\": 0.0}, \"angular\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.0019147992134094238}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}}___END___---START---{\"header\": {\"seq\": 96997, \"stamp\": {\"secs\": 1677514236, \"nsecs\": 463284111}, \"frame_id\": \"odom\"}, \"child_frame_id\": \"base_footprint\", \"pose\": {\"pose\": {\"position\": {\"x\": 0.23223192989826202, \"y\": 0.08237219601869583, \"z\": 0.0}, \"orientation\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.965720534324646, \"w\": 0.2595840394496918}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}, \"twist\": {\"twist\": {\"linear\": {\"x\": -0.0007909589330665767, \"y\": 0.0, \"z\": 0.0}, \"angular\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.0019147992134094238}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}}___END___---START---{\"header\": {\"seq\": 96997, \"stamp\": {\"secs\": 1677514236, \"nsecs\": 463284111}, \"frame_id\": \"odom\"}, \"child_frame_id\": \"base_footprint\", \"pose\": {\"pose\": {\"position\": {\"x\": 0.93489728978191290, \"y\": 0.24543672618774239, \"z\": 0.0}, \"orientation\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.205720534324646, \"w\": 0.2595840394496918}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}, \"twist\": {\"twist\": {\"linear\": {\"x\": -0.0007909589330665767, \"y\": 0.0, \"z\": 0.0}, \"angular\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.0019147992134094238}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}}___END___"); // just for the test
    }

    if (is_Test==1){
        message("");
    }

    char *receivedData = receive();

    // Writing to shared memory
    sem_wait(sem);
    strcpy(echoBuffer, receivedData);
    sem_post(sem);

    return receivedData;
}

void cmd_vel(float linear, float angular) {
    // connection(9999);  // Vous pouvez commenter cette ligne car la connexion est déjà établie dans votre programme

    // Utilisez la mémoire partagée au lieu de la connexion réseau
    sem_wait(sem);
    char command[100]; // Création d'un tableau de caractères pour stocker la commande
    // Formatage de la commande avec les valeurs linéaires et angulaires
    snprintf(command, sizeof(command), "---Start---{\"linear\":%f , \"angular\":%f}___END___", linear, angular);
    message(command);

    // Écriture dans la mémoire partagée
    strcpy(echoBuffer, command);
    sem_post(sem);
}

struct KeyValuePair {
    char key[50];
    float value;
};

void addToOdomData(struct KeyValuePair *odomData, int *numEntries, const char *dataType, float x, float y, float z, float w) {
    // Liste des données spécifiques que vous souhaitez inclure
    const char *desiredData[] = {"position_x", "position_y", "orientation_z", "linear_x", "angular_z"};

    for (int i = 0; i < sizeof(desiredData) / sizeof(desiredData[0]); ++i) {
        if (strcmp(dataType, desiredData[i]) == 0) {
            // Ajouter seulement les données spécifiques
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

void extractOdomData(const char *message, struct KeyValuePair *odomData, int *numEntries) {
    const char *startPtr = strstr(message, "---START---");
    const char *endPtr = strstr(message, "___END___");

    // Vérifier si les délimiteurs de début et de fin sont présents
    if (startPtr != NULL && endPtr != NULL) {
        const char *ptr = startPtr;

        // Parcourir le message à partir de l'occurrence de "---START---" jusqu'à "___END___"
        while (ptr < endPtr) {
            // Vos opérations d'extraction de données ici

            // Vérifier si l'occurrence de "\"position\": {\"x\": " est présente
            ptr = strstr(ptr, "\"position\": {\"x\": ");
            if (ptr != NULL) {
                float x, y, z;
                // Utilisation de sscanf pour extraire les valeurs
                int result = sscanf(ptr, "\"position\": {\"x\": %f, \"y\": %f, \"z\": %f},", &x, &y, &z);
                if (result == 3) {
                    // Utilisation de addToOdomData pour ajouter les données à la liste
                    addToOdomData(odomData, numEntries, "position_x", x, y, z, 0.0);
                    addToOdomData(odomData, numEntries, "position_y", x, y, z, 0.0);
                }
            }
            ptr = strstr(ptr, "\"orientation\": {\"x\": ");
            if (ptr != NULL) {
                float x, y, z, w;
                sscanf(ptr, "\"orientation\": {\"x\": %f, \"y\": %f, \"z\": %f, \"w\": %f}", &x, &y, &z, &w);
                addToOdomData(odomData, numEntries, "orientation_z", x, y, z, w);
            }

            ptr = strstr(ptr, "\"linear\": {\"x\": ");
            if (ptr != NULL) {
                float x, y, z;
                sscanf(ptr, "\"linear\": {\"x\": %f, \"y\": %f, \"z\": %f}", &x, &y, &z);
                addToOdomData(odomData, numEntries, "linear_x", x, y, z, 0.0);
            }

            ptr = strstr(ptr, "\"angular\": {\"x\": ");
            if (ptr != NULL) {
                float x, y, z;
                sscanf(ptr, "\"angular\": {\"x\": %f, \"y\": %f, \"z\": %f}", &x, &y, &z);
                addToOdomData(odomData, numEntries, "angular_z", x, y, z, 0.0);
            }

            // Passer à l'occurrence suivante de "---START---"
            ptr = strstr(ptr, "---START---");

            // Vérifier si l'occurrence de "---START---" est présente pour éviter une boucle infinie
            if (ptr == NULL) {
                break;
            }
        }
    } else {
        // Les délimiteurs de début ou de fin sont manquants
        // Gérer cette condition selon vos besoins
        printf("Délimiteurs manquants dans le message.\n");
    }
}

void processOdomData(struct KeyValuePair *odomData, int *numEntries) {
    const char *message = odom();
    printf("Extracted Data:\n");

    while (message != NULL) {
        const char *startPtr = strstr(message, "---START---");

        if (startPtr != NULL) {
            const char *endPtr = strstr(startPtr, "___END___");

            if (endPtr != NULL) {
                // Réinitialiser la structure odomData
                *numEntries = 0;

                // Appeler votre fonction pour extraire les données
                extractOdomData(startPtr, odomData, numEntries);

                // Afficher les données extraites
                printf("\n");
                for (int i = 0; i < *numEntries; ++i) {
                    printf("%s: %f\n", odomData[i].key, odomData[i].value);
                }

                // Mettre à jour le pointeur pour la prochaine itération
                message = endPtr + strlen("___END___");
            } else {
                printf("Délimiteur '___END___' manquant dans le message.\n");
                break;
            }
        } else {
            printf("Délimiteur '---START---' manquant dans le message.\n");
            break;
        }
    }
}

struct RangeData {
    float *ranges;
    int numRanges;
};

void extractRangeData(const char *message, struct RangeData *rangeData) {
    const char *startPtr = strstr(message, "ranges\": [");
    const char *endPtr = strstr(message, "], \"intensities\"");

    // Vérifier si les délimiteurs de début et de fin sont présents
    if (startPtr != NULL && endPtr != NULL) {
        // Calculer la longueur de la sous-chaîne "ranges": ["
        size_t startLen = strlen("ranges\": [");

        // Avancer le pointeur de début au début de la liste
        startPtr += startLen;

        // Calculer la longueur de la sous-chaîne "], \"intensities\""
        size_t endLen = endPtr - startPtr;

        // Allouer de la mémoire pour stocker la sous-chaîne "ranges"
        char *rangesStr = (char *)malloc(endLen + 1);
        strncpy(rangesStr, startPtr, endLen);
        rangesStr[endLen] = '\0';

        // Utiliser sscanf pour extraire les valeurs de la sous-chaîne "ranges"
        float *ranges = NULL;
        int numRanges = 0;
        char *token = strtok(rangesStr, ",");
        while (token != NULL) {
            float value = atof(token);
            ranges = (float *)realloc(ranges, (numRanges + 1) * sizeof(float));
            ranges[numRanges++] = value;
            token = strtok(NULL, ",");
        }

        // Stocker les données extraites dans la structure RangeData
        rangeData->ranges = ranges;
        rangeData->numRanges = numRanges;

        // Libérer la mémoire allouée
        free(rangesStr);
    } else {
        // Les délimiteurs de début ou de fin sont manquants
        // Gérer cette condition selon vos besoins
        printf("Délimiteurs manquants dans le message.\n");
    }
}

void addRanges(struct KeyValuePair *odomData, int *numEntries, const struct RangeData *rangeData) {
    // Ajouter seulement les données toutes les 5 itérations
    int degree =10;
    for (int i = 0; i < rangeData->numRanges; ++i) {
        if ((i % degree) == 0) {
            char key[20];
            sprintf(key, "%d°", i);  // Créer une clé unique pour chaque plage
            odomData[*numEntries].value = rangeData->ranges[i];
            strcpy(odomData[*numEntries].key, key);
            (*numEntries)++;
        }
    }
}

void processScanData(struct KeyValuePair *scanData, int *numEntries) {
    struct RangeData rangeData;

    const char *message = scan();
    printf("Extracted Scan Data:\n");

    while (message != NULL) {
        const char *startPtr = strstr(message, "---START---");

        if (startPtr != NULL) {
            const char *endPtr = strstr(startPtr, "___END___");

            if (endPtr != NULL) {
                // Réinitialiser la structure scanData
                *numEntries = 0;

                // Appeler votre fonction pour extraire les données de scan
                extractRangeData(startPtr, &rangeData);

                // Appeler votre fonction pour ajouter les données toutes les 10 itérations
                addRanges(scanData, numEntries, &rangeData);

                // Afficher les données extraites
                printf("\n");
                for (int i = 0; i < *numEntries; ++i) {
                    printf("%s: %f\n", scanData[i].key, scanData[i].value);
                }

                // Mettre à jour le pointeur pour la prochaine itération
                message = endPtr + strlen("___END___");
            } else {
                printf("Délimiteur '___END___' manquant dans le message.\n");
                break;
            }
        } else {
            printf("Délimiteur '---START---' manquant dans le message.\n");
            break;
        }
    }
}

void moveRobot_linear(double distance, int *isFinished) {
    double totalDistance = 0.0;
    double speed = 1.0; 

    cmd_vel(speed, 0); 
    receive();    
    sem_wait(sem);
    printf("%s\n", echoBuffer);
    sem_post(sem);

    while (totalDistance < distance) {
        usleep(100000); // Attendre 0.1 seconde (à ajuster selon le robot et l'environnement)

        totalDistance += speed * 0.1; // 0.1 est le temps de chaque itération

        if (totalDistance >= distance) {
            // Arrêter le robot (envoyer une commande d'arrêt)
            cmd_vel(0, 0); 
            printf("Stop: ");
            receive();
            sem_wait(sem);
            printf("%s\n", echoBuffer);
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
    printf("%s\n", echoBuffer);
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
            printf("Stop: ");
            receive();
            sem_wait(sem);
            printf("%s\n", echoBuffer);
            sem_post(sem);

            // Mettre à jour le booléen pour indiquer que la fonction est terminée
            *isFinished = 1;

            break;
        }
    }
}

void move_path(){

    connection(9999); // Establishing a connection with the robot

    struct KeyValuePair odomData[MAX_ENTRIES];
    int odomnumEntries = 0;

    //For the robot to know at which step he is
    int step = 0;
    int isFinished = 0;

    processOdomData(odomData, &odomnumEntries);


    struct KeyValuePair scanData[360];
    int scannumEntries = 0;

    processScanData(scanData, &scannumEntries);


    // distance beetween the robot and the circle
    float app = 0.5;

    double dist;
    printf("Enter linear distance: ");
    scanf("%lf", &dist);

    double rad;
    printf("Enter the radius: ");
    scanf("%lf", &rad);

    printf("Begin of the following path\n");

    //For the robot
    if (is_Test==1){
        if (step==0){
            moveRobot_linear(dist, &isFinished);
            if ((odomData[0].value==dist) && (odomData[1].value==0) && (odomData[2].value==0) && (isFinished==1)){
                step=1;
                isFinished=0;
            }

            if ((odomData[0].value==dist) && (odomData[1].value==0) && (odomData[2].value!=0) && (isFinished==1)){
                cmd_vel(0,0.5);
            }

            if ((odomData[0].value==dist) && (odomData[1].value=!0) && (isFinished==1)){
             cmd_vel(0,0.5);
            }
        }

        if (step==1){
            if ((scanData[4].value==app) && (isFinished==0)){
                moveRobot_circular(rad+app, &isFinished);

            }        
            if ((isFinished==1) && (odomData[2].value==-180)){
                step=2;
                isFinished=0;

            }        
            else{
                cmd_vel(0,0.5);
            }
        }

        if (step==2){
            moveRobot_linear(dist, &isFinished);
            if ((odomData[0].value==0) && (odomData[1].value==0) && (odomData[2].value==0)){
                step=3;
            }
        }
        if (step==3){
            printf("End of the following path\n");
        }   
    }

    //For test
    if (is_Test==0){
        if (step==0){
            moveRobot_linear(dist, &isFinished);
            if (isFinished==1){
                step=1;
                isFinished=0;
            }
        }
        if (step==1){
            moveRobot_circular(rad+app, &isFinished);        
            if (isFinished==1){
                step=2;
                isFinished=0;
            }        
        }
        if (step==2){
            moveRobot_linear(dist, &isFinished);
            if (isFinished==1){
                step=3;
            }
        }
        if (step==3){
            printf("End of the following path\n");
        }
    }
}


int main() {

    initializeIP();
    printf("Server IP: %s\n", servIP);
    
    init_shared_memory();

    // Uncommentary it to see the message received from the odom
    // char *odomResult = odom();
    // printf("Odom Result: %s\n", odomResult);
    // free(odomResult);

    // Uncommentary it to see the message received from the scan
    // char *scanResult = scan();
    // printf("Scan Result: %s\n", scanResult);
    // free(scanResult);

    // Uncomment the lign below if you want to test the filter of the scan
    // processScanData();

    // Uncomment the lign below if you want to test the filter of the odom
    // processOdomData();

    // Uncomment the lign below if you want to test the moving part, only working with test
    if (is_Test==0){
        move_path();
    }

    // Uncomment the lign below if you want to test the moving part commander
    // connection(9999);
    // cmd_vel(1,1);
    // receive();
    // sem_wait(sem);
    // printf("Received message: %s\n", echoBuffer);
    // sem_post(sem);

    close_shared_memory();
    close(sock);
    exit(0);
}

