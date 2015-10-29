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

	printf("******in signal handler****\n");
    siglongjmp( jmpbuf, 1);
//    goto sendagain;
    printf("after siglongjump\n");
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
	int idx = 0, i = 0;
	int no_pack_sent_now=0;
	int complete_file_sent_flag = 0;
	int ack_ts = 0, ack_seq = 0, last_sent_seq_no = 0;

	for (i = 0; i < file_buf_size; ++i)
	{
		file_buf[i].sent = 0;
		file_buf[i].ack = 0;
		file_buf[i].seq = 0;
		file_buf[i].ts = 0;
		file_buf[i].drop = 0;
		file_buf[i].data_size = 0;
	}
	i=0;

	while(!complete_file_sent_flag){
		
		while(no_pack_sent_now < window_empty && complete_file_sent_flag == 0){
			
			file_data_read_size = fread( (void*)file_data, sizeof(char), PAYLOAD_SIZE , fp); 
			//printf("read size is %d\n\n", file_data_read_size);
			printf("file_data_read_size is :%d\n PAYLOAD_SIZE :%d\n", file_data_read_size, PAYLOAD_SIZE);
			
			file_data[file_data_read_size] = '\0';
			if(file_data_read_size < PAYLOAD_SIZE){
				complete_file_sent_flag = 1;
			
			}
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
			iovsend[1].iov_len = PAYLOAD_SIZE;
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

		//	if(complete_file_sent_flag == 0)
			sent_bytes = sendmsg(sockfd, &msgsend, 0);
			no_pack_sent_now++;
			file_buf[idx].sent = 1;
			printf("no of packets sent now is %d\n", no_pack_sent_now);

//			printf("%s\n", iovsend[1].iov_base);

			if( sendhdr.seq == 1){
				interval = 3000;	
//				interval = rtt_start(&rttinfo);
				it_val.it_value.tv_sec = interval/1000;
				it_val.it_value.tv_usec = (interval*1000) % 1000000;   
				it_val.it_interval = it_val.it_value;

				if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
					perror("error calling setitimer()");
					exit(1);
				}
			}
			int go_inside = 0;
		//	if(go_inside == 1){
sendagain:
				if (sigsetjmp(jmpbuf, 1) != 0) {
printf("\nIn if after sigjump\n");
					//SEND unacked pack

					//traverse all acked
					i = 0;
					while(file_buf[i].ack == 1 && i<file_buf_size ){
						i++;	
					}
					//send unacked
					i++;
					sendhdr.seq = file_buf[i].seq;
					sendhdr.ts = file_buf[i].ts;
			
					iovsend[0].iov_base = (void*)&sendhdr;
					iovsend[0].iov_len = sizeof(struct dg_hdr);
					iovsend[1].iov_base = file_buf[i].data;
					iovsend[1].iov_len = PAYLOAD_SIZE;
					sent_bytes = sendmsg(sockfd, &msgsend, 0);
					//no_pack_sent_now++;
					
					printf("retransmitted pack seq no is %d\n", sendhdr.seq);

					if (rtt_timeout(&rttinfo) < 0) {
						err_msg("dg_send_packet: no response from client, giving up");
						rttinit = 0;        
						errno = ETIMEDOUT;
						return;
					}

					
					break;
				}
		//	}
			idx++;
		}			
		
		do{
			printf("recving ack\n");
			n = Recvmsg( sockfd, &msgrecv, 0);
//			Read(sockfd, &recvhdr, sizeof(recvhdr));
			printf("last ack rcvd :%d\n", recvhdr.seq);
				
			ack_seq = recvhdr.seq;
			ack_ts = recvhdr.ts;
			window_empty = recvhdr.window_empty;
			//fast retransimt
			i = 0;
printf("before whlie@@@@@\n");
			while(file_buf[i].seq != ack_seq-1 ){
				file_buf[i].ack = 1;
				i++;
			}
			file_buf[i].ack = 1;
printf("after whlie@@@@@\n");
			it_val.it_value.tv_sec = 0;
			it_val.it_value.tv_usec = 0;   
		
			interval = 2;
		//	interval = rtt_start(&rttinfo);
			it_val.it_value.tv_sec = interval/1000;
			it_val.it_value.tv_usec = (interval*1000) % 1000000;   
			it_val.it_interval = it_val.it_value;

			if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
				perror("error calling setitimer()");
				exit(1);
			}

			//find the point till which packets are sent
			i=0;
			printf("here");
			while(file_buf[i].sent && i<file_buf_size){
				i++;
			}
			last_sent_seq_no = file_buf[i-1].seq;
			printf("there\n");
		
		} while ( n == sizeof(struct dg_hdr) && ack_seq-1 < last_sent_seq_no );
		
		no_pack_sent_now = 0;

	printf("\n\nCOMPLETE FILE SENT : %d\n\n", complete_file_sent_flag);
	}
	rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvhdr.ts);

		interval = 0;
		it_val.it_value.tv_sec = interval/1000;
		it_val.it_value.tv_usec = (interval*1000) % 1000000;   
		it_val.it_interval = it_val.it_value;
		if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
			perror("error calling setitimer()");
			exit(1);
		}
printf("end of func\n");	
return;
}

