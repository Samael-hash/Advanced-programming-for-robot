/*

* File: main.c

* Name: Valentin Stevenin & Kyrylo Stepanets

* Date: 21/01/2024

* Version: 1.0

* Description: This C++ code establishes a TCP/IP connection with a robot, utilizing sockets, 
               semaphores, and shared memory to communicate, receive and filter odometric and scan data, 
               and control the robot's linear and circular movements.

*/

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <cmath>
#include <iomanip>

#define RCVBUFSIZE 9999
#define MAX_ENTRIES 20
#define MAX_NAME_LENGTH 50
#define MAX_VALUE_LENGTH 50
#define MAX_IP_LENGTH 16

int sock;                        /* Socket descriptor */
struct sockaddr_in echoServAddr; /* Address for the Echo server */
unsigned short echoServPort;     /* Port of the Echo server */

int is_Test = 1;                   /* Choose 0 for testing (0) or using the robot (1) */

char servIP[MAX_IP_LENGTH];

// This function initialize the IP adress
void initializeIP() {
    if (is_Test == 0) {
        snprintf(servIP, sizeof(servIP), "127.0.0.1");      /* Server IP for testing */
    } else if (is_Test == 1) {
        snprintf(servIP, sizeof(servIP), "192.168.100.55");  /* Robot server IP, change 'X' according to your turtlebot */
    }
}

const char *echoString = "connection working";                /* String to send to the Echo server */
const char *echoScan = "scan data";                /* String to send to the Echo server */ 
char *echoBuffer;               /* Buffer for the Echo string */
unsigned int echoStringLen;      /* Length of the string to send */
int bytesRcvd, totalBytesRcvd;   /* Bytes read in a single recv() and total bytes read */

void DieWithError(const char *errorMessage) {  /* Error handling function */
    perror(errorMessage);
    exit(EXIT_FAILURE);
}

sem_t *sem;  // Semaphore for synchronization

//This function establishes a TCP connection
void connection(int port) {
    echoServPort = port;
    
    // Creating a TCP socket
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        DieWithError("socket() failed");
    }

    // Initializing server address structure
    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);
    echoServAddr.sin_port = htons(echoServPort);

    // Connecting to the server
    if (connect(sock, reinterpret_cast<struct sockaddr *>(&echoServAddr), sizeof(echoServAddr)) < 0){
         DieWithError("connect() failed");
    }
}

//This function disconnect a TCP connection
void disconnect(int port) {
    // Ensure that the socket is associated with the specified port before closing it
    if (port == echoServPort) {
        // Closing the socket
        if (close(sock) < 0) {
            DieWithError("close() failed");
        }
    }
    // If the port does not match, you can add a warning message or ignore it
    else {
        std::cerr << "The specified port does not match the current socket port." << std::endl;
    }
}

// This function sends the specified message (argv) to the server using the established socket connection
void message(const char *argv) {
    // Calculate the length of the message
    echoStringLen = strlen(argv);
    
    // Sending the message to the server
    if (send(sock, argv, echoStringLen, 0) != static_cast<ssize_t>(echoStringLen)){
        DieWithError("send() sent a different number of bytes than expected");
    }
}

//This function receives data from the server
char *receive(int datasize) {
    // Allocate memory for receiving data
    char *receivedData = static_cast<char *>(malloc(datasize * sizeof(char)));
    if (receivedData == nullptr) {
        DieWithError("Memory allocation failed");
    }

    totalBytesRcvd = 0;

    // If testing, receive data until the expected length is reached
    if (is_Test == 0) {
        while (totalBytesRcvd < echoStringLen) {
            if ((bytesRcvd = recv(sock, receivedData + totalBytesRcvd, datasize - totalBytesRcvd - 1, 0)) <= 0) {
                DieWithError("recv() failed or connection closed prematurely");
            }
            totalBytesRcvd += bytesRcvd;
        }

        receivedData[totalBytesRcvd] = '\0';  // Adding string termination
    }

    // If using the robot, continuously receive data until the connection is closed
    if (is_Test == 1) {
        while (1) {
            if ((bytesRcvd = recv(sock, receivedData + totalBytesRcvd, datasize - totalBytesRcvd - 1, 0)) <= 0) {
                // If the connection is closed or an error occurs, exit the loop
                break;
            }
            totalBytesRcvd += bytesRcvd;
            receivedData[totalBytesRcvd] = '\0';
        }
    }

    return receivedData;
}

