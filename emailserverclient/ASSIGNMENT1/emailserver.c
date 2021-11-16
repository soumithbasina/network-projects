#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define SA struct sockaddr

int main(int argc, char **argv)
{

    int port = atoi(argv[1]);
    // socket code begins

    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket creation
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("Socket creation failed\n");
        exit(0);
    }
    else
        printf("Socket created successfully\n");

    bzero(&servaddr, sizeof(servaddr));

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
    else
        printf("Socket bind successful\n");

    if ((listen(sockfd, 5)) != 0)
    {
        printf("Listen failed\n");
        exit(0);
    }
    else
        printf("Server listening\n");
    len = sizeof(cli);

    // accept the data packet from client and verification
    connfd = accept(sockfd, (SA *)&cli, &len);

    if (connfd < 0)
    {
        printf("Server accept failed\n");
        exit(0);
    }
    else
        printf("Server accepted the client\n");

    // socket code ends

    char cmd[200];
    char firstfive[6];
    char userid[45];
    char status[10];

    FILE *spoolptr = NULL;
    int mail_count = 0;
    int curr_mail = 0;

    while (1)
    {
        bzero(cmd, sizeof(cmd));
        bzero(status, sizeof(status));

        read(connfd, cmd, sizeof(cmd));
        printf("%s\n", cmd);

        for (int i = 0; i < 200; i++)
        {
            if (cmd[i] == '\n')
            {
                cmd[i] = '\0';
                break;
            }
        }

        // Extracting the actual command from cmd
        for (int i = 0; i < 6; i++)
        {
            if (cmd[i] == ' ' || cmd[i] == '\0')
            {
                firstfive[i] = '\0';
                break;
            }
            firstfive[i] = cmd[i];
        }

        if (strcmp(firstfive, "LSTU") == 0)
        {
            DIR *d;
            struct dirent *dir;
            d = opendir("./MAILSERVER");

            // message is sent for every user
            // this allows for any no. of users to be created
            if (d)
            {
                while ((dir = readdir(d)) != NULL)
                {
                    if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                    {
                        continue;
                    }
                    // max size of user name should be 49
                    write(connfd, dir->d_name, 50);
                }
                closedir(d);
                strcpy(status, "##END##");
                write(connfd, status, sizeof(status));
            }
        }

        else if (strcmp(firstfive, "READM") == 0)
        {

            if (mail_count == 0)
            {
                strcpy(status, "##ERR##");
                write(connfd, status, sizeof(status));
            }
            else
            {
                char *buf = NULL;
                size_t sz;

                // store a line in the email in buf
                while (getline(&buf, &sz, spoolptr) > 0)
                {
                    // max length of a line should be 200
                    // sizeof(buf) can't be used as it is defined as a pointer
                    write(connfd, buf, 200);
                    if (strcmp(buf, "###\n") == 0)
                    {
                        curr_mail++;
                        if (curr_mail > mail_count)
                        {
                            rewind(spoolptr);
                            curr_mail = 1;
                        }
                        break;
                    }
                }
            }
        }

        else if (strcmp(firstfive, "DELM") == 0)
        {
            if (mail_count == 0)
            {
                strcpy(status, "##ERR##");
                write(connfd, status, sizeof(status));
            }
            else
            {
                // rewind the spoolptr to the start of the spool file
                rewind(spoolptr);

                //renaming the old file to <userid>tmp
                //and creating a new file named <userid>
                char path_old[60] = "./MAILSERVER/";
                strcat(path_old, userid);
                char path_new[60];
                strcpy(path_new, path_old);
                strcat(path_old, "tmp");
                rename(path_new, path_old);

                //opening the new file in append mode
                int temp_curr_mail = 1;
                FILE *fp = fopen(path_new, "a");

                // write the contents of all the mails from old to new
                // except for the one that is being deleted
                char *buf = NULL;
                size_t sz;
                while (getline(&buf, &sz, spoolptr) > 0)
                {
                    if (temp_curr_mail != curr_mail)
                    {
                        fprintf(fp, "%s", buf);
                    }
                    if (strcmp(buf, "###\n") == 0)
                    {
                        temp_curr_mail++;
                    }
                }
                fclose(fp);
                fclose(spoolptr);
                remove(path_old);

                // open the new file as spoolptr
                spoolptr = fopen(path_new, "r+");

                // setting curr_mail to the correct value
                if (mail_count == 1)
                    curr_mail = 0;
                else if (mail_count == curr_mail)
                    curr_mail = 1;

                // bring spoolptr to the curr_mail position
                else if (curr_mail != 1)
                {
                    temp_curr_mail = 1;
                    while (temp_curr_mail != curr_mail && getline(&buf, &sz, spoolptr) > 0)
                    {
                        if (strcmp(buf, "###\n") == 0)
                        {
                            temp_curr_mail++;
                        }
                    }
                }

                // decrement the mail count
                mail_count--;
                strcpy(status, "##OK##");
                write(connfd, status, sizeof(status));
            }
        }

        else if (strcmp(firstfive, "DONEU") == 0)
        {
            fclose(spoolptr);
        }

        else if (strcmp(firstfive, "QUIT") == 0)
        {
            close(sockfd);
            break;
        }

        else
        {
            char temp_userid[45];

            memmove(temp_userid, cmd + 5, 45);

            if (strcmp(firstfive, "ADDU") == 0)
            {
                char path[60] = "./MAILSERVER/";
                strcat(path, temp_userid);
                FILE *fp = fopen(path, "wx");
                // the file already exists
                if (fp == NULL)
                {
                    strcpy(status, "##ERR##");
                    write(connfd, status, sizeof(status));
                }
                // the file doesn't exist, so it is created
                else
                {
                    strcpy(status, "##OK##");
                    write(connfd, status, sizeof(status));
                    fclose(fp);
                }
            }

            else if (strcmp(firstfive, "USER") == 0)
            {
                char path[60] = "./MAILSERVER/";
                strcat(path, temp_userid);
                spoolptr = fopen(path, "r+");
                // the file doesn't exist
                if (spoolptr == NULL)
                {
                    strcpy(status, "##ERR##");
                    write(connfd, status, sizeof(status));
                }
                // the file exists, so it is opened
                else
                {
                    strcpy(userid, temp_userid);
                    char *buf = NULL;
                    size_t sz;
                    // count the number of mails in the spool file
                    // to display on the client
                    mail_count = 0;
                    while (getline(&buf, &sz, spoolptr) > 0)
                    {
                        if (strcmp(buf, "###\n") == 0)
                            mail_count++;
                    }
                    if (mail_count > 0)
                        curr_mail = 1;

                    rewind(spoolptr);
                    sprintf(status, "%d", mail_count);
                    write(connfd, status, sizeof(status));
                }
            }

            else if (strcmp(firstfive, "SEND") == 0)
            {
                char path[60] = "./MAILSERVER/";
                strcat(path, temp_userid);
                FILE *fp = fopen(path, "r+");

                if (fp == NULL)
                {
                    printf("User ID does not exist, try again\n");
                }

                else
                {
                    fclose(fp);
                    fp = fopen(path, "a");

                    char message[150];

                    fprintf(fp, "From: %s\n", userid);
                    fprintf(fp, "To: %s\n", temp_userid);

                    // For "IST" to be printed after the time
                    time_t t = time(NULL);
                    char curr_time[30];
                    strcpy(curr_time, asctime(localtime(&t)));
                    memmove(curr_time + 24, curr_time + 20, 5);
                    memcpy(curr_time + 20, "IST ", 4);
                    curr_time[29] = '\0';

                    fprintf(fp, "Date: %s", curr_time);

                    // read the subject first
                    bzero(message, sizeof(message));
                    read(connfd, message, sizeof(message));
                    fprintf(fp, "Subject: %s", message);

                    // read the lines and fprintf into the spool file
                    // until "###" is received
                    fprintf(fp, "Contents\n");
                    while (1)
                    {
                        read(connfd, message, sizeof(message));
                        fprintf(fp, "%s", message);
                        if (strcmp(message, "###\n") == 0)
                            break;
                    }
                    fclose(fp);
                }
            }
        }
    }
    return 0;
}