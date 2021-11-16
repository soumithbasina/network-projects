// Implements the Open Shortest Path First (OSPF) routing protocol.
// Run separate instances to emulate different routers.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <string>
#include <sstream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <functional>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define SA struct sockaddr

using namespace std;

// use for indexing into the adjacency matrix
int pos(int i, int j, int n)
{
    return i * n + j;
}

// returns a random number between the pair of ints
int randomInRange(pair<int, int> range)
{
    return rand() % (range.second - range.first + 1) + range.first;
}

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

// sends HELLO packet to all the neighbours
void sendHello(unordered_map<int, pair<int, int>> links, int id, int sockfd)
{
    // create the HELLO packet
    string str = "HELLO ";
    str.append(to_string(id));

    // iterate through all of the neighbours and send them the HELLO packet.
    for (auto &i : links)
    {
        struct sockaddr_in servaddr;
        int port = 10000 + i.first;

        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port);
        servaddr.sin_addr.s_addr = INADDR_ANY;

        sendto(sockfd, str.c_str(), strlen(str.c_str()),
               MSG_CONFIRM, (SA *)&servaddr, sizeof(servaddr));
    }
    printf("HELLO from %d sent!\n", id);

    return;
}

// creates an LSA packet with all neighbour costs and broadcasts it
void sendLSA(unordered_map<int, pair<int, int>> links, int *link_costs,
             int id, int sockfd, int nrouts, int *seqn)
{
    if (*seqn == 0)
    {
        (*seqn)++;
        return;
    }
    char lsa[1024];
    char temp1[512] = {0};
    // no. of neighbors that sent hello reply
    int nrecv = 0;

    // creating packet with all the costs
    for (int i = 0; i < nrouts; i++)
    {
        char temp2[512];
        if (link_costs[pos(id, i, nrouts)] != -1)
        {
            sprintf(temp2, "%d %d ", i, link_costs[pos(id, i, nrouts)]);
            strncat(temp1, temp2, 512);
            nrecv++;
        }
    }
    sprintf(lsa, "LSA %d %d %d %s", id, *seqn, nrecv, temp1);

    // sending the packet to all the neighbours
    for (auto &i : links)
    {
        struct sockaddr_in servaddr;
        int port = 10000 + i.first;

        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port);
        servaddr.sin_addr.s_addr = INADDR_ANY;

        sendto(sockfd, lsa, strlen(lsa),
               MSG_CONFIRM, (SA *)&servaddr, sizeof(servaddr));
    }
    // incrementing the sequence number
    (*seqn)++;

    printf("LSA from %d sent!\n", id);
    return;
}

// return the index of the router with the least cost that hasn't been visited yet
int pickMin(int *cost, bool *visited, int nrouts)
{
    int min_cost = INT_MAX;
    int min_idx;

    for (int i = 0; i < nrouts; i++)
    {
        if (cost[i] <= min_cost && !visited[i])
        {
            min_cost = cost[i];
            min_idx = i;
        }
    }

    return min_idx;
}

// return string with path to dest
string pathtoDest(int *parent, int dest)
{
    // dest is the source
    if (parent[dest] == -1)
    {
        return to_string(dest);
    }

    return pathtoDest(parent, parent[dest]) + to_string(dest);
}

void calculatePaths(int *link_costs, int id, int nrouts, int *ncal, int si, string out)
{
    // avoid calculating at time 0
    if (*ncal == 0)
    {
        (*ncal)++;
        return;
    }

    // cost to the router
    int cost[nrouts];
    // parent of the router in the path
    int parent[nrouts];
    // has the router been visited yet
    bool visited[nrouts];

    for (int i = 0; i < nrouts; i++)
    {
        cost[i] = INT_MAX;
        parent[i] = -1;
        visited[i] = false;
    }

    // cost to the source itself is 0
    cost[id] = 0;

    for (int i = 0; i < nrouts - 1; i++)
    {
        // pick the router with the least cost
        int min = pickMin(cost, visited, nrouts);

        // mark as visited
        visited[min] = true;

        // go over all the neighbours and check if shorter path is available
        // change the parent of the neighbour if available
        for (int j = 0; j < nrouts; j++)
        {
            if (cost[min] + link_costs[pos(min, j, nrouts)] < cost[j] &&
                link_costs[pos(min, j, nrouts)] != -1 && !visited[j])
            {
                cost[j] = cost[min] + link_costs[pos(min, j, nrouts)];
                parent[j] = min;
            }
        }
    }

    // print the routing table out to the outfile
    FILE *outfile = fopen(out.c_str(), "a");

    fprintf(outfile, "Routing Table for Node no. %d at time %d\n", id, (*ncal) * si);
    fprintf(outfile, "Destination\tPath\tCost\n");

    for (int i = 0; i < nrouts; i++)
    {
        if (i == id)
            continue;

        fprintf(outfile, "%d\t\t%s\t%d\n", i, pathtoDest(parent, i).c_str(), cost[i]);
    }

    fclose(outfile);

    printf("Paths from %d calculated!\n", id);
    (*ncal)++;
    return;
}

