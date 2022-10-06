#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>  
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include "list.h"

#define MAXBUFSIZE 5000

void *getinputthread (void *sList);
void *senderthread (void *packet);
void *receiverthread (void *packet);
void *outputthread(void *packet);
void startuperror(void);
void encrypt (char *string, int *buf);
void decrypt (int *buf, char *string, int numchar);

// packets to be sent to each thread
struct packet {
    int *socket, *numbytes;
    List *slist, *rlist;
    struct addrinfo *myaddress_info;
    struct addrinfo *theiraddress_info;
    pthread_t *sendertid, *outputtid, *inputtid, *receivertid;
};

int main (int argc, char *argv[])
{
    if (argc != 4)
    {
        startuperror();
        exit (EXIT_FAILURE);
    }
    
    // Create 2 lists (one to be shared by the senders, and one to be shared by the receivers)
    struct List_s *sendList, *recList;
    
    sendList = List_create();
    if (sendList == NULL) {
        puts ("Error creating send slist");
    };

    recList = List_create();
    if (recList == NULL) {
        puts ("Error creating receive slist");
    }

    // Set up a Socket
    int sockfd, statusout, statusin;
    
    // Set up outgoing address info structs
    struct addrinfo hintsout;
    struct addrinfo *theirinfo;

    memset(&hintsout, 0, sizeof hintsout);
    hintsout.ai_family = AF_INET;
    hintsout.ai_socktype = SOCK_DGRAM;

    if ( (statusout = getaddrinfo(argv[2], argv[3], &hintsout, &theirinfo)) != 0)
    {
        fprintf (stderr, "getaddrinfo error: %s\n", gai_strerror(statusout));
        exit (EXIT_FAILURE);
    }

    // Set up incoming address info structs
    struct addrinfo hintsin;
    struct addrinfo *myinfo;

    memset(&hintsin, 0, sizeof hintsin);
    hintsin.ai_family = AF_INET;
    hintsin.ai_socktype = SOCK_DGRAM;

    if ( (statusin = getaddrinfo(argv[2], argv[1], &hintsin, &myinfo)) != 0)
    {
        fprintf (stderr, "getaddrinfo: %s\n", gai_strerror(statusin));
        exit (EXIT_FAILURE);
    }

    // create the socket
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
    {
        perror("Socket creation failed.");
        exit (EXIT_FAILURE);
    }

    // Create 4 threads
    pthread_t inputtid, sendertid, receivertid, outputtid;

    // To test for Online/Offline status
    int numbytesrec = 0;

    // Set up info to send to threads
    struct packet *info;
    info = (struct packet *) malloc (sizeof(struct packet));
    info->socket = &sockfd;
    info->myaddress_info = myinfo;
    info->theiraddress_info = theirinfo;
    info->slist = sendList;
    info->rlist = recList;
    info->numbytes = &numbytesrec;
    info->sendertid = &sendertid;
    info->outputtid = &outputtid;
    info->inputtid = &inputtid;
    info->receivertid = &receivertid;

    /* Create the threads */
    pthread_create(&inputtid, NULL, (void *) getinputthread, (void *) info);
    pthread_create(&receivertid, NULL, (void *) receiverthread, (void *) info);

    pthread_join(inputtid, NULL);
    pthread_join(receivertid, NULL);
    pthread_cancel(inputtid);
    pthread_cancel(receivertid);

    free (info);
    free (theirinfo);
    exit(0);
}

void startuperror (void)
{
    printf("Usage: \n\t ./lets-talk <local port> <remote host> <remote port>\n");
    printf("Examples: \n\t /lets-talk 6060 192.168.0.513 6001 \n\t /lets-talk 6060 some-computer-name 6001\n");

    return;
}

void encrypt (char *string, int *buf)
{
    for (int i = 0; i < strlen(string); i++)
    {
        *(buf + i) = (int) ( ((*(string + i)) + 3) % 256 );
    }

    return;
}

void decrypt (int *buf, char *string, int numchar)
{
    for (int i = 0; i < numchar; i++)
    {
        *(string + i) = (char) ((buf[i] - 3) % 256);
    }
    
    return;
}

