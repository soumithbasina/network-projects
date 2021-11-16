#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <chrono>
#include <ctime>
#include <thread>
#include <mutex>
#include <vector>
#include <functional>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define SA struct sockaddr
#define time_stamp chrono::high_resolution_clock::time_point
#define current_time chrono::high_resolution_clock::now
#define elapsed_time(end, start) chrono::duration_cast<chrono::milliseconds>(end - start).count()
#define precise_elapsed_time(end, start) chrono::duration_cast<chrono::microseconds>(end - start).count()

using namespace std;

// struct to store all the information about the packet
typedef struct packet_type
{
    int attempts;
    unsigned int seqnum;
    char *data;
    int packet_len;
    time_stamp gen_time;
    time_stamp first_sent_time;
    time_stamp sent_time;
    time_stamp ack_time;
    int timeout;
    bool packet_sent;
    bool ack_recvd;
} packet_type;

// seqn of the first packet in the buffer
int first_seqn;
// buffer to store packets, can store upto buffer_size only
vector<packet_type> buffer;
// mutex to prevent any race conditions for the buffer
mutex buffer_mutex;
// sequence number field length
int seqnfl;

// debug mode?
bool debug = false;

// addresses and socket file descriptor
struct sockaddr_in servaddr, cliaddr;
int sockfd;

// time stamp of the start of the program
time_stamp prog_start;

// executes func() for every interval ms
// creates a new thread and detaches it
void threadStart(function<void()> func, unsigned int interval)
{
    thread([func, interval]() {
        while (true)
        {
            auto x = chrono::steady_clock::now() + chrono::milliseconds(interval);
            func();
            this_thread::sleep_until(x);
        }
    }).detach();
}

// generates a packet everytime it runs, only adds to the buffer if there is space
void generatePackets(int max_packet_len, int buffer_size, int seqnfl, int max_packets)
{
    // generate a random packet length between 40 and max_packet_len
    int plen = (rand() % (max_packet_len - 39)) + 40;

    // current sequence number of packet to be sent to the buffer
    static unsigned int curr_seqn = 0;

    if (curr_seqn == max_packets)
        return;

    unsigned int seqn = (curr_seqn << (32 - seqnfl)) >> (32 - seqnfl);

    char packet[plen] = {0};

    sprintf(packet, "%d ", seqn);

    // initialise the packet_type struct
    packet_type tmp;

    tmp.attempts = 0;
    tmp.seqnum = seqn;
    tmp.data = new char[plen];
    strcpy(tmp.data, packet);
    tmp.packet_len = plen;
    tmp.gen_time = current_time();
    tmp.packet_sent = false;
    tmp.ack_recvd = false;

    buffer_mutex.lock();
    // push to the buffer only if the buffer size is less than the max
    if (buffer.size() < buffer_size)
    {
        // if the buffer is empty, initialise the first_seqn to seqnum
        if (buffer.empty())
        {
            first_seqn = tmp.seqnum;
        }
        buffer.push_back(tmp);
        curr_seqn++;
    }
    else
    {
        delete tmp.data;
    }
    buffer_mutex.unlock();

    return;
}

// runs as an infinite loop to listen for the ACK packets
void listenACK()
{
    // ACK packet received
    char ack[10];
    // type of ACK packet, header of the packet
    char acktype[5];
    // seqn in the packet
    int seqn;
    while (true)
    {
        socklen_t saddrsize;
        int ii = recvfrom(sockfd, ack, 10, MSG_WAITALL, (SA *)&servaddr, &saddrsize);
        ack[ii] = '\0';

        sscanf(ack, "%s %d", acktype, &seqn);

        // if seqn < first_seqn, overflow of the seqnum field occured
        // calculate for the relative position in the buffer in that case
        int idx = seqn < first_seqn ? pow(2, seqnfl) - first_seqn + seqn : seqn - first_seqn;

        buffer_mutex.lock();
        if (buffer.empty())
            continue;

        if (strcmp(acktype, "ACK") == 0)
        {
            buffer[idx].ack_recvd = true;
            buffer[idx].ack_time = current_time();
            if (debug)
            {
                packet_type temp = buffer[idx];
                int gtime = precise_elapsed_time(temp.gen_time, prog_start);
                int rtt = precise_elapsed_time(temp.ack_time, temp.first_sent_time);

                printf("%d: Time Generated: %d:%d RTT: %d:%d No. of Attempts: %d\n",
                       temp.seqnum, gtime / 1000, gtime % 1000, rtt / 1000, rtt % 1000, temp.attempts);
            }
        }

        buffer_mutex.unlock();
    }
}