int main(int argc, char **argv)
{
    if (argc != 13)
    {
        printf("Incorrect no. of arguments, please try again\n");
        return -1;
    }

    // node identifier
    int id;
    // filenames of infile and outfile
    string in, out;
    // hi - hello interval
    // li - LSA interval
    // si - SPF interval
    double hi, li, si;

    // code for taking command-line parameters
    for (int i = 1; i < 12; i = i + 2)
    {
        string tmp = argv[i];

        if (tmp == "-i")
            sscanf(argv[i + 1], "%d", &id);
        else if (tmp == "-h")
            sscanf(argv[i + 1], "%lf", &hi);
        else if (tmp == "-a")
            sscanf(argv[i + 1], "%lf", &li);
        else if (tmp == "-s")
            sscanf(argv[i + 1], "%lf", &si);
        else if (tmp == "-f")
            in = argv[i + 1];
        else if (tmp == "-o")
            out = argv[i + 1];
        else
        {
            printf("Illegal parameter %s, please try again\n", argv[i]);
            return -1;
        }
    }

    out = out + '-' + to_string(id) + ".txt";
    FILE *infile = fopen(in.c_str(), "r");
    // neighbours of the router, along with min and max costs
    // <node, <cmin, cmax>>
    unordered_map<int, pair<int, int>> links;

    int nrouts, nlinks;

    // takes no. of routers and links from the first line of the file
    // and stores all the neighbours and min - max costs in the links vector
    fscanf(infile, "%d %d\n", &nrouts, &nlinks);
    for (int i = 0; i < nlinks; i++)
    {
        int t1, t2, tmin, tmax;
        fscanf(infile, "%d %d %d %d\n", &t1, &t2, &tmin, &tmax);

        if (t1 == id)
            links[t2] = make_pair(tmin, tmax);
        else if (t2 == id)
            links[t1] = make_pair(tmin, tmax);
        else
            continue;
    }
    fclose(infile);

    // socket code begins
    // port number is initialised as defined (10000 + router id)
    int port = 10000 + id;

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    // socket creation
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        printf("Socket creation failed\n");
        exit(0);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // assigning IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    // binding newly created socket to IP and verify
    if ((bind(sockfd, (SA *)&servaddr, sizeof(servaddr))) != 0)
    {
        printf("Socket bind failed\n");
        exit(0);
    }
    // socket code ends

    // adjacency matrix for representing the network and link costs
    // link_costs[nrouts * nrouts] = {-1}
    int *link_costs = (int *)malloc(sizeof(int) * nrouts * nrouts);
    memset(link_costs, -1, nrouts * nrouts * sizeof(int));

    // sequence number of the router, starts from 1
    int seqn = 0;
    // no. of times calculatePaths has been called
    int ncal = 0;

    // giving time for all the files to be created properly
    sleep(2);

    // starting the message threads to repeat execution at given intervals
    threadStart(std::bind(sendHello, links, id, sockfd), hi * 1000);
    threadStart(std::bind(sendLSA, links, link_costs, id, sockfd, nrouts, &seqn), li * 1000);
    // starting the calculation thread
    threadStart(std::bind(calculatePaths, link_costs, id, nrouts, &ncal, si, out), si * 1000);

    // received packet
    char buffer[1024];
    // header of the packet
    string packettype;
    // sequence number of the last received LSA packet of all routers
    int seqnum[nrouts] = {0};
    int len = sizeof(cliaddr);
    // source of the packet
    int srcid;

    // initialise random seed
    srand(time(NULL));

    // go into an infinite loop for receiving packets
    while (true)
    {
        memset(&cliaddr, 0, sizeof(cliaddr));
        int ii = recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL,
                          (SA *)&cliaddr, (socklen_t *)&len);
        buffer[ii] = '\0';
        string packet = buffer;
        stringstream ss(packet);

        ss >> packettype;

        // send a random number in cmin - cmax as the cost to the router
        if (packettype == "HELLO")
        {
            ss >> srcid;
            char message[1024];
            sprintf(message, "HELLOREPLY %d %d %d", id, srcid, randomInRange(links[srcid]));
            sendto(sockfd, message, sizeof(message),
                   MSG_CONFIRM, (SA *)&cliaddr, len);
        }
        // record the link cost in the link_costs adjacency matrix
        else if (packettype == "HELLOREPLY")
        {
            int tempid, cost;
            ss >> srcid >> tempid >> cost;
            link_costs[pos(tempid, srcid, nrouts)] = cost;
            link_costs[pos(srcid, tempid, nrouts)] = cost;
        }
        // record the link costs of all the neighbours of the source router
        else if (packettype == "LSA")
        {
            int recvseqn, nentries;
            ss >> srcid >> recvseqn >> nentries;

            // skip the packet if the sequence number of the received packet
            // is less than or equal to the last recorded sequence number
            // from the source router
            if (recvseqn <= seqnum[srcid])
                continue;

            // update the last recorded sequence number for the router
            seqnum[srcid] = recvseqn;

            for (int i = 0; i < nentries; i++)
            {
                int neigh, cost;
                ss >> neigh >> cost;
                link_costs[pos(neigh, srcid, nrouts)] = cost;
                link_costs[pos(srcid, neigh, nrouts)] = cost;
            }

            // broadcast this packet to neighbours except for the source
            for (auto &i : links)
            {
                if (i.first == srcid)
                    continue;
                struct sockaddr_in tempservaddr;
                int tempport = 10000 + i.first;

                tempservaddr.sin_family = AF_INET;
                tempservaddr.sin_port = htons(tempport);
                tempservaddr.sin_addr.s_addr = INADDR_ANY;

                sendto(sockfd, buffer, strlen(buffer),
                       MSG_CONFIRM, (SA *)&tempservaddr, sizeof(tempservaddr));
            }
        }
    }

    return 0;
}