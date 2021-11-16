#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <random>
#include <vector>
#include <mutex>
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

// addresses and socket file descriptor
struct sockaddr_in servaddr, cliaddr;
int sockfd;

// packet error rate
double packet_err_rate;
// buffer size
int buffer_size;

// random number generator
random_device rd;
default_random_engine generator(rd());
uniform_real_distribution<double> distribution(0.0, 1.0);

// struct to store all the information about the packet
typedef struct packet_type
{
    int seqnum;
    char data[MAX_PACKET_SIZE];
    int packet_len;
    time_stamp recv_time;
} packet_type;
// buffer to store packets, can store upto buffer_size only
vector<packet_type> buffer;
// mutex to prevent any race conditions for the buffer
mutex rbuffer_mutex;

// run as a separate thread to listen for the incoming packets
void listenforPackets()
{
    while (true)
    {
        char packet[MAX_PACKET_SIZE];
        socklen_t cliaddr_size;
        int packet_len = recvfrom(sockfd, packet, MAX_PACKET_SIZE, MSG_WAITALL,
                                  (SA *)&cliaddr, &cliaddr_size);

        packet[packet_len] = '\0';
        double rndnum = distribution(generator);
        // drop the packet
        if (rndnum < packet_err_rate)
            continue;

        int seqn;
        sscanf(packet, "%d", &seqn);

        packet_type tmp;

        tmp.seqnum = seqn;
        strcpy(tmp.data, packet);
        tmp.recv_time = current_time();
        tmp.packet_len = packet_len;

        rbuffer_mutex.lock();

        // push into the buffer if size of the vector has not exceeded buffer_size
        if (buffer.size() < buffer_size)
        {
            buffer.push_back(tmp);
            char ack[10];
            sprintf(ack, "ACK %d", seqn);
            sendto(sockfd, ack, 10, 0, (SA *)&cliaddr, cliaddr_size);
        }

        rbuffer_mutex.unlock();
    }
}

int main(int argc, char **argv)
{
    // receiver's port number
    int recvport;
    // max. number of packets
    int max_packets;
    // sequence number field length
    int seqnfl;
    // window size
    int window_size;
    // debug mode?
    bool debug;

    if (argc != 13 && argc != 14)
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
        else if (tmp == "-N")
            sscanf(argv[i + 1], "%d", &max_packets);
        else if (tmp == "-n")
            sscanf(argv[i + 1], "%d", &seqnfl);
        else if (tmp == "-W")
            sscanf(argv[i + 1], "%d", &window_size);
        else if (tmp == "-B")
            sscanf(argv[i + 1], "%d", &buffer_size);
        else if (tmp == "-e")
            sscanf(argv[i + 1], "%lf", &packet_err_rate);
        else
        {
            printf("Illegal parameter %s, please try again\n", argv[i]);
            return -1;
        }
    }

    // socket code begins
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

    // no. of received packets
    int recvd_packets = 0;
    // bool mask of the window for indicating whether the packet is received
    bool packet_window[window_size] = {false};
    // time stamps of the received packets
    time_stamp recv_time[window_size];
    // seqn of the first packet in buffer
    int first_seqn = 0;

    // max sequence number determined by seqnfl
    int max_seqn = pow(2, seqnfl);

    // time stamp of the start of the program
    time_stamp prog_start = current_time();

    // starting thread to listenforPackets
    thread listen_thread(listenforPackets);

    while (first_seqn < max_packets)
    {
        rbuffer_mutex.lock();

        // remove all the addressed packets from the buffer
        while (!buffer.empty())
        {
            int seqn = buffer[0].seqnum;
            // if seqn < first_seqn, overflow of the seqnum field occured
            // calculate for the relative position in the buffer in that case
            int idx = seqn < first_seqn ? max_seqn - first_seqn + seqn : seqn - first_seqn;

            // if the received packet is beyond window
            if (idx >= window_size)
            {
                buffer.erase(buffer.begin());
                continue;
            }

            packet_window[idx] = true;
            recv_time[idx] = buffer[0].recv_time;

            // remove from the buffer
            buffer.erase(buffer.begin());
        }

        rbuffer_mutex.unlock();

        // shifting the windows depending on the no. of consecutive acked packets
        if (packet_window[0])
        {
            // amount by which the window has to be shifted
            int shift = 1;
            int time_recvd;
            if (debug)
            {
                time_recvd = precise_elapsed_time(recv_time[0], prog_start);
                printf("%d: Time Received: %d:%d\n", first_seqn % max_seqn, time_recvd / 1000, time_recvd % 1000);
            }
            for (int i = 1; i < window_size; i++)
            {
                // break if an unacked packet is encountered
                if (!packet_window[i])
                    break;

                if (debug)
                {
                    time_recvd = precise_elapsed_time(recv_time[i], prog_start);
                    printf("%d: Time Received: %d:%d\n", (first_seqn + i) % max_seqn, time_recvd / 1000, time_recvd % 1000);
                }
                // increment the shift value for every acked packet encounter
                shift++;
            }

            // increment the first_seqn by the shift length
            first_seqn += shift;

            // shift the windows
            for (int i = 0; i < window_size - shift; i++)
            {
                packet_window[i] = packet_window[i + shift];
                recv_time[i] = recv_time[i + shift];
            }

            // set all the remaining in the window to false
            for (int i = window_size - shift; i < window_size; i++)
                packet_window[i] = false;
        }
    }
    listen_thread.detach();
    close(sockfd);

    return 0;
}