int main(int argc, char **argv)
{
    // receiver's port number
    int recvport;
    // max. packet length
    int max_packet_len;
    // packet gen. rate
    int packet_gen_rate;
    // max. number of packets
    int max_packets;
    // window size
    int window_size;
    // buffer size
    int buffer_size;
    // receiver name or ip address
    char *recvip;

    if (argc != 17 && argc != 18)
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
        else if (tmp == "-s")
            recvip = argv[i + 1];
        else if (tmp == "-p")
            sscanf(argv[i + 1], "%d", &recvport);
        else if (tmp == "-n")
            sscanf(argv[i + 1], "%d", &seqnfl);
        else if (tmp == "-L")
            sscanf(argv[i + 1], "%d", &max_packet_len);
        else if (tmp == "-R")
            sscanf(argv[i + 1], "%d", &packet_gen_rate);
        else if (tmp == "-N")
            sscanf(argv[i + 1], "%d", &max_packets);
        else if (tmp == "-W")
            sscanf(argv[i + 1], "%d", &window_size);
        else if (tmp == "-B")
            sscanf(argv[i + 1], "%d", &buffer_size);
        else
        {
            printf("Illegal parameter %s, please try again\n", argv[i]);
            return -1;
        }
    }

    // initialise the random seed
    srand(time(0));

    // socket code begins
    struct hostent *dest_hnet;

    dest_hnet = gethostbyname(recvip);
    if (!dest_hnet)
    {
        printf("Unknown host\n");
        exit(0);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        printf("Socket creation failed\n");
        exit(0);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    bcopy(dest_hnet->h_addr, (char *)&servaddr.sin_addr,
          dest_hnet->h_length);
    servaddr.sin_port = htons(recvport);

    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr = INADDR_ANY;
    cliaddr.sin_port = htons(0);

    if ((bind(sockfd, (SA *)&cliaddr, sizeof(cliaddr))) != 0)
    {
        printf("Socket bind failed\n");
        exit(0);
    }
    // socket code ends

    prog_start = current_time();

    // start thread to listen to ack messages
    thread listen_thread(listenACK);

    // start thread to generate packets
    threadStart(std::bind(generatePackets, max_packet_len, buffer_size, seqnfl, max_packets), 1000 / packet_gen_rate);

    // number of packets that are sent and have been acknowledged
    int sent_packets = 0;
    // number of transmissions (including retransmissions) done
    int transmission_count = 0;
    // cumulative round trip time of all the acknowledged packets
    // divide by the number of acknowledged packets to get average RTT
    long int cumul_trip_time = 0;

    while (sent_packets < max_packets)
    {
        buffer_mutex.lock();

        // remove the ACKed packets from the buffer
        while (!buffer.empty() && buffer[0].ack_recvd)
        {
            cumul_trip_time += precise_elapsed_time(buffer[0].ack_time, buffer[0].first_sent_time);
            buffer.erase(buffer.begin());
            sent_packets++;
        }

        // update the first_seqn if buffer is not empty
        if (!buffer.empty())
        {
            first_seqn = buffer[0].seqnum;
        }

        buffer_mutex.unlock();

        for (int i = 0; i < window_size; i++)
        {
            buffer_mutex.lock();

            if (buffer.empty() || i >= buffer.size())
            {
                buffer_mutex.unlock();
                break;
            }
            packet_type temp = buffer[i];

            // if the packet has been dropped or has not been sent
            if (!temp.packet_sent ||
                (!temp.ack_recvd && (elapsed_time(current_time(), temp.sent_time) > temp.timeout)))
            {
                // abort if a packet has exceeded 10 attempts
                if (temp.attempts > 10)
                {
                    printf("A packet has exceeded 10 attempts, aborting...\n");
                    listen_thread.detach();
                    close(sockfd);
                    return 1;
                }

                buffer[i].attempts++;
                buffer[i].packet_sent = true;
                // timeout is 2 * rtt after 10 packets and 300 ms before
                buffer[i].timeout = sent_packets > 10 ? 2 * cumul_trip_time / (1000 * sent_packets) : 300;
                buffer[i].sent_time = current_time();
                if (buffer[i].attempts == 1)
                {
                    buffer[i].first_sent_time = buffer[i].sent_time;
                }
                transmission_count++;

                sendto(sockfd, temp.data, temp.packet_len, 0,
                       (SA *)&servaddr, sizeof(servaddr));
            }

            buffer_mutex.unlock();
        }
    }

    printf("All %d packets have been acknowledged!\n", max_packets);
    printf("Packet gen. rate: %d\n", packet_gen_rate);
    printf("Max. Packet length: %d\n", max_packet_len);
    printf("Retransmission rate: %f\n", (float)transmission_count / (float)sent_packets);
    printf("Average RTT: %f us\n", (float)cumul_trip_time / (float)sent_packets);

    listen_thread.detach();
    close(sockfd);

    return 0;
}