int main(int argc, char* argv[]){
	FILE * fp;
	char read_file_line[128];
	size_t len = 128;
	
	const char *temp_ptr= NULL;
	int server_port = 0;
	int idx=0;
	int i = 0, ret = 0, no_of_interface = 0, on =1;
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

	fd_set rset, rset_copy;
	int maxfdp1, maxfd = -1;

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
		if(i==0)
			server_port = atoi(read_file_line);
		if(i==1)
			client_window_size = atoi(read_file_line);
		i++;
	}
	printf("Well known server port is : %d\n", server_port);
	printf("Max sending window sliding size is : %d\n", client_window_size);

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
		on = 1;
		if( ( ret = setsockopt(server_info_list[idx].sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on) ) ) < 0 ){
                        printf("setsockopt SO_REUSEADDR failed\n");
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
	while(1){
		
printf("begining of while\n");
printf("no of inter in while = %d\n", no_of_interface);
idx=0;
		for(idx = 0 ; idx < no_of_interface; idx++){
	//		printf("before FD_SET for idx %d\n", idx);
			FD_SET(server_info_list[idx].sockfd, &rset);
			printf("socket fd is %d\n", server_info_list[idx].sockfd);
	//		printf("after FD_SET for idx %d\n", idx);
			if(server_info_list[idx].sockfd > maxfd){
				maxfd = server_info_list[idx].sockfd;
			}
	//		printf("maxfd in loop is %d\n", maxfd);
		}
	//	rset_copy = rset ;
		maxfdp1 = maxfd + 1;
		printf("maxfd is %d\n", maxfd);
		printf("before select\n");
		select( maxfdp1, &rset, NULL, NULL, NULL);
		printf("after select\n");

		for(idx = 0; idx < no_of_interface; idx++){

			printf("socket is %d\n", server_info_list[idx].sockfd);
			if(FD_ISSET(server_info_list[idx].sockfd, &rset )){



//				FD_CLR(server_info_list[idx].sockfd, &rset);
				printf("set socket is %d\n", server_info_list[idx].sockfd);
				recvfrom_addr = (struct sockaddr*) malloc(sizeof(struct sockaddr));
				ret = recvfrom( server_info_list[idx].sockfd, buf_str, 64, 0, recvfrom_addr, &recvfrom_len);

				printf("Connection request recieved from %s\n", Sock_ntop_host(recvfrom_addr, recvfrom_len));
				buf_len = strlen(buf_str);
//				printf("buf_len is %d\n",buf_len);
				buf_str[ret] = '\0';
				printf("Message from client is:\n");
				printf("%s\n", buf_str);

				if ( (childpid = Fork()) == 0 ){
					printf("in child*******\n");	
					for( i=0; i< no_of_interface; i++){
						if(server_info_list[i].sockfd != server_info_list[idx].sockfd ){
							close(server_info_list[i].sockfd);
	//						server_info_list[i].sockfd = -1;
						}
					}	
					i=0;
				//	Connect(server_info_list[idx].sockfd, recvfrom_addr, recvfrom_len);
	
					
					
					child_sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
					//printf();	
					child_server_addr_in = (struct sockaddr_in*) malloc(sizeof (struct sockaddr_in));

					child_server_addr_in->sin_family = AF_INET;
					
					child_server_addr_in->sin_port = htons(0);
	
					child_server_addr_in->sin_addr.s_addr = ((struct sockaddr_in*)(server_info_list[idx].IP_addr))->sin_addr.s_addr;
					
					child_server_addr = (struct sockaddr*)child_server_addr_in;
					Bind(child_sockfd, child_server_addr, sizeof(*child_server_addr));
						
					on=1;	
					if( ( ret = setsockopt(child_sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on) ) ) < 0 ){
                         			printf("setsockopt SO_REUSEADDR failed\n");
					}

					if( (ret = getsockname(child_sockfd, child_server_addr, &child_server_addr_len)) == -1){
						printf("getsockname failed\n");
					}
	
	
					child_port = ((struct sockaddr_in*)child_server_addr)->sin_port;
					
					sprintf( child_port_str, "%d", child_port);

					sendto( server_info_list[idx].sockfd, child_port_str, 16, 0, recvfrom_addr, recvfrom_len);
					printf("new port is %d\n", child_port);
					
					close(server_info_list[idx].sockfd);

					Connect( child_sockfd, recvfrom_addr, recvfrom_len);

					ret = -1;
			//		while(1){
						ret = recv( child_sockfd, requested_file_name, 32, 0);
			//			if(ret >-1)
			//				break;
			//		}

					printf("Name of file requested is: %s\n", requested_file_name);

					for(i = 0 ; i< strlen(requested_file_name); i++){
					
						printf("%d : %c\n", i , requested_file_name[i]);
					}
					requested_file_name[i] = '\0';
					
					printf("Name of file requested is: %s\n", requested_file_name);
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

					close( child_sockfd);
			//		child_sockfd = -1;

					close(server_info_list[idx].sockfd);
					printf("after sending file\n");
					exit(0);
				}
				else{
				//	close(server_info_list[idx].sockfd);
					printf("sockfd in parent is ### %d\n", server_info_list[idx].sockfd);
				}
				

			}
		}
	printf("waiting for request\n");
	}

	fclose(fp);
	if (read_file_line)
		free(read_file_line);
	

}








