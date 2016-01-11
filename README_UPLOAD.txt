CSE 533: Network Programming
UDP Socket Programming
--------------------------------------
Anish Ahmed 
SBU ID: 110560809
Netid: aniahmed
Pratik Shrivastav
SBU ID: 110385785
Netid: pshrivastav cse533-6
--------------------------------------


User Documentation
--------------------------------------
-This submission consists of file 
1.client.c
2.server.c
3.dg_hdr.h
-It also has a Makefile and a ReadMe
-'make' command is used to compile the files
-'make clean' is used to remove all the executables.
-To run the server enter ./server server.in
-To run the client enter ./client client.in
-Details regarding server and client will be present
in the files client.in and server.in 
They should be in the same directory as the other files.
-The file to be transferred must also be in the same directory.

System Documentation

--------------------------------------

1.	Server gets the parameters such as window size and port from the
	file sever.in .It obtains details of all its interfaces using get_ifi_info_plus.c
	which is a modified version of get_ifi_info.c This has been done for subnet calculation
	
2.  The interfaces obtained above are stored in a structure which consists of 
	following fields.
	-sockfd
	-addr
	-ntmaddr
	-subaddr
	
3.  Client obtains its parameters from the file client.in which includes 
	-Ipserver
	-Port number
	-File name to be transferred
	-Receiver window size
	-Seed value 
	-Probability of datagram loss
	-Mean of the distribution at which buffer is read.
	
4.  Client also finds details of all its interfaces using get_ifi_info_plus.c
	The client then determines if client and host are on the same subnet by calculating
	the bitwise AND of the network mask and IPserver address. If the client and server are on 
	same subnet then the message is printed and SO_DONTROUTE option is enabled.
	
5.  Client creates an UDP socket and binds it to an ephemeral port.
	Client gets details about its port and IP address from the getsockname function.
	
6.  Client then connects to the well known port of server .It uses getpeername function to find 
	details of the server such as the servers port and Ipserver address.
	
7.  The client sends the server the name of the file it wants.
	The server receives the name of file, port number and client Ip address from using recvfrom function.
	
8.  The server then forks off a child process to carry on the communication. The child inherits the 
	listening socket properties. At the same time it closes all other sockets from the array of structures.
	Server also checks if the client is local or not by the use of netmask and ip address. If they are local
	then the SO_DONTROUTE option is set. It displays if the client is local or not.
	
9.  The server child carries on the further communication for file transfer. It binds with the server Ip
	and its ephemeral port number. The server sends its Ip address and port number to the client. It waits for the 
	acknowledgement. Once the ack is received the server closes down its listening socket.
	
10. The client connects to servers connection socket. Now the file transfer starts.

11. Also the environment for packet loss is simulated by the use of probability and a random number generator.
	If the probability of packet is more than the threshold specified in client.in then it is kept or else it is
	dropped.
	NOTE: When the packet drop probability is very high the file transfer will require a lot of time for 
	successful execution.
	
Reliability Implementation
--------------------------

1. The packet structure used in the assignment consists of a header and payload.
   The header structure is 
   -sequence number
   -time stamp
   -window empty size : the client tells the server the space left in its window
   
TCP like reliability is achieved by implementing the following things:

a) Sequence Number and Acknowledgement
	The sequence number indicates the packet numbers that are being sent. Based on the sequence number the 
	acknowledgement is issued. The acknowledgement indicates the sequence number of the packet which is expected 
	next. For example: If packet 1 is sent and is received at client then the client will send an ACK with value 
	2 which indicates that the packet 1 has been received and the next expexted packet is 2.
	
b) Sliding Window Protocol
	The client server communication is handled by the use of windows. By the use of window we are assured that
	packets are not sent beyond the window size. All the packets within the window are sent at once and then
	they are read at server. The capacity of receiver window is displayed as the packets are being sent. This helps 
	us acheive flow control.
	
c) Retransmission of lost datagrams and Timeout mechanism
	The server uses timeout mechanism . It works in the way that if within the RTT time the acknowledgement ACK 
	is not received then the server re-transmits the packet assuming that the packet is lost. The code from 
	Steven's book is used for calculating value of RTO after packet reception and time out.The ACK being issued 
	is a cumulative ACK. For the entire window it is possible that the packet arrive out of order. Hence a cumulative 
	ACK for the entire window is issued. 
	-The time out has been modified as per the assignment required by making changes to unprtt.h
	We have set the values to 
	RTT_RXTMIN to 1sec
	RTT_RXTMAX to 3sec
	RTT_MAXNREXMT to 12
	
d) Flow control:
	The client indicates the available window size in every acknowledgement sent to the server. The server adjusts 
	its send size as per the received window size. The available window size is being printed out to the screen.
	The available window size is calculated as follows rwnd = window_size-(last packet sent- last packet ACKed).
	
	  
f) Threads
	The consumer sits on another thread which sleeps occasionally(following an exponential distribution). When it wakes up it 
	consumes all the data in the window and changes its pointer. 
	The consumer also prints the data .The thread sleeps based on the value of the distribution 
	provided as input by the client.in file .The separate thread ensures that there is no interference between the printing
	of contents and the sliding of window.

g) Mutex lock
	Mutex lock has ben used to avoid concurrent read/write between the client receiver and the consuming thread. 
	
	

Program Documentation
----------------------
-The status messages are being displayed on the screen from time to time as per the requirement.
-Comments in the code add more clarity for a better understanding



	
	

	