void *getinputthread (void *info)
{
    struct packet *inputpacket = (struct packet *) info;
    
    puts("Welcome to Lets-Talk! Please type your messages now.");

    while (1)
    {
        char buf[MAXBUFSIZE];
        char *rest = buf;
        char *token = "\n";
        fgets(buf, sizeof(buf), stdin);

        if (strcmp(buf, "\n") != 0 )
            rest = strtok(buf, token);

        // Add the message to the head of the slist
        List_add((List*)inputpacket->slist, (void *) rest);

        // send the message
        pthread_create(inputpacket->sendertid, NULL, (void *) senderthread, (void *) info);
        pthread_join(*inputpacket->sendertid, NULL);

        if (strcmp(rest, "!exit") == 0)
        {
            break;
        }
    }

    pthread_cancel(*inputpacket->receivertid);
    pthread_exit(NULL);
}

void *senderthread (void *info)
{
    struct packet *senderpacket = (struct packet *) info;

    // get the message from the head of the slist
    char current[strlen((char *) List_first(senderpacket->slist))];
    strcpy(current, (char *) List_first(senderpacket->slist));

    // remove the node at the head of the slist
    List_remove(senderpacket->slist);

    // encrypt the message
    int encryptedstr[strlen(current)];
    encrypt(current, encryptedstr);

    // send the message
    int numbytes;
    if ((numbytes = sendto(*senderpacket->socket, encryptedstr, sizeof(encryptedstr), 0, senderpacket->theiraddress_info->ai_addr, senderpacket->theiraddress_info->ai_addrlen)) == -1)
    {
        perror("Unable to send message");
        exit (EXIT_FAILURE);
    }

    if (strcmp(current, "!status") == 0)
    {
        sleep(1);
        if (*senderpacket->numbytes == 0)
        {
            puts("Offline");
            fflush(stdout);
        }
    }

    pthread_exit(NULL);
}

void *receiverthread (void *info)
{
    struct packet *receiverpacket = (struct packet *) info;
    
    // bind to port
    if (bind(*receiverpacket->socket, receiverpacket->myaddress_info->ai_addr, receiverpacket->myaddress_info->ai_addrlen) == -1)
    {
        close(*receiverpacket->socket);
        perror("Listener failed to bind socket");
        exit (EXIT_FAILURE);
    }

    free (receiverpacket->myaddress_info);

    while (1)
    {
        // listen
        struct sockaddr_storage their_addr;
        socklen_t addr_len = sizeof their_addr;
        
        int buf[MAXBUFSIZE];
        memset(buf, 0, sizeof(buf));

        if ( (*receiverpacket->numbytes = recvfrom(*receiverpacket->socket, buf, sizeof(buf)-1, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror ("recvfrom");
            exit (EXIT_FAILURE);
        }

        // decrypt the message
        int size = *receiverpacket->numbytes/sizeof(int);
        char *decryptedmessage = (char *) malloc (sizeof(char) * (size+1));
        memset(decryptedmessage, '\0', sizeof(char)*size+1);
        decrypt(buf, decryptedmessage, size);

        // put the message to the head of the receiver slist
        List_add(receiverpacket->rlist, (void *) decryptedmessage);

        // send the message
        pthread_create(receiverpacket->outputtid, NULL, (void *) outputthread, (void *) info);
        pthread_join(*receiverpacket->outputtid, NULL);

        if(strcmp(decryptedmessage, "!exit") == 0)
        {
            free(decryptedmessage);
            break;    
        }

        free(decryptedmessage);
    }

    pthread_cancel(*receiverpacket->inputtid);
    if (close(*receiverpacket->socket) == -1)
    {
        perror ("Unable to close socket");
    }
    return NULL;
}

void *outputthread(void *packet)
{
    struct packet *outputpacket = (struct packet *) packet;

    // get the message from the head of the slist
    int size = (*outputpacket->numbytes) / sizeof(int) ;
    char current[size+1];
    strcpy(current, (char *) List_first(outputpacket->rlist));

    // remove the node at the head of the slist
    List_remove(outputpacket->rlist);

    
    // Check for !status
    if (strcmp(current, "!status") == 0)
    {
        // send a message back
        int numbytes;
        char *message = "Online";
        int buf[strlen(message)];
        encrypt(message, buf);
        if ((numbytes = sendto(*outputpacket->socket, buf, sizeof(buf), 0, outputpacket->theiraddress_info->ai_addr, outputpacket->theiraddress_info->ai_addrlen)) == -1)
        {
            perror("Unable to send message");
            exit (EXIT_FAILURE);
        }
    }
    else if (strcmp(current, "!exit") == 0)
        ;
    else
        printf("%s\n", current);

    fflush(stdout);

    pthread_exit(NULL);
}