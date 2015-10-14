#include <stdio.h>
#include "unp.h"
#include "unpthread.h"
#include "unpifiplus.h"
#include <stdlib.h>

int main(int argc, char* argv[]){
	FILE * fp;
	char read_file_line[128];
	size_t len = 128;
	
	int server_port = 0;
	int i =0;
	struct ifi_info* ifi;
	struct ifi_info* ifihead;
	u_char *ptr;
	struct sockaddr *sa;
	
	fp = fopen(argv[1], "r");	
	
	if (fp == NULL){
		printf("server.in cannot be opened\nEnter file with port and max sliding window size in the args\n");
		exit(EXIT_FAILURE);
	}

	while (fgets(read_file_line, len, fp) != NULL) {
		printf("%s", read_file_line);
		server_port = atoi(read_file_line);
	}


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
		if ( (sa = ifi->ifi_brdaddr) != NULL)
			printf("  broadcast addr: %s\n",
		Sock_ntop_host (sa, sizeof(*sa)));
		
		if ( (sa = ifi->ifi_dstaddr) != NULL)
			printf("  destination addr: %s\n",
		Sock_ntop_host(sa, sizeof(*sa)));


		if((sa = ifi->ifi_ntmaddr) != NULL)
			printf("network mask: %s\n", Sock_ntop_host(sa, sizeof(*sa)));		


/*binding every returned address to a socket*/
		
		


	}

	fclose(fp);
	if (read_file_line)
		free(read_file_line);
	

}
