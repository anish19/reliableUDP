#include <stdio.h>
#include <string.h>
#include "unp.h"
#include "unpthread.h"
#include "unpifiplus.h"
#include "dg_hdr.h"
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include "unprtt.h"
#include <setjmp.h>

#define MAX_INTF 16

struct server_info{
	int sockfd;
	struct sockaddr* IP_addr;
	struct sockaddr* netmask;
	struct sockaddr* subnet_addr;
};

static struct rtt_info rttinfo;
static int rttinit = 0;
static struct msghdr msgsend, msgrecv;
static struct dg_hdr sendhdr, recvhdr;
static void sig_alrm(int signo);
static sigjmp_buf jmpbuf;
int client_window_size = 5;

void sig_alrm(int signo)
{
    siglongjmp(jmpbuf, 1);
}

void send_file( FILE *fp, int file_size, int sockfd, struct sockaddr* client_addr ){
	
	char file_data[PAYLOAD_SIZE];
	int file_data_read_size, file_data_size = PAYLOAD_SIZE, n;
	sendhdr.seq = 0;
	int interval, last_unack_pack=0;
	struct itimerval it_val;
	int max_window_size = client_window_size;
	int file_buf_size = (file_size/PAYLOAD_SIZE) + 1;
	struct buf_ele file_buf[file_buf_size];
	
	int curr_pos = 0, window_empty = max_window_size;
	int idx = 0, i;
	int no_pack_sent_now=0;
	int complete_file_sent_flag = 0;
	int ack_ts, ack_seq, last_sent_seq_no;

	for (i = 0; i < file_buf_size; ++i)
	{
		file_buf[i].sent = 0;
	}
	i=0;

	while(!complete_file_sent_flag){
		
		while(no_pack_sent_now < window_empty && complete_file_sent_flag == 0){
			
			file_data_read_size = fread( (void*)file_data, sizeof(char), PAYLOAD_SIZE , fp); 
			//printf("read size is %d\n\n", file_data_read_size);
			if(file_data_read_size < 0){
				complete_file_sent_flag = 1;
			}
			else if(file_data_read_size < PAYLOAD_SIZE)
				file_data[file_data_read_size] = '\0';
		
			struct iovec iovsend[2], iovrecv[1];
			int ret, sent_bytes; 

			if(rttinit == 0){
				rtt_init(&rttinfo);
				rttinit = 1;
				rtt_d_flag = 1;
			}

			sendhdr.seq++;
			file_buf[idx].seq = sendhdr.seq;
			file_buf[idx].data_size = file_data_read_size;
			
			for(i = 0; i < file_data_read_size; i++){
				file_buf[idx].data[i] = file_data[i];
			}
			if(file_data_read_size < PAYLOAD_SIZE)
				file_buf[idx].data[file_data_read_size] = file_data[file_data_read_size];

			file_buf[idx].ts = sendhdr.ts;
			file_buf[idx].ack = 0;

			msgsend.msg_namelen = sizeof(*client_addr);
			iovsend[0].iov_base = (void*)&sendhdr;
			iovsend[0].iov_len = sizeof(struct dg_hdr);
			iovsend[1].iov_base = file_data;
			iovsend[1].iov_len = file_data_read_size;
			msgsend.msg_iov = iovsend;
			msgsend.msg_iovlen = 2;

			msgrecv.msg_name = NULL;
			msgrecv.msg_namelen = 0;
			iovrecv[0].iov_base = (void*) &recvhdr;
			iovrecv[0].iov_len = sizeof(struct dg_hdr);
			msgrecv.msg_iov = iovrecv;
			msgrecv.msg_iovlen = 1;
			Signal(SIGALRM, sig_alrm);
			rtt_newpack(&rttinfo);

			sendhdr.ts = rtt_ts(&rttinfo);
			file_buf[idx].ts = sendhdr.ts;

			if(complete_file_sent_flag == 0)
				sent_bytes = sendmsg(sockfd, &msgsend, 0);
			no_pack_sent_now++;
			file_buf[idx].sent = 1;
		
			printf("%s\n", iovsend[1].iov_base);

			if( sendhdr.seq == 1){
				interval = rtt_start(&rttinfo);
				it_val.it_value.tv_sec = interval/1000;
				it_val.it_value.tv_usec = (interval*1000) % 1000000;   
				it_val.it_interval = it_val.it_value;

				if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
					perror("error calling setitimer()");
					exit(1);
				}
			}
			int go_inside =0;
			if(go_inside == 1){
				if (sigsetjmp(jmpbuf, 1) != 0) {

					//SEND unacked pack

					//traverse all acked
					i = 0;
					while(file_buf[i].ack == 1 ){
						i++;	
					}
					//send unacked
					i++;
					sendhdr.seq = file_buf[i].seq;
					sendhdr.ts = file_buf[i].ts;
					iovsend[1].iov_base = file_buf[i].data;
					iovsend[1].iov_len = file_buf[i].data_size;
					sent_bytes = sendmsg(sockfd, &msgsend, 0);
					//no_pack_sent_now++;

					if (rtt_timeout(&rttinfo) < 0) {
						err_msg("dg_send_packet: no response from client, giving up");
						rttinit = 0;        
						errno = ETIMEDOUT;
						return;
					}

					
					break;
				}
			}
			idx++;
		}			
		
		do{
			n = Recvmsg( sockfd, &msgrecv, 0);
				
			ack_seq = recvhdr.seq;
			ack_ts = recvhdr.ts;
			window_empty = recvhdr.window_empty;
			//fast retransimt
			i = 0;
			while(file_buf[i].seq != ack_seq-1 ){
				file_buf[i].ack = 1;
				i++;
			}
			file_buf[i].ack == 1;

			it_val.it_value.tv_sec = 0;
			it_val.it_value.tv_usec = 0;   
			
			interval = rtt_start(&rttinfo);
			it_val.it_value.tv_sec = interval/1000;
			it_val.it_value.tv_usec = (interval*1000) % 1000000;   
			it_val.it_interval = it_val.it_value;

			if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
				perror("error calling setitimer()");
				exit(1);
			}

			//find the point till which packets are sent
			i=0;
			while(file_buf[i].sent){
				i++;
			}
			last_sent_seq_no = file_buf[i-1].seq;

		} while ( n == sizeof(struct dg_hdr) && ack_seq-1 < last_sent_seq_no );
		
		no_pack_sent_now = 0;
	}
	rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvhdr.ts);
	

}

