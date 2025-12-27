/* 
   xpilot UDP ping utility by Baron & Coppa 2005
   compile with gcc xpping.c
   Do not use very many packets per second on a low bandwidth
   link, if you play simultaneously, as this will interfere
   with playing
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>

#define MSG_SIZE 9
#define DEFAULT_CONTACT_PORT 15345

char msg[MSG_SIZE];
struct timeval sendtime[256];
int sock, seq, pkt_second, contact_port, pings = 0, sent = 0;
double min, max, average = 0;

static void send_request(int seq)
{
    struct timeval *tv;

    msg[MSG_SIZE - 1] = (char)seq;
    tv = &sendtime[seq];
    if (tv->tv_sec != 0) {
	printf("timeout %d\n", seq);
    }
    if (send(sock, msg, MSG_SIZE, 0) == -1) {
	perror("send");
	exit(-1);
    }
    gettimeofday(tv, NULL);
    sent++;
}

static void alarm(int sig)
{
  send_request(seq + 1);
  seq = (seq + 1) % 255;
}

static void recv_reply()
{
    struct timeval now;
    struct timeval *start;
    int seq;
    long sec, usec;
    double pingtime;
    unsigned char buf[6];

    if (recv(sock, &buf, 6, 0) == -1) {
	perror("recv");
	fprintf(stderr, "Unable to establish a connection to xpilot server.\n");
	exit(-1);
    }
    gettimeofday(&now, NULL);
    seq = buf[4];
    start = &sendtime[seq];
    if (now.tv_usec < start->tv_usec) {
	sec = now.tv_sec - 1 - start->tv_sec;
	usec = 1000000 + now.tv_usec - start->tv_usec;
    } else {
	sec = now.tv_sec - start->tv_sec;
	usec = now.tv_usec - start->tv_usec;
    }
    pingtime = sec * 1000.0 + usec / 1000.0;
    printf("seq: %d lag: %.2lf\n", 
	   seq, pingtime);
    start->tv_sec = start->tv_usec = 0;
    average +=pingtime;

    if (pings > 0){
      if (pingtime > max){
	max = pingtime;
      }
      if (pingtime < min){
	min = pingtime;
      }
    }
    
    else{
      max = pingtime;
      min = pingtime;
    }
    
    pings++;
}

void sig_handler(int signum) {
  printf("Packets sent: %d received: %d     Lag min: %.2lf ms  max: %.2lf ms  average: %.2lf ms\n",
	 sent, pings, min, max, average/pings);
  exit(1);
}

int main(int argc, char *argv[])
{
    size_t t;
    struct sockaddr_in my_sa;
    struct sockaddr_in srv_sa;
    struct hostent *srv_host;
    struct itimerval timer;

    if (argc < 4) {
	fprintf(stderr, "Usage: %s host, packets per second, receiveport, [serverport]\n",argv[0]);
	return -1;
    }

    if (argc == 4) {
      contact_port = DEFAULT_CONTACT_PORT;
    }
 
    if (argc == 5) {
      contact_port = atoi(argv[4]);
    }

    if ((srv_host = gethostbyname(argv[1])) == NULL) {
	perror("gethostbyname");
	return -1;
    }

    pkt_second = atoi(argv[2]);

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
	perror("socket");
	return -1;
    }
    t = sizeof my_sa;
    my_sa.sin_port = htons(atoi(argv[3]));
    my_sa.sin_family = PF_INET;
    my_sa.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&my_sa, t)) {
	perror("failed to bind receive port");
	fprintf(stderr,"Make sure your system allows incoming UDP traffic on this port.\n");
	return -1;
    }
    srv_sa.sin_port = htons(contact_port);
    srv_sa.sin_family = PF_INET;
    memcpy(&srv_sa.sin_addr,
	   srv_host->h_addr,
	   srv_host->h_length);
    if (connect(sock, (struct sockaddr*)&srv_sa, sizeof srv_sa)) {
	perror("connect");
	return -1;
    }
    if (signal(SIGALRM, alarm) == SIG_ERR) {
	perror("setitimer");
	return -1;
    }
    memcpy(msg, "\x00\x00\xf4\xedp\x00\x04\x00 ", MSG_SIZE);
    timer.it_value.tv_sec = 1;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = (int)(1000000.0/(float)pkt_second);
    printf("Pinging xpilot server %s at port %d, receive port is %d with %d packets per second.\n",
	   argv[1], contact_port, atoi(argv[3]), pkt_second);

    if (setitimer(ITIMER_REAL, &timer, NULL)) {
	perror("setitimer");
	return -1;
    }

    while(1) { 
      if (signal (SIGINT, sig_handler) == SIG_IGN)
        signal (SIGINT, SIG_IGN);
      recv_reply(); 
    }
}
