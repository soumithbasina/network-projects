#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define SA struct sockaddr

int main(int argc, char **argv)
{
    // second argument is the port number
    int port = atoi(argv[2]);

    // socket code begins
    int sockfd, connfd;
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

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(argv[1]); // first argument is the ip address
    servaddr.sin_port = htons(port);

    // connect client socket to server socket
    if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) != 0)
    {
        printf("Connection to the Server failed\n");
        exit(0);
    }
    else
        printf("Connected to the Server\n");

    // socket code ends

    char input[60];
    char cmd[10];
    char userid[50];
    char buf[50];

    while (1)
    {
        printf("Main-Prompt> ");
        fgets(input, 60, stdin);

        // getting rid of the newline character from fgets
        for (int i = 0; i < 60; i++)
        {
            if (input[i] == '\n')
            {
                input[i] = '\0';
                break;
            }
        }

        // extracting the actual command from the input
        for (int i = 0; i < 10; i++)
        {
            if (input[i] == ' ' || input[i] == '\0')
            {
                cmd[i] = '\0';
                break;
            }
            cmd[i] = input[i];
        }

        bzero(buf, 50);

        if (strcmp(cmd, "Listusers") == 0)
        {
            strcpy(buf, "LSTU");
            write(sockfd, buf, sizeof(buf));

            // counter for the number of users
            int n_users = 0;
            while (1)
            {
                bzero(buf, sizeof(buf));
                read(sockfd, buf, sizeof(buf));
                if (strcmp(buf, "##END##") == 0)
                {
                    // if the counter has not incremented once,
                    // there are no users
                    if (n_users == 0)
                        printf("No users found");
                    break;
                }
                printf("%s ", buf);
                n_users++;
            }
            printf("\n");
        }

        else if (strcmp(cmd, "Quit") == 0)
        {
            printf("Quitting...\n");
            strcpy(buf, "QUIT");
            write(sockfd, buf, sizeof(buf));
            break;
        }

        else
        {
            char temp_userid[45];
            memmove(temp_userid, input + 8, 45);

            if (strcmp(cmd, "Adduser") == 0)
            {
                strcpy(buf, "ADDU ");
                strcat(buf, temp_userid);
                write(sockfd, buf, sizeof(buf));
                bzero(buf, sizeof(buf));
                read(sockfd, buf, sizeof(buf));

                // if it receives ##ERR##, the user already exists
                if (strcmp(buf, "##ERR##") == 0)
                {
                    printf("User already exists, pick a unique name\n");
                }

                // if it receives ##OK##, the user is created successfully
                else if (strcmp(buf, "##OK##") == 0)
                {
                    printf("User created successfully!\n");
                }
            }

            else if (strcmp(cmd, "SetUser") == 0)
            {
                strcpy(buf, "USER ");
                strcat(buf, temp_userid);
                write(sockfd, buf, sizeof(buf));
                bzero(buf, sizeof(buf));
                read(sockfd, buf, sizeof(buf));

                // if it receives ##ERR##, the user does not exist
                if (strcmp(buf, "##ERR##") == 0)
                {
                    printf("User does not exist, try again\n");
                }

                // else, it exists
                // the program enters another infinite loop for the subprompt
                // it only accepts 4 commands in this loop
                else
                {
                    printf("User exists, setting current user to %s\n", temp_userid);
                    printf("You have %s messages\n", buf);

                    strcpy(userid, temp_userid);
                    char subbuf[200];
                    char subinput[60];
                    char subcmd[5];

                    while (1)
                    {
                        bzero(subbuf, sizeof(subbuf));

                        printf("Sub-Prompt-%s> ", userid);
                        fgets(subinput, 60, stdin);

                        for (int i = 0; i < 60; i++)
                        {
                            if (subinput[i] == '\n')
                            {
                                subinput[i] = '\0';
                                break;
                            }
                        }
                        memmove(subcmd, subinput, 4);
                        subcmd[4] = '\0';
                        memmove(temp_userid, subinput + 5, 45);

                        for (int i = 0; i < 60; i++)
                        {
                            if (subinput[i] == '\n')
                            {
                                subinput[i] = '\0';
                                break;
                            }
                        }

                        if (strcmp(subinput, "Read") == 0)
                        {
                            strcpy(subbuf, "READM");
                            write(sockfd, subbuf, sizeof(subbuf));
                            bzero(subbuf, sizeof(subbuf));
                            read(sockfd, subbuf, sizeof(subbuf));

                            // if ##ERR## is received, there are no mails
                            if (strcmp(subbuf, "##ERR##") == 0)
                            {
                                printf("No mails found\n");
                            }

                            // else we read the lines in an infinite loop
                            // it terminates when it encounters ###
                            else
                            {
                                while (1)
                                {
                                    printf("%s", subbuf);
                                    read(sockfd, subbuf, sizeof(subbuf));

                                    if (strcmp(subbuf, "###\n") == 0)
                                        break;
                                }
                            }
                        }

                        else if (strcmp(subinput, "Delete") == 0)
                        {
                            strcpy(subbuf, "DELM");
                            write(sockfd, subbuf, sizeof(subbuf));
                            bzero(subbuf, sizeof(subbuf));
                            read(sockfd, subbuf, sizeof(subbuf));

                            // prints messages corresponding to the error codes
                            if (strcmp(subbuf, "##ERR##") == 0)
                            {
                                printf("No mails to delete\n");
                            }
                            else if (strcmp(subbuf, "##OK##") == 0)
                            {
                                printf("Mail has been deleted succesfully\n");
                            }
                        }

                        else if (strcmp(subinput, "Done") == 0)
                        {
                            strcpy(subbuf, "DONEU");
                            write(sockfd, subbuf, sizeof(subbuf));

                            printf("User \"%s\" closed\n", userid);
                            break;
                        }

                        else if (strcmp(subcmd, "Send") == 0)
                        {
                            char *message = NULL;
                            size_t sz;
                            strcpy(subbuf, "SEND ");
                            strcat(subbuf, temp_userid);
                            write(sockfd, subbuf, sizeof(subbuf));

                            // takes lines from stdin and sends to the server
                            // take the first line as subject
                            printf("Enter the Subject: ");
                            getline(&message, &sz, stdin);
                            write(sockfd, message, 150);

                            // takes multiple lines as the message
                            // loop terminates when it encounters ###
                            printf("Enter the Message: \n");
                            while (1)
                            {
                                getline(&message, &sz, stdin);
                                write(sockfd, message, 150);
                                if (strcmp(message, "###\n") == 0)
                                    break;
                            }

                            printf("Message delivered successfully\n");
                        }
                        else
                        {
                            // if no command is given, continue
                            if (strcmp(subinput, "") == 0)
                                continue;

                            // else it is an illegal command
                            printf("Illegal command\n");
                        }
                    }
                }
            }

            else
            {
                // if no command is given, continue
                if (strcmp(input, "") == 0)
                    continue;

                // else it is an illegal command
                printf("Illegal command\n");
            }
        }
    }

    return 0;
}