//This function initializes shared memory and a semaphore for inter-process communication.
void init_shared_memory() {
    // Create or open a shared memory segment and obtain a file descriptor
    int fd = shm_open("/my_shared_memory", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    
    // Set the size of the shared memory segment
    ftruncate(fd, RCVBUFSIZE);
    
    // Map the shared memory segment into the process's address space
    echoBuffer = static_cast<char *>(mmap(NULL, RCVBUFSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    
    // Close the file descriptor after mapping
    close(fd);

    // Open or create a semaphore for synchronization
    sem = sem_open("/my_semaphore", O_CREAT, S_IRUSR | S_IWUSR, 1);
    if (sem == SEM_FAILED) {
        DieWithError("sem_open() failed");
    }
}

//This function closes and cleans up shared memory and the associated semaphore
void close_shared_memory() {
    // Unmap the shared memory segment from the process's address space
    munmap(echoBuffer, RCVBUFSIZE);

    // Close the semaphore
    sem_close(sem);

    // Remove the shared memory segment
    shm_unlink("/my_shared_memory");

    // Remove the semaphore
    sem_unlink("/my_semaphore");
}

//This function initiates a connection to a server on port 9997. The port of the Laserscan
char *scan() {
    // Establish a connection to the server on port 9997
    connection(9997);

    // For testing , send a message to the server to simulate the scan message
    if (is_Test==0){
        message("---START---{\"header\": {\"seq\": 20550, \"stamp\": {\"secs\": 1677514522, \"nsecs\": 217517053}, \"frame_id\": \"base_scan\"}, \"angle_min\": 0.0, \"angle_max\": 6.2657318115234375, \"angle_increment\": 0.01745329238474369, \"time_increment\": 0.0005592841189354658, \"scan_time\": 0.20134228467941284, \"range_min\": 0.11999999731779099, \"range_max\": 3.5, \"ranges\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.3619999885559082, 0.3619999885559082, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.318000078201294, 2.249000072479248, 0.8429999947547913, 2.4260001182556152, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.8250000476837158, 0.0, 0.0, 2.6989998817443848, 0.0, 4.14300012588501, 3.8970000743865967, 3.4739999771118164, 4.199999809265137, 4.1579999923706055, 0.0, 4.197999954223633, 0.0, 4.165999889373779, 4.189000129699707, 4.164000034332275, 4.184000015258789, 4.184000015258789, 0.0, 0.0, 4.158999919891357, 0.0, 0.0, 0.503000020980835, 0.5139999985694885, 0.5260000228881836, 0.5180000066757202, 0.5139999985694885, 0.5149999856948853, 0.5199999809265137, 0.527999997138977, 0.6759999990463257, 0.6779999732971191, 0.6809999942779541, 0.6729999780654907, 0.675000011920929, 0.6679999828338623, 0.0, 0.6470000147819519, 0.6359999775886536, 0.6259999871253967, 0.6129999756813049, 0.6010000109672546, 0.5889999866485596, 0.5899999737739563, 0.5849999785423279, 0.5759999752044678, 0.5640000104904175, 0.5509999990463257, 0.5479999780654907, 0.5419999957084656, 0.5350000262260437, 0.5260000228881836, 0.5180000066757202, 0.49799999594688416, 0.4869999885559082, 0.4909999966621399, 0.4909999966621399, 0.48500001430511475, 0.4790000021457672, 0.4779999852180481, 0.47999998927116394, 0.4729999899864197, 0.4749999940395355, 0.47699999809265137, 0.4790000021457672, 0.4819999933242798, 0.4830000102519989, 0.48500001430511475, 0.4869999885559082, 0.4959999918937683, 0.5049999952316284, 0.5559999942779541, 1.534999966621399, 1.465000033378601, 1.4709999561309814, 1.1770000457763672, 1.1610000133514404, 1.1579999923706055, 1.1360000371932983, 1.1380000114440918, 1.1319999694824219, 1.13100004196167, 1.1100000143051147, 1.0839999914169312, 1.090999960899353, 1.0950000286102295, 1.0759999752044678, 1.0779999494552612, 1.062000036239624, 1.069000005722046, 1.0809999704360962, 1.0700000524520874, 1.062000036239624, 1.0549999475479126, 1.0709999799728394, 1.0470000505447388, 1.0520000457763672, 0.9459999799728394, 1.315999984741211, 1.2929999828338623, 1.315999984741211, 1.3240000009536743, 1.315000057220459, 1.315000057220459, 1.3240000009536743, 1.2990000247955322, 1.3040000200271606, 1.309999942779541, 1.3380000591278076, 1.3220000267028809, 1.3259999752044678, 1.3250000476837158, 1.3339999914169312, 1.3489999771118164, 1.3289999961853027, 1.36899995803833, 1.3589999675750732, 1.3660000562667847, 1.3639999628067017, 1.3609999418258667, 1.3960000276565552, 1.3880000114440918, 1.3980000019073486, 1.4160000085830688, 1.4140000343322754, 1.4160000085830688, 1.440000057220459, 1.4459999799728394, 1.465999960899353, 1.4730000495910645, 1.5019999742507935, 1.5080000162124634, 1.5140000581741333, 1.5470000505447388, 1.5499999523162842, 1.5750000476837158, 1.593999981880188, 1.6139999628067017, 1.6360000371932983, 1.6640000343322754, 1.6670000553131104, 1.7100000381469727, 1.746000051498413, 1.7669999599456787, 1.7890000343322754, 1.812999963760376, 1.8389999866485596, 1.8700000047683716, 1.8880000114440918, 1.9320000410079956, 1.9550000429153442, 1.9839999675750732, 2.055999994277954, 2.0840001106262207, 2.127000093460083, 2.197999954223633, 2.2360000610351562, 2.302000045776367, 2.368000030517578, 2.428999900817871, 2.502000093460083, 2.5850000381469727, 2.747999906539917, 2.8239998817443848, 2.9240000247955322, 2.9809999465942383, 3.111999988555908, 3.2279999256134033, 3.38100004196167, 3.490999937057495, 3.615999937057495, 3.819000005722046, 3.9019999504089355, 0.0, 4.065000057220459, 4.184999942779541, 0.0, 2.382999897003174, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 3.5899999141693115, 3.5280001163482666, 3.556999921798706, 3.4200000762939453, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], \"intensities\": [0.0, 0.0, 170.0, 170.0, 170.0, 0.0, 0.0, 170.0, 170.0, 0.0, 0.0, 0.0, 36.0, 105.0, 52.0, 78.0, 96.0, 0.0, 0.0, 0.0, 0.0, 0.0, 72.0, 80.0, 106.0, 0.0, 96.0, 47.0, 442.0, 104.0, 116.0, 117.0, 121.0, 138.0, 126.0, 93.0, 85.0, 62.0, 0.0, 61.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 105.0, 170.0, 105.0, 69.0, 0.0, 105.0, 150.0, 0.0, 0.0, 170.0, 170.0, 0.0, 0.0, 0.0, 170.0, 0.0, 0.0, 0.0, 0.0, 170.0, 0.0, 0.0, 0.0, 105.0, 0.0, 0.0, 0.0, 170.0, 0.0, 170.0, 0.0, 0.0, 170.0, 0.0, 0.0, 0.0, 60.0, 81.0, 2840.0, 1263.0, 79.0, 132.0, 150.0, 174.0, 343.0, 511.0, 518.0, 396.0, 397.0, 353.0, 488.0, 240.0, 351.0, 2787.0, 103.0, 170.0, 0.0, 0.0, 0.0, 130.0, 0.0, 456.0, 0.0, 0.0, 538.0, 0.0, 289.0, 185.0, 1313.0, 451.0, 567.0, 606.0, 718.0, 844.0, 950.0, 661.0, 572.0, 551.0, 525.0, 483.0, 486.0, 467.0, 450.0, 434.0, 1096.0, 3619.0, 3318.0, 3578.0, 3940.0, 3868.0, 3606.0, 4910.0, 940.0, 2799.0, 2496.0, 2996.0, 2364.0, 2618.0, 2377.0, 2619.0, 2750.0, 4141.0, 2716.0, 2313.0, 3475.0, 3649.0, 2878.0, 3614.0, 2659.0, 3078.0, 3007.0, 2897.0, 3302.0, 3693.0, 3723.0, 3330.0, 3310.0, 3681.0, 3162.0, 2888.0, 3113.0, 4428.0, 3643.0, 3486.0, 3498.0, 3608.0, 3829.0, 3731.0, 3786.0, 2977.0, 4043.0, 3736.0, 6391.0, 297.0, 1067.0, 2049.0, 2455.0, 1635.0, 1829.0, 1922.0, 2058.0, 2179.0, 1959.0, 2057.0, 1912.0, 2066.0, 2108.0, 2123.0, 2181.0, 2203.0, 2274.0, 2331.0, 2351.0, 2509.0, 2759.0, 2766.0, 2316.0, 2124.0, 2615.0, 3379.0, 1456.0, 1629.0, 1694.0, 1648.0, 1520.0, 1805.0, 1664.0, 1711.0, 1694.0, 1622.0, 1651.0, 1317.0, 1560.0, 2154.0, 1582.0, 1555.0, 1505.0, 1612.0, 1671.0, 1610.0, 1562.0, 1742.0, 1801.0, 1903.0, 1817.0, 1779.0, 1730.0, 2094.0, 1825.0, 1900.0, 2122.0, 2037.0, 2149.0, 2101.0, 2001.0, 1911.0, 1804.0, 1697.0, 1738.0, 1593.0, 1481.0, 1550.0, 1297.0, 1196.0, 1231.0, 1305.0, 1212.0, 837.0, 1040.0, 960.0, 907.0, 860.0, 817.0, 756.0, 779.0, 716.0, 640.0, 562.0, 497.0, 441.0, 396.0, 371.0, 356.0, 330.0, 250.0, 256.0, 266.0, 289.0, 279.0, 292.0, 281.0, 271.0, 245.0, 229.0, 266.0, 2208.0, 258.0, 243.0, 244.0, 2038.0, 78.0, 567.0, 162.0, 46.0, 118.0, 88.0, 154.0, 98.0, 98.0, 0.0, 0.0, 0.0, 154.0, 98.0, 98.0, 99.0, 267.0, 334.0, 295.0, 280.0, 67.0, 0.0, 154.0, 154.0, 0.0, 0.0, 154.0, 65.0, 98.0, 101.0, 154.0, 0.0, 0.0, 0.0, 92.0, 0.0, 0.0, 0.0, 154.0, 0.0, 0.0, 0.0, 0.0, 154.0, 0.0, 0.0, 0.0, 0.0, 0.0, 109.0, 0.0, 154.0, 154.0, 0.0, 0.0, 0.0, 0.0, 154.0, 0.0, 154.0, 0.0, 0.0, 154.0, 0.0, 0.0, 98.0, 154.0, 0.0, 0.0, 0.0, 98.0, 0.0, 98.0]}___END___"); 
    }

    // For testing (is_Test=1), send a message to the server
    if (is_Test == 1) {
        message(".");
    }

    // Receive data from the server with a maximum data size of 8000
    char *receivedData = receive(9999);

    // Write the received data to shared memory
    sem_wait(sem);
    strcpy(echoBuffer, receivedData);
    sem_post(sem);

    // Disconnect from the server on port 9997
    disconnect(9997);

    return receivedData;
}

//This function initiates a connection to a server on port 9998. The port of the Odometry
char *odom() {
    // Establish a connection to the server on port 9998
    connection(9998);

    // For testing , send a message to the server to simulate the odom message
    if (is_Test==0){
        message("---START---{\"header\": {\"seq\": 96997, \"stamp\": {\"secs\": 1677514236, \"nsecs\": 463284111}, \"frame_id\": \"odom\"}, \"child_frame_id\": \"base_footprint\", \"pose\": {\"pose\": {\"position\": {\"x\": 0.45223192989826202, \"y\": 0.86337219601869583, \"z\": 0.0}, \"orientation\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.965720534324646, \"w\": 0.2595840394496918}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}, \"twist\": {\"twist\": {\"linear\": {\"x\": -0.0007909589330665767, \"y\": 0.0, \"z\": 0.0}, \"angular\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.0019147992134094238}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}}___END___---START---{\"header\": {\"seq\": 96997, \"stamp\": {\"secs\": 1677514236, \"nsecs\": 463284111}, \"frame_id\": \"odom\"}, \"child_frame_id\": \"base_footprint\", \"pose\": {\"pose\": {\"position\": {\"x\": 0.73223192989826202, \"y\": 0.28237219601869583, \"z\": 0.0}, \"orientation\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.965720534324646, \"w\": 0.2595840394496918}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}, \"twist\": {\"twist\": {\"linear\": {\"x\": -0.0007909589330665767, \"y\": 0.0, \"z\": 0.0}, \"angular\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.0019147992134094238}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}}___END___---START---{\"header\": {\"seq\": 96997, \"stamp\": {\"secs\": 1677514236, \"nsecs\": 463284111}, \"frame_id\": \"odom\"}, \"child_frame_id\": \"base_footprint\", \"pose\": {\"pose\": {\"position\": {\"x\": 0.23223192989826202, \"y\": 0.08237219601869583, \"z\": 0.0}, \"orientation\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.965720534324646, \"w\": 0.2595840394496918}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}, \"twist\": {\"twist\": {\"linear\": {\"x\": -0.0007909589330665767, \"y\": 0.0, \"z\": 0.0}, \"angular\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.0019147992134094238}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}}___END___---START---{\"header\": {\"seq\": 96997, \"stamp\": {\"secs\": 1677514236, \"nsecs\": 463284111}, \"frame_id\": \"odom\"}, \"child_frame_id\": \"base_footprint\", \"pose\": {\"pose\": {\"position\": {\"x\": 0.93489728978191290, \"y\": 0.24543672618774239, \"z\": 0.0}, \"orientation\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.205720534324646, \"w\": 0.2595840394496918}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}, \"twist\": {\"twist\": {\"linear\": {\"x\": -0.0007909589330665767, \"y\": 0.0, \"z\": 0.0}, \"angular\": {\"x\": 0.0, \"y\": 0.0, \"z\": -0.0019147992134094238}}, \"covariance\": [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}}___END___"); // just for the test
    }

    // For receive the messa from the robot .
    if (is_Test == 1) {
        message(".");
    }

    // Receive data from the server with a maximum data size of 1000
    char *receivedData = receive(1000);

    // Write the received data to shared memory
    sem_wait(sem);
    strcpy(echoBuffer, receivedData);
    sem_post(sem);

    // Disconnect from the server on port 9998
    disconnect(9998);

    return receivedData;
}

//This function initiates a connection to a server on port 9999. The port dor the commander
void cmd_vel(float linear, float angular) {
    // Establish a connection to the server on port 9999
    connection(9999);

    // Wait for the semaphore
    sem_wait(sem);

    char command[100];
    // Format the command with linear and angular values
    snprintf(command, sizeof(command), "---START---{\"linear\":%f , \"angular\":%f}___END___", linear, angular);
    std::string formattedCommand = std::string(command);

    // Use the formatted string in the subsequent code
    if (send(sock, formattedCommand.c_str(), formattedCommand.length(), 0) != formattedCommand.length()) {
        sem_post(sem); // Release the semaphore in case of an error
        DieWithError("Error sending message");
    }

    // Release the semaphore after sending the message
    sem_post(sem);

    // Disconnect from the server on port 9999
    disconnect(9999);
}


struct KeyValuePair {
    char key[50];
    float value;
};

//This function  extract specific values from the provided data
void addToOdomData(KeyValuePair *odomData, int *numEntries, const char *dataType, float x, float y, float z, float w) {
    // List of desired data types to be extracted from the provided values
    const char *desiredData[] = {"position_x", "position_y", "orientation_z", "linear_x", "angular_z"};

    // Loop through the desired data types
    for (int i = 0; i < sizeof(desiredData) / sizeof(desiredData[0]); ++i) {
        // Check if the current data type matches the desired data type
        if (strcmp(dataType, desiredData[i]) == 0) {
            float valueToAdd = 0.0;

            // Assign the appropriate value based on the desired data type
            if (i == 0) {
                valueToAdd = x;  // Extract 'x' for "position_x"
            } else if (i == 1) {
                valueToAdd = y;  // Extract 'y' for "position_y"
            } else if (i == 2) {
                valueToAdd = z;  // Extract 'z' for "orientation_z"
            } else if (i == 3) {
                valueToAdd = x;  // Extract 'x' for "linear_x"
            } else if (i == 4) {
                valueToAdd = z;  // Extract 'z' for "angular_z"
            }

            // Store the extracted key-value pair in the odomData array
            odomData[*numEntries].value = valueToAdd;
            strcpy(odomData[*numEntries].key, dataType);
            (*numEntries)++;
        }
    }
}

//This function extract specific data related to position, orientation, linear, and angular values, and add them to the odomData array
void extractOdomData(const char *message, KeyValuePair *odomData, int *numEntries) {
    // Find the start and end markers in the message
    const char *startPtr = strstr(message, "---START---");
    const char *endPtr = strstr(message, "___END___");

    // Check if both markers are present in the message
    if (startPtr != nullptr && endPtr != nullptr) {
        const char *ptr = startPtr;

        // Iterate through the message to extract relevant data
        while (ptr < endPtr) {
            // Extract position data
            ptr = strstr(ptr, "\"position\": {\"x\": ");
            if (ptr != nullptr) {
                float x, y, z;
                // Extract x, y, and z values
                int result = sscanf(ptr, "\"position\": {\"x\": %f, \"y\": %f, \"z\": %f},", &x, &y, &z);
                if (result == 3) {
                    // Add position_x and position_y to odomData
                    addToOdomData(odomData, numEntries, "position_x", x, y, z, 0.0);
                    addToOdomData(odomData, numEntries, "position_y", x, y, z, 0.0);
                }
            }

            // Extract orientation data
            ptr = strstr(ptr, "\"orientation\": {\"x\": ");
            if (ptr != nullptr) {
                float x, y, z, w;
                // Extract x, y, z, and w values
                sscanf(ptr, "\"orientation\": {\"x\": %f, \"y\": %f, \"z\": %f, \"w\": %f}", &x, &y, &z, &w);
                // Add orientation_z to odomData
                addToOdomData(odomData, numEntries, "orientation_z", x, y, z, w);
            }

            // Extract linear data
            ptr = strstr(ptr, "\"linear\": {\"x\": ");
            if (ptr != nullptr) {
                float x, y, z;
                // Extract x, y, and z values
                sscanf(ptr, "\"linear\": {\"x\": %f, \"y\": %f, \"z\": %f}", &x, &y, &z);
                // Add linear_x to odomData
                addToOdomData(odomData, numEntries, "linear_x", x, y, z, 0.0);
            }

            // Extract angular data
            ptr = strstr(ptr, "\"angular\": {\"x\": ");
            if (ptr != nullptr) {
                float x, y, z;
                // Extract x, y, and z values
                sscanf(ptr, "\"angular\": {\"x\": %f, \"y\": %f, \"z\": %f}", &x, &y, &z);
                // Add angular_z to odomData
                addToOdomData(odomData, numEntries, "angular_z", x, y, z, 0.0);
            }

            // Look for the start marker again
            ptr = strstr(ptr, "---START---");

            // Break the loop if the start marker is not found
            if (ptr == nullptr) {
                break;
            }
        }
    } else {
        // Print an error message if markers are missing
        std::cout << "Délimiteurs manquants dans le message." << std::endl;
    }
}

//This function process odometer data by repeatedly extracting and printing information until the end of the message is reached. 
void processOdomData(KeyValuePair *odomData, int *numEntries) {
    // Retrieve odometer data message from the robot
    const char *message = odom();
    std::cout << "Données extraites :" << std::endl;

    // Continue processing until the message is nullptr
    while (message != nullptr) {
        // Find the start marker in the message
        const char *startPtr = strstr(message, "---START---");

        // Check if the start marker is found
        if (startPtr != nullptr) {
            // Find the end marker starting from the identified start marker
            const char *endPtr = strstr(startPtr, "___END___");

            // Check if the end marker is found
            if (endPtr != nullptr) {
                // Reset the number of entries to 0 before extracting new data
                *numEntries = 0;

                // Extract odometer data from the message
                extractOdomData(startPtr, odomData, numEntries);

                // Output extracted data
                std::cout << std::endl;
                for (int i = 0; i < *numEntries; ++i) {
                    std::cout << odomData[i].key << ": " << odomData[i].value << std::endl;
                }

                // Move the message pointer to the end of the extracted data
                message = endPtr + strlen("___END___");
            } else {
                // Print an error message if the end marker is missing
                std::cout << "Délimiteur '___END___' manquant dans le message.\n" << std::endl;
                break;
            }
        } else {
            // Print an error message if the start marker is missing
            std::cout << "Délimiteur '---START---' manquant dans le message.\n" << std::endl;
            break;
        }
    }
}

struct RangeData {
    float *ranges;
    int numRanges;
};

//This function appears to extract range data from a message,
void extractRangeData(const char *message, RangeData *rangeData) {
    // Find the starting point of the ranges in the message
    const char *startPtr = strstr(message, "ranges\": [");

    // Find the ending point of the ranges in the message
    const char *endPtr = strstr(message, "], \"intensities\"");

    // Check if both start and end markers are found
    if (startPtr != nullptr && endPtr != nullptr) {
        // Calculate the length of the "ranges\": [" string
        size_t startLen = strlen("ranges\": [");
        // Move the start pointer to the beginning of the actual ranges
        startPtr += startLen;

        // Calculate the length of the ranges string
        size_t endLen = endPtr - startPtr;

        // Allocate memory for the ranges string
        char *rangesStr = static_cast<char *>(malloc(endLen + 1));

        // Copy the ranges string into the allocated memory
        strncpy(rangesStr, startPtr, endLen);
        // Null-terminate the string
        rangesStr[endLen] = '\0';

        // Initialize variables to store parsed ranges
        float *ranges = nullptr;
        int numRanges = 0;

        // Tokenize the ranges string and convert tokens to float values
        char *token = strtok(rangesStr, ",");
        while (token != nullptr) {
            // Convert token to a float value
            float value = atof(token);

            // Reallocate memory for the ranges array
            ranges = static_cast<float *>(realloc(ranges, (numRanges + 1) * sizeof(float)));
            
            // Store the value in the ranges array
            ranges[numRanges++] = value;

            // Move to the next token
            token = strtok(nullptr, ",");
        }

        // Store the parsed ranges data in the RangeData structure
        rangeData->ranges = ranges;
        rangeData->numRanges = numRanges;

        // Free the memory allocated for the ranges string
        free(rangesStr);
    } else {
        // Print an error message if start or end markers are missing
        std::cout << "Délimiteurs manquants dans le message." << std::endl;
    }
}

//This function adds range data to the scan array, selecting data points based on a specified degree interval.
void addRanges(KeyValuePair *scanData, int *numEntries, const RangeData *rangeData) {
    // Define the degree interval for selecting data points
    int degree = 1;

    // Iterate through the range data
    for (int i = 0; i < rangeData->numRanges; ++i) {
        // Select data points based on the defined degree interval
        if ((i % degree) == 0) {
            // Create a key string with the current degree
            char key[20];
            sprintf(key, "%d°", i);

            // Store the range value and associated key in the scanData array
            scanData[*numEntries].value = rangeData->ranges[i];
            strcpy(scanData[*numEntries].key, key);

            // Increment the number of entries in the scanData array
            (*numEntries)++;
        }
    }
}

//This function processes scan data received from the server, extracting range data, adding it to the scanData array
void processScanData(KeyValuePair *scanData, int *numEntries) {
    // Create a structure to hold range data
    RangeData rangeData;

    // Retrieve the scan message from the server
    const char *message = scan();
    std::cout << "Extracted Scan Data:" << std::endl;

    // Process each scan message
    while (message != nullptr) {
        // Find the start delimiter in the message
        const char *startPtr = strstr(message, "---START---");

        if (startPtr != nullptr) {
            // Find the end delimiter in the message
            const char *endPtr = strstr(startPtr, "___END___");

            if (endPtr != nullptr) {
                // Reset the number of entries for the scan data
                *numEntries = 0;

                // Extract range data from the scan message
                extractRangeData(startPtr, &rangeData);

                // Add range data to the scanData array
                addRanges(scanData, numEntries, &rangeData);

                // Display the extracted scan data
                std::cout << std::endl;
                for (int i = 0; i < *numEntries; ++i) {
                    std::cout << scanData[i].key << ": " << scanData[i].value << std::endl;
                }

                // Move to the next scan message in the message stream
                message = endPtr + strlen("___END___");
            } else {
                // Display an error if the end delimiter is missing
                std::cout << "Missing '___END___' delimiter in the message." << std::endl;
                break;
            }
        } else {
            // Display an error if the start delimiter is missing
            std::cout << "Missing '---START---' delimiter in the message." << std::endl;
            break;
        }
    }
}


double get_radius(){
    
    KeyValuePair scanData[360];
    int scannumEntries = 0;
    processScanData(scanData, &scannumEntries);
    int i;
    double radius;
    for(i=0; i<sizeof(scanData);i++){
        if(scanData[i].value==0){
            radius = scanData[0].value*tan(i*M_PI/180);
            break;
        }
    }
    return radius;
}

void security(int *secur){
    KeyValuePair scanData[360];
    int scannumEntries = 0;
    processScanData(scanData, &scannumEntries);
    int block;
    if (*secur==0){
        int i;
        for(i=0; i<sizeof(scanData);i++){
            if((scanData[i].value<0.02) && (scanData[i].value>0.01) ){
                cmd_vel(0.0,0.0);
                std::cout << "Security danger "<< std::endl;
                block =i;
                *secur = 1;
                break;
            }
        }
    }
    if (*secur==1){
        if((scanData[block].value>0.02) || (scanData[block].value<0.01) ){
                std::cout << "No more danger "<< std::endl;
                *secur = 0;
                block =-1;
            }

    }   
}

//This function move the robot linearly by a specified distance.
void moveRobot_linear(double dist_b,double dist_c, int *isFinished,double diam_d) {
    // Array to store odometer data
    KeyValuePair odomData[MAX_ENTRIES];
    // Number of entries in the odometer data array
    int odomnumEntries = 0;
    // Retrieve and process odometer data to calculate the target position
    processOdomData(odomData, &odomnumEntries);
    double end_odom = odomData[0].value + dist_b;

    double end_scan = dist_c-(diam_d/2);
    std::cout << "end_scan: " << end_scan << std::endl;


    // Time duration for the linear motion
    int time = 5;
    // Calculate the speed required to cover the specified distance in the given time
    double speed = dist_b / time;
    std::cout << "speed: " << speed << std::endl;

    // Send velocity command to move the robot linearly
    cmd_vel(speed, 0.0);

    // Display the received message in the shared memory
    sem_wait(sem);
    std::cout << echoBuffer << std::endl;
    sem_post(sem);

    // Monitor the robot's position until the target position is reached
    while (1) {
        // Retrieve and process odometer data to get the current position
        KeyValuePair odomData[MAX_ENTRIES];
        int odomnumEntries = 0;
        processOdomData(odomData, &odomnumEntries);
        double position = odomData[0].value;
        std::cout << "end: " << end_odom << std::endl;

        KeyValuePair scanData[360];
        int scannumEntries = 0;
        processScanData(scanData, &scannumEntries);
        double scan_end = scanData[0].value;
        std::cout << "scan_end: " << scan_end<< std::endl;

        // Check if the robot has reached the target position with a small tolerance
        if (std::abs(end_odom - position) < 0.005 || (scan_end <= end_scan && scan_end > 0.11)) {
            // Stop the robot by sending a stop command
            cmd_vel(0, 0);
            
            // Display the received message in the shared memory
            sem_wait(sem);
            std::cout << echoBuffer << std::endl;
            sem_post(sem);

            // Update the boolean to indicate that the function has finished
            *isFinished = 1;
            // std::cout << "isFinished: " << *isFinished << std::endl;
            break;
        }
    }
}

//This function move the robot in a circular path with a specified radius. 
void moveRobot_circular(double radius, int *isFinished) {
    // Array to store odometer data
    KeyValuePair odomData[MAX_ENTRIES];
    // Number of entries in the odometer data array
    int odomnumEntries = 0;
    // Retrieve and process initial odometer data to get the starting position
    processOdomData(odomData, &odomnumEntries);
    double start_x = odomData[0].value;
    double start_y = odomData[1].value;

    std::cout << "isFinished: " << *isFinished << std::endl;

    // Calculate the circumference of the circle
    double circumference = 2.0 * M_PI * radius;

    // Set the time duration for circular motion
    int time = 10;

    // Calculate the linear speed required to cover the circumference with a modified factor
    double speed = circumference * (1 - radius) / time;
    // Calculate the corresponding angular speed based on the radius
    double angularSpeed = speed / radius;

    // Move the robot with the calculated angular speed to make it follow a circular path
    cmd_vel(speed, angularSpeed);
    
    // Display the received message in the shared memory
    sem_wait(sem);
    std::cout << echoBuffer << std::endl;
    sem_post(sem);

    // Pause for a short time to allow the robot to start moving
    sleep(2);

    // Simulate the circular movement until the robot completes one full rotation
    while (1) {
        // Retrieve and process current odometer data to get the current position
        KeyValuePair odomData[MAX_ENTRIES];
        int odomnumEntries = 0;
        processOdomData(odomData, &odomnumEntries);
        double position_x = odomData[0].value;
        double position_y = odomData[1].value;

        std::cout << "start_x: " << start_x << std::endl;
        std::cout << "start_y: " << start_y << std::endl;

        // Move the robot with the calculated angular speed to make it follow a circular path
        cmd_vel(speed, angularSpeed);

        // Stop the robot when it completes one full rotation (a circumference)
        if ((std::abs(start_x - position_x) < 0.02) && (std::abs(start_y - position_y) < 0.02)) {
            // Send a stop command to the robot
            cmd_vel(0, 0);
            std::cout << "Done ";
            
            // Display the received message in the shared memory
            sem_wait(sem);
            std::cout << echoBuffer << std::endl;
            sem_post(sem);

            // Update the boolean to indicate that the function has finished
            *isFinished = 1;
            std::cout << "isFinished: " << *isFinished << std::endl;

            break;
        }
    }
}

//This function defines a sequence of robot movements based on user input.
void move_path() {
    

    // Variable to keep track of the current step in the movement sequence
    int step = 0;
    // Boolean indicating whether the current movement is finished
    int isFinished = 0;

    // Prompt user for distance b
    double dist_b;
    std::cout << "Enter distance b: ";
    std::cin >> dist_b;

    // Prompt user for distance c
    double dist_c;
    std::cout << "Enter distance b: ";
    std::cin >> dist_c;

    // Prompt user for the radius
    double diam_d;
    std::cout << "Enter the diameter d: ";
    std::cin >> diam_d;

        // distance between the robot and the circle
    float app = dist_c-(diam_d/2);


    std::cout << "Begin of the following path\n";
    // Execute movement steps based on the current step variable
    if (step == 0) {
        // Move the robot linearly for the specified distance
        moveRobot_linear(dist_b,dist_c, &isFinished,diam_d);
        if (isFinished == 1) {
            // Turn the robot clockwise (negative angular velocity) for a brief period, need to turn PI / 2 for 90°, so in 3 seconde, velocity PI/6
            cmd_vel(0.0, -(M_PI/6));
            sleep(3);
            // Stop the robot
            cmd_vel(0.0, 0.0);
            // Move to the next step in the sequence
            step = 1;
            isFinished = 0;
        }
    }

    if (step == 1) {
        // Move the robot in a circular path with a modified radius
        moveRobot_circular((diam_d/2)+app, &isFinished);
        if (isFinished == 1) {
            // Turn the robot clockwise (negative angular velocity) for a brief period
            cmd_vel(0.0, -(M_PI/6));
            sleep(3);
            // Stop the robot
            cmd_vel(0.0, 0.0);
            // Move to the next step in the sequence
            step = 2;
            isFinished = 0;
        }
    }

    if (step == 2) {
        // Move the robot linearly for the specified distance
        moveRobot_linear(dist_b,dist_c, &isFinished,diam_d);
        if (isFinished == 1) {
            // Move to the final step in the sequence
            step = 3;
        }
    }

    if (step == 3) {
        std::cout << "End of the following path\n";
    }
}

int main() {
    initializeIP();
    std::cout << "Server IP: " << servIP << std::endl;

    init_shared_memory();

    // Uncomment the line below if you want to test the moving part commander
    // cmd_vel(-0.4, -0.5);
    // sleep(3);
    // cmd_vel(0.0,0.0);

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

    // Uncomment the line below if you want to test the filter for scan on a loop. It may not work if your using a batterie, try the robot with cable
    // while(1){
    //     KeyValuePair scanData[360];
    //     int scannumEntries = 0;
    //     processScanData(scanData, &scannumEntries);
    //     double scan_end= scanData[0].value;
    //     std::cout << "scan_end: " << scan_end<< std::endl;
    // }

    // Uncomment the line below if you want to test the filter for odom on a loop
    // while(1){
    //     KeyValuePair odomData[MAX_ENTRIES];
    //     int odomnumEntries = 0;
    //     processOdomData(odomData, &odomnumEntries);
    // }

    // Uncomment the line below if you want to test the linear moving part, only working with is_tested = 1.
    // int isFinished = 0;
    // moveRobot_linear(0.4,0.3,&isFinished,0.15);

    // Uncomment the line below if you want to test the circular moving part, only working with is_tested = 1.
    // int isFinished = 0;
    // moveRobot_circular(0.2,&isFinished);

    // Uncomment the line below if you want to test the moving part, only working with is_tested = 1.
    // move_path();


    // int isFinished = 0;
    // double rad=get_radius();
    // std::cout << "radius: " << rad << std::endl;
    // moveRobot_circular(rad,&isFinished);

    close_shared_memory();
    close(sock);
    exit(0);
}


