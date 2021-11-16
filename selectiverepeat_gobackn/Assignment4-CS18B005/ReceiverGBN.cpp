#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <random>
#include <chrono>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define SA struct sockaddr
#define time_stamp chrono::high_resolution_clock::time_point
#define current_time chrono::high_resolution_clock::now
#define elapsed_time(end, start) chrono::duration_cast<chrono::milliseconds>(end - start).count()
#define precise_elapsed_time(end, start) chrono::duration_cast<chrono::microseconds>(end - start).count()
#define MAX_PACKET_SIZE 2048

using namespace std;

int main(int argc, char **argv)
{
    // debug mode?
    bool debug;
    // receiver's port number
    int recvport;
    // max no. of packets
    int max_packets;
    // packet error rate
    double packet_err_rate;

    if (argc != 7 && argc != 8)
    {
        printf("Make sure all the required arguments are passed. Aborting...\n");
        return 1;
    }

    // code for taking command-line parameters
    for (int i = 1; i < argc; i = i + 2)
    {
        string tmp = argv[i];
        if (tmp == "-d")
        {
            debug = true;
            i--;
        }
        else if (tmp == "-p")
            sscanf(argv[i + 1], "%d", &recvport);
        else if (tmp == "-n")
            sscanf(argv[i + 1], "%d", &max_packets);
        else if (tmp == "-e")
            sscanf(argv[i + 1], "%lf", &packet_err_rate);
        else
        {
            printf("Illegal parameter %s, please try again\n", argv[i]);
            return -1;
        }
    }

    // socket code begins
    struct sockaddr_in servaddr, cliaddr;
    int sockfd;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        printf("Socket creation failed\n");
        exit(0);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(recvport);

    if ((bind(sockfd, (SA *)&servaddr, sizeof(servaddr))) != 0)
    {
        printf("Socket bind failed\n");
        exit(0);
    }
    // socket code ends

    // no. of packets received
    int recvd_packets = 0;
    // the sequence number expected to receive, move forward only if this is received
    int expected_seqn = 0;
    // received packet
    char packet[MAX_PACKET_SIZE];

    // random number generator
    random_device rd;
    default_random_engine generator(rd());
    uniform_real_distribution<double> distribution(0.0, 1.0);

    // time stamp of the start of the program
    time_stamp prog_start = current_time();

    while (expected_seqn < max_packets)
    {
        socklen_t cliaddr_size;
        int packet_len = recvfrom(sockfd, packet, MAX_PACKET_SIZE, MSG_WAITALL,
                                  (SA *)&cliaddr, &cliaddr_size);

        packet[packet_len] = '\0';
        time_stamp recvd_time = current_time();

        double rndnum = distribution(generator);
        // drop the packet
        if (rndnum < packet_err_rate)
            continue;

        int seqn;
        sscanf(packet, "%d", &seqn);

        // ignore if the received packet is already acknowledged
        if (seqn < expected_seqn)
            continue;

        // received the expected packet
        else if (seqn == expected_seqn)
        {
            // move the window forward
            expected_seqn++;

            char ack[10];
            sprintf(ack, "ACK %d", expected_seqn);
            sendto(sockfd, ack, 10, 0, (SA *)&cliaddr, cliaddr_size);

            if (debug)
            {
                int time_recvd = precise_elapsed_time(recvd_time, prog_start);
                printf("%d: Time Received: %d:%d Packet dropped: false\n", seqn,
                       time_recvd / 1000, time_recvd % 1000);
            }
        }

        // send the ack with required packet sequence number if,
        // received packet is not yet acked and is not expected
        else
        {
            char ack[10];
            sprintf(ack, "ACK %d", expected_seqn);
            sendto(sockfd, ack, 10, 0, (SA *)&cliaddr, cliaddr_size);
        }
    }

    close(sockfd);

    return 0;
}