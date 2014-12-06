#include <kern/e1000.h>
#include <kern/pci.h>
#include <kern/pmap.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>


// LAB 6: Your driver code here

int i; 

volatile uint32_t *e1000;

struct tx_desc tx_array[E1000_TXD] __attribute__ ((aligned (16)));
struct tx_pkt tx_pkt_bufs[E1000_TXD];

struct rcv_desc rcv_array[E1000_RCVD] __attribute__ ((aligned (16)));
struct rcv_pkt rcv_pkt_bufs[E1000_RCVD];

int 
e1000_init(struct pci_func *pci)
{
	pci_func_enable(pci);

	e1000 = mmio_map_region(pci->reg_base[0], pci->reg_size[0]);
		
	memset(tx_array, 0x0, sizeof(struct tx_desc) * E1000_TXD);
	memset(tx_pkt_bufs, 0x0, sizeof(struct tx_pkt) * E1000_TXD);
	for (i = 0; i < E1000_TXD; i++) {
		tx_array[i].addr = PADDR(tx_pkt_bufs[i].buf);
		tx_array[i].status |= E1000_TXD_STAT_DD;
	}

	// Initialize rcv desc buffer array
	memset(rcv_array, 0x0, sizeof(struct rcv_desc) * E1000_RCVD);
	memset(rcv_pkt_bufs, 0x0, sizeof(struct rcv_pkt) * E1000_RCVD);
	for (i = 0; i < E1000_RCVD; i++) {
		rcv_array[i].addr = PADDR(rcv_pkt_bufs[i].buf);
	}

	/* Transmit initialization */
	// Transmit Descriptor Base Address Registers
	e1000[E1000_TDBAL] = PADDR(tx_array);
	e1000[E1000_TDBAH] = 0x0;

	// Set Transmit Descriptor Length Register
	e1000[E1000_TDLEN] = sizeof(struct tx_desc) * E1000_TXD;

	// Set Transmit Descriptor Head and Tail Registers

	e1000[E1000_TDT] = 0x0;
	e1000[E1000_TDH] = 0x0;
	
	// Initialize the Transmit Control Register 
	e1000[E1000_TCTL] |= E1000_TCTL_EN;
	e1000[E1000_TCTL] |= E1000_TCTL_PSP;

	//TODO: Fix shifts. 
	e1000[E1000_TCTL] |= (0x10) << 1;	//TCTL_CT
	e1000[E1000_TCTL] |= (0x40) << 3;	//TCTL_COLD


	// Program the Transmit IPG Register
	e1000[E1000_TIPG] = 0x0;	//reserve
	e1000[E1000_TIPG] |= (0x6) << 20; // IPGR2 
	e1000[E1000_TIPG] |= (0x8) << 10; // IPGR1
	e1000[E1000_TIPG] |= 0xA; // IPGT


	/* Recieve Initialization */
	// Recieve Address Registers
	/* e1000[E1000_RAL] = 0x12005452; 
	e1000[E1000_RAH] |= 0x5634; */

	//Challenge! EEPROM and MAC Addresses
	/* Data [31:16] | Address [15:8] | RSV. [7:5] | DONE [4] | RSV [3:1] | START [0] */

	#define EERD_START 0x1
	#define EERD_DONE 0x10	
	
	e1000[E1000_EERD] = 0x0 << 8;			// 5452 0010 
	e1000[E1000_EERD] |= EERD_START;		//Set Read
	while(!(e1000[E1000_EERD] & EERD_DONE));	//Read until Done
        cprintf("EERD %x \n", e1000[E1000_EERD]);
 
	e1000[E1000_RAL] = e1000[E1000_EERD] >> 16; 

	e1000[E1000_EERD] = 0x1 << 8;			// 1200 0110	
	e1000[E1000_EERD] |= EERD_START;                //Set Read
        while(!(e1000[E1000_EERD] & EERD_DONE));   	//Read until Done
        cprintf("EERD %x \n", e1000[E1000_EERD]);

	e1000[E1000_RAL] |= (e1000[E1000_EERD] &= ~0xffff);
	
	e1000[E1000_EERD] = 0x2 << 8;
	e1000[E1000_EERD] |= EERD_START;
	while(!(e1000[E1000_EERD] & EERD_DONE));
        cprintf("EERD %x \n", e1000[E1000_EERD]);
	
	e1000[E1000_RAH] = e1000[E1000_EERD] >> 16;

	e1000[E1000_RAH] |= 0x1 << 31; 
        cprintf("RAH %x \n", e1000[E1000_RAH]);


	// Program the Receive Descriptor Base Address Registers
	e1000[E1000_RDBAL] = PADDR(rcv_array);
        e1000[E1000_RDBAH] = 0x0;

	// Set the Receive Descriptor Length Register
	e1000[E1000_RDLEN] = sizeof(struct rcv_desc)  * E1000_RCVD;

        // Set the Receive Descriptor Head and Tail Registers
	e1000[E1000_RDT] = E1000_RCVD - 1;
	e1000[E1000_RDH] = 0x0;
	
	// Initialize the Receive Control Register
	e1000[E1000_RCTL] |= E1000_RCTL_EN;
	e1000[E1000_RCTL] &= ~E1000_RCTL_LPE;
	e1000[E1000_RCTL] &= ~E1000_RCTL_LBM;
	e1000[E1000_RCTL] &= ~E1000_RCTL_RDMTS;
	e1000[E1000_RCTL] &= ~E1000_RCTL_MO;
	e1000[E1000_RCTL] |= E1000_RCTL_BAM;
	//e1000[E1000_RCTL] &= ~E1000_RCTL_SZ; // 2048 byte size
	e1000[E1000_RCTL] |= E1000_RCTL_SECRC;


	cprintf("addr is %08x \n", e1000[E1000_STATUS]);		
	return 0;

}


int
e1000_MAC_high(void){
 	int high;
	high = e1000[E1000_RAH] & 0xffff;
	return high; 
}

int 
e1000_MAC_low(void){
        int low;                  
        low = e1000[E1000_RAL];
        return low;
}



int
e1000_transmit(char *data, int len)
{
	if (len > TX_PKT_SIZE) {
		return -E_PKT_TOO_BIG;
	}

	uint32_t tdt = e1000[E1000_TDT];

	if (tx_array[tdt].status & E1000_TXD_STAT_DD) {
		memcpy(tx_pkt_bufs[tdt].buf, data, len);
		tx_array[tdt].length = len;

		tx_array[tdt].status &= ~E1000_TXD_STAT_DD;
		tx_array[tdt].cmd |= E1000_TXD_CMD_RS;
		tx_array[tdt].cmd |= E1000_TXD_CMD_EOP;

		e1000[E1000_TDT] = (tdt + 1) % E1000_TXD;
	
		return 0;
	}
	
	return -E_TX_FULL;
}

int
e1000_receive(char *data)
{
	uint32_t rdt, len;
	rdt = (e1000[E1000_RDT] + 1) % E1000_RCVD;

	if (rcv_array[rdt].status & E1000_RXD_STAT_DD) {
		if (!(rcv_array[rdt].status & E1000_RXD_STAT_EOP)) {
			panic("e1000_receive: end of packet \n");
		}
		len = rcv_array[rdt].length;
		
		memcpy(data, rcv_pkt_bufs[rdt].buf, len);
		rcv_array[rdt].status &= ~E1000_RXD_STAT_DD;
		rcv_array[rdt].status &= ~E1000_RXD_STAT_EOP;
	
		e1000[E1000_RDT] = rdt;
	
		return len;
	}

	return -E_RCV_EMPTY;
}
