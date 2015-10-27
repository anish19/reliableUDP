struct dg_hdr{
	uint32_t seq;
	uint32_t ts;
	int window_empty;		//size of empty part of window
};

struct buf_ele{
	int seq;
	int ts;
	int ack;
	char data[PAYLOAD_SIZE];
	int data_size;
	int drop;
	int sent;
};