#include <random>
#include <limits>
#include <math.h>
#include <stdio.h>

using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 15)
    {
        printf("Incorrect no. of arguments, please try again\n");
        return -1;
    }

    // input parameters from command line
    double ki, km, kn, kf, ps;
    int T;
    string out;

    for (int i = 1; i < 14; i = i + 2)
    {
        string tmp = argv[i];
        if (tmp == "-i")
            sscanf(argv[i + 1], "%lf", &ki);
        else if (tmp == "-m")
            sscanf(argv[i + 1], "%lf", &km);
        else if (tmp == "-n")
            sscanf(argv[i + 1], "%lf", &kn);
        else if (tmp == "-f")
            sscanf(argv[i + 1], "%lf", &kf);
        else if (tmp == "-s")
            sscanf(argv[i + 1], "%lf", &ps);
        else if (tmp == "-T")
            sscanf(argv[i + 1], "%d", &T);
        else if (tmp == "-o")
            out = argv[i + 1];
        else
        {
            printf("Illegal parameter %s, please try again\n", argv[i]);
            return -1;
        }
    }

    // check if all the parameters are within the bounds given in the problem
    if (!((ki >= 1 && ki <= 4) && (km >= 0.5 && km <= 2) && (kn >= 0.5 && kn <= 2) && (kf >= 0.1 && kf <= 0.5) && (ps > 0 && ps < 1)))
    {
        printf("One or more parameters are out of bounds. Please follow the bounds given in Assignment\n");
        return 0;
    }

    FILE *outfile = fopen(out.c_str(), "w");
    // congestion window size
    double cw = ki;
    fprintf(outfile, "%lf\n", cw);
    // receiver window size
    double rws = 1024.0;
    // threshold value for slow start, initial value is infinity
    double ssthr = numeric_limits<double>::max();
    // state == 1 - slow start
    // state == 2 - congestion avoidance
    int state = 1;
    // next segment to be sent
    int nextseq = 1;

    // random number generator
    random_device rd;
    default_random_engine generator(rd());
    uniform_real_distribution<double> distribution(0.0, 1.0);

    // ps indicates the probability of timing out
    while (nextseq <= T)
    {
        double rndnum = distribution(generator);

        // timeout
        if (rndnum < ps)
        {
            ssthr = cw / 2;
            cw = max(1.0, kf * cw);
            fprintf(outfile, "%lf\n", cw);
            // since the only case this happens is when kf == 0.5 due to the bounds
            if (cw == ssthr)
                state = 2;
            else
                state = 1;
        }

        else
        {
            if (state == 1)
            {
                nextseq++;
                cw = min(cw + km, rws);
                fprintf(outfile, "%lf\n", cw);
                // change to congestion avoidance if window size is
                // higher than threshold
                if (cw >= ssthr)
                    state = 2;
            }
            else if (state == 2)
            {
                nextseq++;
                cw = min(cw + kn / cw, rws);
                fprintf(outfile, "%lf\n", cw);
            }
        }
    }
    fclose(outfile);

    return 0;
}