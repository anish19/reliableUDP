#include <stdio.h>
#include <string.h>
#include "unp.h"
#include "unpthread.h"
#include "unpifiplus.h"
#include <stdlib.h>
#include <sys/select.h>

#define MAX_INTF 16

struct server_info{
	int sockfd;
	struct sockaddr* IP_addr;
	struct sockaddr* netmask;
	struct sockaddr* subnet_addr;
};


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

	char buf_str[16];
	void *buf = NULL;
	int buf_len = 0;
	int recvfrom_len = 0;
	struct sockaddr *recvfrom_addr;

	struct server_info server_info_list[MAX_INTF];


	fd_set rset;
	int maxfdp1, maxfd = -10000000;

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
//				ret = recv( server_info_list[idx].sockfd, buf, buf_len, 0);
				buf_len = strlen(buf_str);
				printf("buf_len is %d\n",buf_len);
				buf_str[ret-1] = '\0';
				printf("Message from client is:\n");
				printf("%s\n", buf_str);
				
			}
		}
	}


	fclose(fp);
	if (read_file_line)
		free(read_file_line);
	

}




















