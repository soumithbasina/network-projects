#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
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
    int seqnum;
    char *data;
    time_stamp gen_time;
    time_stamp first_sent_time;
    time_stamp sent_time;
    time_stamp ack_time;
    int timeout;
    bool packet_sent;
    bool ack_recvd;
    bool required;
} packet_type;

// debug mode?
bool debug;
// seqn of the first packet in the buffer
int first_seqn;

// addresses and socket file descriptor
struct sockaddr_in servaddr, cliaddr;
int sockfd;

// time stamp of the start of the program
time_stamp prog_start;
// buffer to store packets, can store upto buffer_size only
vector<packet_type> buffer;
// mutex to prevent any race conditions for the buffer
mutex buffer_mutex;

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
void generatePackets(int packet_len, int max_buffer_size, int max_packets)
{
    static int curr_seqn = 0;

    if (curr_seqn == max_packets)
        return;

    char packet[packet_len] = {0};

    sprintf(packet, "%d ", curr_seqn);

    // initialise the packet_type struct
    packet_type tmp;

    tmp.attempts = 0;
    tmp.seqnum = curr_seqn;
    tmp.data = new char[packet_len];
    strcpy(tmp.data, packet);
    tmp.gen_time = current_time();
    tmp.packet_sent = false;
    tmp.ack_recvd = false;
    if (curr_seqn == 0)
        tmp.required = true;

    buffer_mutex.lock();

    if (buffer.size() < max_buffer_size)
    {
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

        buffer_mutex.lock();

        if (buffer.empty())
        {
            buffer_mutex.unlock();
            continue;
        }

        if (strcmp(acktype, "ACK") == 0)
        {
            if (first_seqn > seqn)
            {
                buffer_mutex.unlock();
                continue;
            }

            for (int i = 0; i < seqn - first_seqn; i++)
            {
                buffer[i].ack_time = current_time();
                buffer[i].ack_recvd = true;
                if (debug)
                {
                    packet_type temp = buffer[i];
                    int gtime = precise_elapsed_time(temp.gen_time, prog_start);
                    int rtt = precise_elapsed_time(temp.ack_time, temp.first_sent_time);

                    printf("%d: Time Generated: %d:%d RTT: %d:%d No. of Attempts: %d\n",
                           temp.seqnum, gtime / 1000, gtime % 1000, rtt / 1000, rtt % 1000, temp.attempts);
                }
            }
            buffer[seqn - first_seqn].required = true;
        }

        buffer_mutex.unlock();
    }
}

int main(int argc, char **argv)
{
    // receiver name or ip address
    char *recvip;
    // receiver's port number
    int recvport;
    // length of the packet in bytes
    int packet_len;
    // packet generation rate
    int packet_gen_rate;
    // max no. of packets to be sent
    int max_packets;
    // window size
    int window_size;
    // max buffer size
    int max_buffer_size;

    if (argc != 15 && argc != 16)
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
        else if (tmp == "-l")
            sscanf(argv[i + 1], "%d", &packet_len);
        else if (tmp == "-r")
            sscanf(argv[i + 1], "%d", &packet_gen_rate);
        else if (tmp == "-n")
            sscanf(argv[i + 1], "%d", &max_packets);
        else if (tmp == "-w")
            sscanf(argv[i + 1], "%d", &window_size);
        else if (tmp == "-b")
            sscanf(argv[i + 1], "%d", &max_buffer_size);
        else
        {
            printf("Illegal parameter %s, please try again\n", argv[i]);
            return -1;
        }
    }

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

    thread listen_thread(listenACK);

    // start thread to generate packets
    threadStart(std::bind(generatePackets, packet_len, max_buffer_size, max_packets), 1000 / packet_gen_rate);

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

        // unlock the buffer if the buffer is empty
        if (buffer.empty())
        {
            buffer_mutex.unlock();
            continue;
        }

        // update the first_seqn if buffer is not empty
        first_seqn = buffer[0].seqnum;

        packet_type temp = buffer[0];

        // enter if the packet has been dropped
        if (temp.required ||
            (!temp.ack_recvd && (elapsed_time(current_time(), temp.sent_time) > temp.timeout)))
        {
            // resend the entire window
            for (int i = 0; i < window_size; i++)
            {
                if (buffer.empty() || i >= buffer.size())
                {
                    break;
                }

                // abort sending if a packet has exceeded 5 attempts
                if (buffer[i].attempts > 5)
                {
                    printf("A packet has exceeded 5 attempts, aborting...\n");
                    listen_thread.detach();
                    close(sockfd);
                    return 1;
                }

                buffer[i].attempts++;
                buffer[i].packet_sent = true;
                buffer[i].required = false;
                buffer[i].ack_recvd = false;
                // timeout is 2 * rtt after 10 packets and 100 ms before
                buffer[i].timeout = sent_packets > 10 ? 2 * cumul_trip_time / (1000 * sent_packets) : 100;
                if (buffer[i].attempts == 1)
                {
                    buffer[i].first_sent_time = current_time();
                }
                buffer[i].sent_time = current_time();
                transmission_count++;

                sendto(sockfd, buffer[i].data, packet_len, 0,
                       (SA *)&servaddr, sizeof(servaddr));
            }
        }

        buffer_mutex.unlock();
    }

    printf("All %d packets have been acknowledged!\n", max_packets);
    printf("Packet gen. rate: %d\n", packet_gen_rate);
    printf("Max. Packet length: %d\n", packet_len);
    printf("Retransmission rate: %f\n", (float)transmission_count / (float)sent_packets);
    printf("Average RTT: %f us\n", (float)cumul_trip_time / (float)sent_packets);

    listen_thread.detach();

    close(sockfd);

    return 0;
}