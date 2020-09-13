
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct Node {
    int client_name;
    struct Node* next;
    char buf[1000];
    int bytes_in_buf;
};

char board[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
char who_starts = 'x';

void reset_board() {
    for (int i = 0; i<9; i++) {
	board[i] = (char)(49+i);
    }
}

int game_is_over()  /* returns winner, or ' ' for draw, or 0 for not over */
{
    int i, c;
    extern int allthree(int start, int offset);
    extern int isfull();

    for (i = 0; i < 3; i++)
        if ((c = allthree(i, 3)) || (c = allthree(i * 3, 1)))
            return(c);
    if ((c = allthree(0, 4)) || (c = allthree(2, 2)))
        return(c);
    if (isfull())
        return(' ');
    return(0);
}

int allthree(int start, int offset)
{
    if (board[start] > '9' && board[start] == board[start + offset]
            && board[start] == board[start + offset * 2])
        return(board[start]);
    return(0);
}

int isfull()
{
    int i;
    for (i = 0; i < 9; i++)
        if (board[i] < 'a')
            return(0);
    return(1);
}

void showboard(int fd)
{
    char buf[100], *bufp, *boardp;
    int col, row;

    for (bufp = buf, col = 0, boardp = board; col < 3; col++) {
        for (row = 0; row < 3; row++, bufp += 4)
            sprintf(bufp, " %c |", *boardp++);
        bufp -= 2;  // kill last " |"
        strcpy(bufp, "\r\n---+---+---\r\n");
        bufp = strchr(bufp, '\0');
    }
    if (write(fd, buf, bufp - buf) != bufp-buf)
        perror("write");
}

int main(int argc, char **argv)
{
    extern void write_to_all();
    setbuf(stdout, NULL);
    int c, fd, port = 3000, status = 0;
    //int fd;
    socklen_t size;
    struct sockaddr_in r, q;
    struct Node *head = NULL;
    char yourx[37] = "You now get to play! You are now x.\r\n";
    char youro[37] = "You now get to play! You are now o.\r\n";


    while ((c = getopt(argc, argv, "p:")) != EOF) {
        switch (c) {
        case 'p':
            port = atoi(optarg);
            break;
        default:
            status = 1;
        }
    }

    if (status || optind < argc) {
        fprintf(stderr, "usage: %s [-p port]\n", argv[0]);
        return(1);
    }

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return(1);
    }

    head = malloc(sizeof(struct Node));
    head->client_name = fd;

    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&r, sizeof r) < 0) {
        perror("bind");
        return(1);
    }

    if (listen(fd, 5)) {
        perror("listen");
        return(1);
    }

    fd_set fds;
    q.sin_family = AF_INET;
    char *whose_turn = "x";
    char *nums = "123456789";
    int whose_x = -1, whose_o = -1, max;
    struct Node *curr6;
    while (1) {
	FD_ZERO(&fds);
	/* find highest value for numfd and add everything in linkedlist into
	the fd set */
	struct Node *curr = head;
	max = head->client_name;
	while (curr) {
	    FD_SET(curr->client_name, &fds);

	    if (curr->client_name > max)
		max = curr->client_name;

	    curr = curr->next;
	}

	if (select(max+1, &fds, NULL, NULL, NULL) < 0) {
	    perror("select");
	    return(1);
	}

	// service all ready sockets
	curr = head;
	while (curr) {
	    if (FD_ISSET(curr->client_name, &fds)) {
		if (curr->client_name == fd) {
		    // new connection
		    size = sizeof q;
		    struct Node *new_head = malloc(sizeof(struct Node));
		    if (new_head == NULL)
			fprintf(stderr, "Out of memory!\n");
		    else {
		        new_head->bytes_in_buf = 0;
		        new_head->client_name = accept(fd, (struct sockaddr *)&q, &size);

		        if (new_head->client_name < 0) {
			    perror("accept");
		        } else {
			    printf("New connection from %s\n", inet_ntoa(q.sin_addr));
			    showboard(new_head->client_name);
			    char whose_turn_string[17] = "It is ";
			    strncat(whose_turn_string, whose_turn, 1);
			    strncat(whose_turn_string, "'s turn.\r\n", 10);

			    if (write(new_head->client_name, whose_turn_string, 17) != 17)
			        perror("write");

			    new_head->next = head;
			    head = new_head;
			    if (whose_x == -1) {
			        whose_x = new_head->client_name;

			        if (write(new_head->client_name, yourx, 37) != 37)
	                            perror("write");
			        printf("Client from %s is now x.\n", inet_ntoa(q.sin_addr));

			    } else if (whose_o == -1) {
			        whose_o = new_head->client_name;

	                        if (write(new_head->client_name, youro, 37) != 37)
	                            perror("write");

			        printf("Client from %s is now o.\n", inet_ntoa(q.sin_addr));
			    }
        		}
		    }
		} else {
		    // read from socket
		    int len = read(curr->client_name, curr->buf + curr->bytes_in_buf,
                             sizeof(curr->buf) - curr->bytes_in_buf - 1);
		    curr->bytes_in_buf += len;
                    curr->buf[curr->bytes_in_buf] = '\0';
		    if (len == 0 && (sizeof(curr->buf) - curr->bytes_in_buf - 1)!=0) {
			// if a client disconnects
			printf("%s has disconnected\n", inet_ntoa(q.sin_addr));
			if (curr->client_name == whose_x)
			    whose_x = -1;
			else if (curr->client_name == whose_o)
			    whose_o = -1;
			close(curr->client_name);
			// remove client from linked list
    			if (curr == head) {
        		    head = head->next;
    			} else {
			curr6 = head;
    			while (curr6->next != curr)
        		    curr6 = curr6->next;
    			curr6->next = curr6->next->next;
			}
			struct Node *temp = curr;
			curr = curr->next;
			free(temp);
			// check for spectators to fill spots
			curr6 = head;
			while (curr6) {
			    if (whose_x == -1 && curr6->client_name != fd && curr6->client_name != whose_o) {
				whose_x = curr6->client_name;
				if (write(curr6->client_name, yourx, 37) != 37)
	                            perror("write");
				printf("Client from %s is now x.\n", inet_ntoa(q.sin_addr));
			    } else if (whose_o == -1 && curr6->client_name != fd && curr6->client_name != whose_x) {
				whose_o = curr6->client_name;
				if (write(curr6->client_name, youro, 37) != 37)
                            	    perror("write");
				printf("Client from %s is now o.\n", inet_ntoa(q.sin_addr));
			    }
			    curr6 = curr6->next;
			}
			continue;
		    } else if (strchr(curr->buf, '\n') || (sizeof(curr->buf) - curr->bytes_in_buf - 1)==0) {
		    	curr->bytes_in_buf = 0;
			// if a player tries to make a move
			if (strlen(curr->buf) == 2 && strchr(nums, curr->buf[0]))
			{
			    char message1[16] = "";
			    int stat = 0; // 1 if valid move was made, 0 otherwise
			    // if x makes a move and it is x's turn
			    if (curr->client_name == whose_x && strncmp(whose_turn, "x", 1)==0) {
				// if x chooses an open spot
				if (board[atoi(curr->buf)-1] != 'x' &&
				    board[atoi(curr->buf)-1] != 'o') {
				    stat = 1;
				    whose_turn = "o";
				    board[atoi(curr->buf)-1] = 'x';
				    strncpy(message1, "x", 1);
				    printf("%s (x) makes move %s", inet_ntoa(q.sin_addr), curr->buf);
				// if x chooses an already taken spot
				} else {
				    printf("%s tries to make move %c, but that space is taken.\n",
				    inet_ntoa(q.sin_addr), curr->buf[0]);
				    if (write(curr->client_name, "That space is taken.\r\n", 22) != 22)
					perror("write");
				}
			    // if o makes a move and it is o's turn
			    } else if (curr->client_name == whose_o && strncmp(whose_turn, "o", 1)==0) {
				// if o chooses an open spot
				if (board[atoi(curr->buf)-1] != 'x' &&
                                    board[atoi(curr->buf)-1] != 'o') {
                                    stat = 1;
                                    whose_turn = "x";
                                    board[atoi(curr->buf)-1] = 'o';
                                    strncpy(message1, "o", 1);
                                    printf("%s (o) makes move %s", inet_ntoa(q.sin_addr), curr->buf);
                                // if o chooses an already taken spot
                                } else {
                                    printf("%s tries to make move %c, but that space is taken.\n",
                                    inet_ntoa(q.sin_addr), curr->buf[0]);
                                    if (write(curr->client_name, "That space is taken.\r\n", 22) != 22)
                                        perror("write");
                                }
			    // wrong person tried taking a turn
			    } else if (curr->client_name != whose_x && curr->client_name != whose_o) {
				printf("%s tried to make a move, but they are not playing\n", inet_ntoa(q.sin_addr));
				if (write(curr->client_name, "You can't make moves; you can only watch the game.\r\n", 52) != 52)
				    perror("write");
			    } else {
				printf("%s tries to make move %c, but it is not their turn.\n", inet_ntoa(q.sin_addr), curr->buf[0]);
				if (write(curr->client_name, "It is not your turn.\r\n", 22) != 22)
				    perror("write");
			    }
			    // print message "x/o makes move [1-9]" and "It is your/x's/o's turn."
			    if (stat==1) {
				strncat(message1, " makes move ", 12);
				strncat(message1, curr->buf, 1);
				strncat(message1, "\r\n", 2);

				char message2[17] = "It is ";
				strncat(message2, whose_turn, 1);
				strncat(message2, "'s turn.\r\n", 10);

				curr6 = head;
				while (curr6) {
				    if (curr6->client_name != fd) {
					if (write(curr6->client_name, message1, 16) != 16)
					    perror("write");

					showboard(curr6->client_name);

					if ((curr6->client_name == whose_x && strncmp(whose_turn, "x", 1)==0) ||
					    (curr6->client_name == whose_o && strncmp(whose_turn, "o", 1)==0)) {
					    if (write(curr6->client_name, "It is your turn.\r\n", 18) != 18)
						perror("write");
					} else {
					    if (write(curr6->client_name, message2, 17) != 17) 
                                                perror("write");
					}
				    }
				    curr6 = curr6->next;
				}
				int w;
				//if game is over
				if ((w = game_is_over()) != 0) {
				    curr6 = head;
				    printf("Game over!\n");

				    if (w == ' ')
					printf("It is a draw.\n");
				    else
					printf("%c wins.\n", (char)w);

				    int temp = whose_x;
                                    whose_x = whose_o;
                                    whose_o = temp;

				    while (curr6) {
					if (curr6->client_name != fd) {

					    if (write(curr6->client_name, "Game over!\r\n", 12) != 12)
						perror("write");

					    showboard(curr6->client_name);

					    if (w==' ') {
						if (write(curr6->client_name, "It is a draw.\r\n", 15) != 15)
						    perror("write");
					    } else if (w == 'x') {
						if (write(curr6->client_name, "x wins.\r\n", 9) != 9)
                                                    perror("write");
					    } else {
						if (write(curr6->client_name, "o wins.\r\n", 9) != 9)
                                                    perror("write");
					    }

					    if (write(curr6->client_name, "Let's play again!\r\n", 19) != 19)
						perror("write");

					    if (curr6->client_name == whose_x) {
						if (write(curr6->client_name, "You are now x.\r\n", 16) != 16)
						    perror("write");
					    } else if (curr6->client_name == whose_o) {
						if (write(curr6->client_name, "You are now o.\r\n", 16) != 16)
						    perror("write");
					    }
					}
					curr6 = curr6->next;
				    }

				    reset_board();

				    if (who_starts == 'x') {
					whose_turn = "o";
					who_starts = 'o';
				    } else {
					whose_turn = "x";
					who_starts = 'x';
				    }
				}
			    }
			// if read wasn't a move
			} else {

		            printf("chat message: %s", curr->buf);

			    //output chat message to other clients
			    struct Node *curr2;
			    curr2 = head;

			    while (curr2) {
			        if (curr2->client_name != fd &&
				    curr2->client_name != curr->client_name) {
				    if (write(curr2->client_name, curr->buf, strlen(curr->buf))
				        != strlen(curr->buf))
				        perror("write");
			        }
			        curr2 = curr2->next;
			    }
			}
		    }
		}
	    }
	    curr = curr->next;
	}
    }
}