int main(int argc, char* argv[]){
	FILE * fp;
	char read_file_line[128];
	size_t len = 128;
	
	const char *temp_ptr= NULL;
	int server_port = 0;
	int idx=0;
	int i, ret = 0, no_of_interface = 0;
	struct ifi_info* ifi;
	struct ifi_info* ifihead;
	u_char *ptr;
	struct sockaddr *sa;
	struct sockaddr_in *server_addr_in, *network_mask_in, *subnet_addr_in;
	int sockfd;
	struct sockaddr *subnet_addr; 

	char buf_str[64];
	void *buf = NULL;
	int buf_len = 0;
	int recvfrom_len = -1;
	struct sockaddr *recvfrom_addr = NULL;

	struct server_info server_info_list[MAX_INTF];

	fd_set rset;
	int maxfdp1, maxfd = -10000000;

	pid_t childpid;
	int child_sockfd, child_port, child_server_addr_len = -1;
	char child_port_str[16];
	struct sockaddr *child_server_addr;
	struct sockaddr_in *child_server_addr_in;

	char requested_file_name[32];
	int requested_file_name_len;

	char file_data[512], file_size_str[15];
	int file_data_size = 512, file_size;

	fp = fopen(argv[1], "r");	
	
	if (fp == NULL){
		printf("server.in cannot be opened\nEnter file with port and max sliding window size in the args\n");
		exit(EXIT_FAILURE);
	}

	while (fgets(read_file_line, len, fp) != NULL) {
		printf("%s", read_file_line);
		server_port = atoi(read_file_line);
	}

	i = 0; 	//index for server_info array
	ifihead = get_ifi_info_plus(AF_INET, 1);

	for( ifi = ifihead; ifi != NULL; ifi = ifi->ifi_next){

/*print the info from ifi_info struct*/
		printf("%s: ", ifi->ifi_name);
		if (ifi->ifi_index != 0)
		printf("(%d) ", ifi->ifi_index);
		printf("<");
		if (ifi->ifi_flags & IFF_UP)            printf("UP ");
		if (ifi->ifi_flags & IFF_BROADCAST)     printf("BCAST ");
		if (ifi->ifi_flags & IFF_MULTICAST)     printf("MCAST ");
		if (ifi->ifi_flags & IFF_LOOPBACK)      printf("LOOP ");
		if (ifi->ifi_flags & IFF_POINTOPOINT)   printf("P2P ");
		printf(">\n");
		if ( (i = ifi->ifi_hlen) > 0) {
			ptr = ifi->ifi_haddr;
			do {
			printf("%s%x", (i == ifi->ifi_hlen) ? "  " : ":", *ptr++);
        		} while (--i > 0);
             		printf("\n");
         	}
		if (ifi->ifi_mtu != 0)
			printf("  MTU: %d\n", ifi->ifi_mtu);
		if ( (sa = ifi->ifi_addr) != NULL)
			printf("  IP addr: %s\n", Sock_ntop_host (sa, sizeof (*sa)));

		((struct sockaddr_in*)sa)->sin_port = 60810;
		server_info_list[idx].IP_addr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
		server_info_list[idx].IP_addr = sa;

		printf("port is %d\n", ((struct sockaddr_in*)server_info_list[idx].IP_addr)->sin_port);
		/*creating scoket and binding every returned address to a socket*/
		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if(sockfd == -1){
			printf("could not create socket for %s\n", Sock_ntop_host(sa, sizeof (*sa)) );
		}
		
		server_info_list[idx].sockfd = sockfd;

		if( (ret = bind( server_info_list[idx].sockfd, server_info_list[idx].IP_addr, sizeof(struct sockaddr)) ) == -1 ){
			printf("could not bind socket to server IP address for %s\n",  Sock_ntop_host(sa, sizeof (*sa)) );
		}


		if ( (sa = ifi->ifi_brdaddr) != NULL)
			printf("  broadcast addr: %s\n",
		Sock_ntop_host (sa, sizeof(*sa)));
		
		if ( (sa = ifi->ifi_dstaddr) != NULL)
			printf("  destination addr: %s\n",
		Sock_ntop_host(sa, sizeof(*sa)));


		if((sa = ifi->ifi_ntmaddr) != NULL)
			printf("network mask: %s\n", Sock_ntop_host(sa, sizeof(*sa)));		
		server_info_list[idx].netmask = (struct sockaddr*) malloc(sizeof(struct sockaddr));
		server_info_list[idx].netmask = sa;

		//finding the subnet address
		server_addr_in = (struct sockaddr_in*) server_info_list[idx].IP_addr;
		network_mask_in = (struct sockaddr_in*) server_info_list[idx].netmask;
		
		subnet_addr_in = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
		subnet_addr_in->sin_addr.s_addr = server_addr_in->sin_addr.s_addr & network_mask_in->sin_addr.s_addr;
		
		server_info_list[idx].subnet_addr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
		server_info_list[idx].subnet_addr = (struct sockaddr*) subnet_addr_in ; 
	
		printf("IP address is: %s\n", Sock_ntop_host(server_info_list[idx].IP_addr, sizeof(struct sockaddr*)) );
		printf("Network mask is: %s\n", Sock_ntop_host(server_info_list[idx].netmask, sizeof(struct sockaddr*)) );
		printf("Subnet mask is: %s\n", inet_ntoa(subnet_addr_in->sin_addr) );
		printf("\n");

		idx++;
		no_of_interface++;
		printf("\n--------------------------------%d\n", no_of_interface);

	}
printf("no of inter = %d\n", no_of_interface);	

	//IO multiplexing for sockfd
	FD_ZERO(&rset);
	
	for(;;){
		for(idx = 0 ; idx < no_of_interface; idx++){
			FD_SET(server_info_list[idx].sockfd, &rset);
			if(sockfd > maxfd)
				maxfd = sockfd;
		}

		maxfdp1 = maxfd + 1;
		Select( maxfdp1, &rset, NULL, NULL, NULL);
		for(idx = 0; idx < no_of_interface; idx++){
			if(FD_ISSET(server_info_list[idx].sockfd, &rset )){

				recvfrom_addr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
				ret = recvfrom( server_info_list[idx].sockfd, buf_str, 64, 0, recvfrom_addr, &recvfrom_len);

				printf("Connection request recieved from %s\n", Sock_ntop_host(recvfrom_addr, recvfrom_len));
				buf_len = strlen(buf_str);
//				printf("buf_len is %d\n",buf_len);
				buf_str[ret-1] = '\0';
				printf("Message from client is:\n");
				printf("%s\n", buf_str);

				if ( (childpid = Fork()) == 0 ){
					
					Connect(server_info_list[idx].sockfd, recvfrom_addr, recvfrom_len);
					
					for( i=0; i< no_of_interface; i++){
						if(server_info_list[i].sockfd != server_info_list[idx].sockfd ){
							Close(server_info_list[i].sockfd);
							server_info_list[i].sockfd = -1;
						}
					}	
					i=0;
					child_sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
				
					child_server_addr_in->sin_port = htons(0);
					child_server_addr_in->sin_family = AF_INET;
					child_server_addr_in->sin_addr.s_addr = ((struct sockaddr_in*)(server_info_list[idx].IP_addr))->sin_addr.s_addr;
						
					child_server_addr = (struct sockaddr*)child_server_addr_in;
					Bind(child_sockfd, child_server_addr, sizeof(*child_server_addr));
						
						
					if( (ret = getsockname(child_sockfd, child_server_addr, &child_server_addr_len)) == -1){
						printf("getsockname failed\n");
					}
	
	
					child_port = ((struct sockaddr_in*)child_server_addr)->sin_port;
					
					sprintf( child_port_str, "%d", child_port);

					printf("new port is %d\n", child_port);
					Send( server_info_list[idx].sockfd, child_port_str, 16, 0);
					
					Connect( child_sockfd, recvfrom_addr, recvfrom_len);

					ret = -1;
					while(1){
						ret = recv( child_sockfd, requested_file_name, 32, 0);
						if(ret >-1)
							break;
					}

					printf("Name of file requested is: %s\n", requested_file_name);

					for(i = 0 ; i< strlen(requested_file_name); i++){
					
						printf("%d : %c\n", i , requested_file_name[i]);
					}
					requested_file_name[i-1] = '\0';
					
					printf("Name of file requested is: %s\n", requested_file_name);
					/**Sending file**/
					FILE *fpr;
					
					fpr = fopen( requested_file_name, "rb");
					if( fpr == NULL){
						printf("File named %s doesn't exist on server\n", requested_file_name);
						printf("%s\n", strerror(errno));
						exit(1);
					}
				
					printf("Sending file to client...\n");

					fseek(fpr, 0L, SEEK_END);
					file_size = ftell(fpr);
					fseek(fpr, 0L, SEEK_SET);

					sprintf( file_size_str, "%d", file_size);
					
					printf("before sending file name\n");
					//sending file size
					Send( child_sockfd, file_size_str, strlen(file_size_str), 0);
		printf("child socket fd is %d\n", child_sockfd);			
					send_file( fpr, file_size, child_sockfd, recvfrom_addr);

					printf("after sending file name\n");

/*
//while( fgets( file_data, file_data_size, fpr) != NULL){
					while( fread( (void*)file_data, sizeof(char),512 , fpr) ){
						
						printf("segment is %s\n", file_data);
						
						Send( child_sockfd, file_data, file_data_size, 0);
						if(ret == 0 )
							break;
					}
					printf("file sent\n");

*/

				}
				

			}
		}
	}


	fclose(fp);
	if (read_file_line)
		free(read_file_line);
	

}








