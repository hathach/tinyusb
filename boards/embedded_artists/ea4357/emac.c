/******************************************************************
 *****                                                        *****
 *****  Name: cs8900.c                                        *****
 *****  Ver.: 1.0                                             *****
 *****  Date: 07/05/2001                                      *****
 *****  Auth: Andreas Dannenberg                              *****
 *****        HTWK Leipzig                                    *****
 *****        university of applied sciences                  *****
 *****        Germany                                         *****
 *****  Func: ethernet packet-driver for use with LAN-        *****
 *****        controller CS8900 from Crystal/Cirrus Logic     *****
 *****                                                        *****
 *****  NXP: Module modified for use with NXP            	  *****
 *****        lpc43xx EMAC Ethernet controller                *****
 *****                                                        *****
 ******************************************************************/

#include "../../board.h"

#if BOARD == BOARD_EA4357

#include "emac.h"
//#include "tcpip.h"
#include "LPC43xx.h"
#include "lpc43xx_scu.h"
#include "lpc43xx_rgu.h"

#define		TIMEOUT		100000

static unsigned short *rptr;
static unsigned short *tptr;

static unsigned int TxDescIndex = 0;
static unsigned int RxDescIndex = 0;

// Keil: function added to write PHY
static void write_PHY (unsigned int PhyReg, unsigned short Value) {
   
   unsigned int tout;

   /* Write a data 'Value' to PHY register 'PhyReg'. */
   while(LPC_ETHERNET->MAC_MII_ADDR & GMII_BUSY);			// Check GMII busy bit
   LPC_ETHERNET->MAC_MII_ADDR = (DP83848C_DEF_ADR<<11) | (PhyReg<<6) | GMII_WRITE;
   LPC_ETHERNET->MAC_MII_DATA = Value;
   LPC_ETHERNET->MAC_MII_ADDR |= GMII_BUSY;				// Start PHY Write Cycle

   /* Wait utill operation completed */
   for (tout = 0; tout < MII_WR_TOUT; tout++) {
      if ((LPC_ETHERNET->MAC_MII_ADDR & GMII_BUSY) == 0) {
         break;
      }
   }
   if (tout == MII_WR_TOUT)								// Trap the timeout
     while(1);
}


// Keil: function added to read PHY
static unsigned short read_PHY (unsigned int PhyReg) {
   
   unsigned int tout, val;

   /* Read a PHY register 'PhyReg'. */
   while(LPC_ETHERNET->MAC_MII_ADDR & GMII_BUSY);			// Check GMII busy bit
   LPC_ETHERNET->MAC_MII_ADDR = (DP83848C_DEF_ADR<<11) | (PhyReg<<6) | GMII_READ;
   LPC_ETHERNET->MAC_MII_ADDR |= GMII_BUSY;				// Start PHY Read Cycle

   /* Wait until operation completed */
   for (tout = 0; tout < MII_RD_TOUT; tout++) {
      if ((LPC_ETHERNET->MAC_MII_ADDR & GMII_BUSY) == 0) {
         break;
      }
   }
   if (tout == MII_RD_TOUT)								// Trap the timeout
     while(1);
   val = LPC_ETHERNET->MAC_MII_DATA;
   return (val);
}


// Keil: function added to initialize Rx Descriptors
void rx_descr_init (void)
{
  unsigned int i;

  for (i = 0; i < NUM_RX_DESC; i++) {
    RX_DESC_STAT(i) = OWN_BIT; 
	RX_DESC_CTRL(i) = ETH_FRAG_SIZE;
	RX_BUFADDR(i) = RX_BUF(i); 
	if (i == (NUM_RX_DESC-1)) 			// Last Descriptor?
	  RX_DESC_CTRL(i) |= RX_END_RING;
  }

  /* Set Starting address of RX Descriptor list */
  LPC_ETHERNET->DMA_REC_DES_ADDR = RX_DESC_BASE;
}


// Keil: function added to initialize Tx Descriptors
void tx_descr_init (void)
{
  unsigned int i;

  for (i = 0; i < NUM_TX_DESC; i++) {						// Take it out!!!!
	 TX_DESC_STAT(i) = 0;
	 TX_DESC_CTRL(i) = 0;
	 TX_BUFADDR(i) = 0;
  }

  for (i = 0; i < NUM_TX_DESC; i++) {
    TX_DESC_STAT(i) = TX_LAST_SEGM | TX_FIRST_SEGM;
	TX_DESC_CTRL(i) = 0;
	TX_BUFADDR(i) = TX_BUF(i); 
	if (i == (NUM_TX_DESC-1)) 		   // Last Descriptor?
	  TX_DESC_STAT(i) |= TX_END_RING;
  }

  /* Set Starting address of RX Descriptor list */
  LPC_ETHERNET->DMA_TRANS_DES_ADDR = TX_DESC_BASE;
}



// configure port-pins for use with LAN-controller,
// reset it and send the configuration-sequence

void Init_EMAC(void)
{
  int id1, id2, tout, regv;
  unsigned phy_in_use = 0;
  
  /* Ethernet pins configuration		*/
#if MII  
  scu_pinmux(0xC ,1 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_MDC: PC_1 -> FUNC3
  scu_pinmux(0x1 ,17 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_MDIO: P1_17 -> FUNC3 
  scu_pinmux(0x1 ,18 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_TXD0: P1_18 -> FUNC3 
  scu_pinmux(0x1 ,20 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_TXD1: P1_20 -> FUNC3
  scu_pinmux(0x1 ,19 , (MD_PLN | MD_EZI | MD_ZI), FUNC0); 	// ENET_REF: P1_19 -> FUNC0 (default)

//  scu_pinmux(0xC ,4 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_TX_EN: PC_4 -> FUNC3
  scu_pinmux(0x0 ,1 , (MD_PLN | MD_EZI | MD_ZI), FUNC6); 	// ENET_TX_EN: P0_1 -> FUNC6

  scu_pinmux(0x1 ,15 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_RXD0: P1_15 -> FUNC3
  scu_pinmux(0x0 ,0 , (MD_PLN | MD_EZI | MD_ZI), FUNC2); 	// ENET_RXD1: P0_0 -> FUNC2	

//  scu_pinmux(0x1 ,16 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_CRS: P1_16 -> FUNC3
  scu_pinmux(0x9 ,0 , (MD_PLN | MD_EZI | MD_ZI), FUNC5); 	// ENET_CRS: P9_0 -> FUNC5

//  scu_pinmux(0xC ,9 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_RX_ER: PC_9 -> FUNC3
  scu_pinmux(0x9 ,1 , (MD_PLN | MD_EZI | MD_ZI), FUNC5);	// ENET_RX_ER: P9_1 -> FUNC5

//  scu_pinmux(0xC ,8 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_RXDV: PC_8 -> FUNC3	 	
  scu_pinmux(0x1 ,16 , (MD_PLN | MD_EZI | MD_ZI), FUNC7); 	// ENET_RXDV: P1_16 -> FUNC7	 	

#else
  scu_pinmux(0xC ,1 , (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_MDC: PC_1 -> FUNC3
  scu_pinmux(0x1 ,17 , (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_MDIO: P1_17 -> FUNC3 
  scu_pinmux(0x1 ,18 , (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_TXD0: P1_18 -> FUNC3 
  scu_pinmux(0x1 ,20 , (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_TXD1: P1_20 -> FUNC3
  scu_pinmux(0x1 ,19 , (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC0); 	// ENET_REF: P1_19 -> FUNC0 (default)
//  scu_pinmux(0xC ,4 , (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_TX_EN: PC_4 -> FUNC3
  scu_pinmux(0x0 ,1 , (MD_PLN | MD_EZI | MD_ZI), FUNC6); 	// ENET_TX_EN: P0_1 -> FUNC6
  scu_pinmux(0x1 ,15 , (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_RXD0: P1_15 -> FUNC3
  scu_pinmux(0x0 ,0 , (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC2); 	// ENET_RXD1: P0_0 -> FUNC2	
//  scu_pinmux(0x1 ,16 , (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_CRS: P1_16 -> FUNC3
//  scu_pinmux(0x9 ,0 , (MD_PLN | MD_EZI | MD_ZI), FUNC5); 	// ENET_CRS: P9_0 -> FUNC5
//  scu_pinmux(0xC ,9 , (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_RX_ER: PC_9 -> FUNC3
//  scu_pinmux(0x9 ,1 , (MD_PLN | MD_EZI | MD_ZI), FUNC5);	// ENET_RX_ER: P9_1 -> FUNC5
//  scu_pinmux(0xC ,8 , (MD_EHS | MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_RXDV: PC_8 -> FUNC3
  scu_pinmux(0x1 ,16 , (MD_PLN | MD_EZI | MD_ZI), FUNC7); 	// ENET_RXDV: P1_16 -> FUNC7
#endif
 
  
#if MII				  /*   Select MII interface       */				 // check MUXING for new Eagle...
//  scu_pinmux(0xC ,6 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_RXD2: PC_6 -> FUNC3
  scu_pinmux(0x9 ,3 , (MD_PLN | MD_EZI | MD_ZI), FUNC5); 	// ENET_RXD2: P9_3 -> FUNC5

//  scu_pinmux(0xC ,7 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_RXD3: PC_7 -> FUNC3
  scu_pinmux(0x9 ,2 , (MD_PLN | MD_EZI | MD_ZI), FUNC5); 	// ENET_RXD3: P9_2 -> FUNC5

  scu_pinmux(0xC ,0 , (MD_PLN | MD_EZI | MD_ZI), FUNC3);  // ENET_RXLK: PC_0 -> FUNC3

//  scu_pinmux(0xC ,2 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_TXD2: PC_2 -> FUNC3
  scu_pinmux(0x9 ,4 , (MD_PLN | MD_EZI | MD_ZI), FUNC5); 	// ENET_TXD2: P9_4 -> FUNC5

//  scu_pinmux(0xC ,3 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_TXD3: PC_3 -> FUNC3
  scu_pinmux(0x9 ,5 , (MD_PLN | MD_EZI | MD_ZI), FUNC5); 	// ENET_TXD3: P9_5 -> FUNC5

//  scu_pinmux(0xC ,5 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_TX_ER:  PC_5 -> FUNC3
  scu_pinmux(0xC ,5 , (MD_PLN | MD_EZI | MD_ZI), FUNC3); 	// ENET_TX_ER:  PC_5 -> FUNC3
  
//  scu_pinmux(0x0 ,1 , (MD_PLN | MD_EZI | MD_ZI), FUNC2); 	// ENET_COL:  P0_1 -> FUNC2
    scu_pinmux(0x9 ,6 , (MD_PLN | MD_EZI | MD_ZI), FUNC5); 	// ENET_COL:  P9_6 -> FUNC5
#else				   /*   Select RMII interface     */
  LPC_CREG->CREG6 |= RMII_SELECT;
#endif


  RGU_SoftReset(RGU_SIG_ETHERNET);
  while(1){													  // Confirm the reset happened
	 if (LPC_RGU->RESET_ACTIVE_STATUS0 & (1<<ETHERNET_RST))
	   break;
  }

  LPC_ETHERNET->DMA_BUS_MODE |= DMA_SOFT_RESET; 	         // Reset all GMAC Subsystem internal registers and logic  
  while(LPC_ETHERNET->DMA_BUS_MODE & DMA_SOFT_RESET);	     // Wait for software reset completion

  /* Put the DP83848C in reset mode */
  write_PHY (PHY_REG_BMCR, PHY_BMCR_RESET);

  /* Wait for hardware reset to end. */
  for (tout = 0; tout < TIMEOUT; tout++) {
    regv = read_PHY (PHY_REG_BMCR);
    if (!(regv & PHY_BMCR_RESET)) {
      /* Reset complete */
      break;
    }
  }

  /* Check if this is a DP83848C PHY. */
  id1 = read_PHY (PHY_REG_IDR1);
  id2 = read_PHY (PHY_REG_IDR2);
  if (((id1 << 16) | (id2 & 0xFFF0)) == DP83848C_ID) {
    phy_in_use =  DP83848C_ID;
  }
  else if (((id1 << 16) | (id2 & 0xFFF0)) == LAN8720_ID) {
    phy_in_use = LAN8720_ID;
  }

  if (phy_in_use != 0) {
	/* Configure the PHY device */
#if !MII
  write_PHY (PHY_REG_RBR, 0x20);
#endif

    /* Use autonegotiation about the link speed. */
    write_PHY (PHY_REG_BMCR, PHY_AUTO_NEG);
    /* Wait to complete Auto_Negotiation. */
    for (tout = 0; tout < TIMEOUT; tout++) {
      regv = read_PHY (PHY_REG_BMSR);
      if (regv & PHY_AUTO_NEG_DONE) {
        /* Autonegotiation Complete. */
        break;
      }
    }
  }



  /* Check the link status. */
  for (tout = 0; tout < TIMEOUT; tout++) {
    regv = read_PHY (PHY_REG_STS);
    if (regv & LINK_VALID_STS) {
      /* Link is on. */
      break;
    }
  }
  
  // Configure the EMAC with the established parameters
  switch (phy_in_use) {
  	  case DP83848C_ID:

        /* Configure Full/Half Duplex mode. */
        if (regv & FULL_DUP_STS) {
          /* Full duplex is enabled. */
          LPC_ETHERNET->MAC_CONFIG    |= MAC_DUPMODE;
        }

        /* Configure 100MBit/10MBit mode. */
        if (~(regv & SPEED_10M_STS)) {
          /* 100MBit mode. */
          LPC_ETHERNET->MAC_CONFIG    |= MAC_100MPS;
        }
      
//   		  value = ReadFromPHY (PHY_REG_STS);	/* PHY Extended Status Register  */
//   		  // Now configure for full/half duplex mode
//   		  if (value & 0x0004) {
//   		    // We are in full duplex is enabled mode
//   			  LPC_ETHERNET->MAC2    |= MAC2_FULL_DUP;
//   			  LPC_ETHERNET->Command |= CR_FULL_DUP;
//   			  LPC_ETHERNET->IPGT     = IPGT_FULL_DUP;
//   		  }
//   		  else {
//   		    // Otherwise we are in half duplex mode
//   			  LPC_ETHERNET->IPGT = IPGT_HALF_DUP;
//   		  }

//   		  // Now configure 100MBit or 10MBit mode
//   		  if (value & 0x0002) {
//   		    // 10MBit mode
//   			  LPC_ETHERNET->SUPP = 0;
//   		  }
//   		  else {
//   		    // 100MBit mode
//   			  LPC_ETHERNET->SUPP = SUPP_SPEED;
//   		  }
  		  break;

  	  case LAN8720_ID:

  		  regv = read_PHY (PHY_REG_SCSR);	/* PHY Extended Status Register  */
  		  // Now configure for full/half duplex mode
  		  if (regv & (1<<4)) {		/* bit 4: 1 = Full Duplex, 0 = Half Duplex  */
    		  // We are in full duplex is enabled mode
          LPC_ETHERNET->MAC_CONFIG    |= MAC_DUPMODE;
  		  }

  		  // Now configure 100MBit or 10MBit mode
  		  if (regv & (1<<3)) {	/* bit 3: 1 = 100Mbps, 0 = 10Mbps  */
  			  // 100MBit mode
          LPC_ETHERNET->MAC_CONFIG    |= MAC_100MPS;
  		  }


  		  break;

  }
   
  /* Set the Ethernet MAC Address registers */
  LPC_ETHERNET->MAC_ADDR0_HIGH = (MYMAC_6 << 8) | MYMAC_5;
  LPC_ETHERNET->MAC_ADDR0_LOW =	(MYMAC_4 << 24) | (MYMAC_3 << 16) | (MYMAC_2 << 8) | MYMAC_1;

  /* Initialize Descriptor Lists    */
  rx_descr_init();
  tx_descr_init();
  
  /* Configure Filter           */  
  LPC_ETHERNET->MAC_FRAME_FILTER = MAC_PROMISCUOUS | MAC_RECEIVEALL;

  /* Enable Receiver and Transmitter   */
  LPC_ETHERNET->MAC_CONFIG |= (MAC_TX_ENABLE | MAC_RX_ENABLE); 

  /* Enable interrupts    */
  //LPC_ETHERNET->DMA_INT_EN =  DMA_INT_NOR_SUM | DMA_INT_RECEIVE | DMA_INT_TRANSMIT;	 

  /* Start Transmission & Receive processes   */
  LPC_ETHERNET->DMA_OP_MODE |= (DMA_SS_TRANSMIT | DMA_SS_RECEIVE );		 

}


// reads a word in little-endian byte order from RX_BUFFER

unsigned short ReadFrame_EMAC(void)
{
  return (*rptr++);
}


// easyWEB internal function
// help function to swap the byte order of a WORD

unsigned short SwapBytes(unsigned short Data)
{
  return (Data >> 8) | (Data << 8);
}

// reads a word in big-endian byte order from RX_FRAME_PORT
// (useful to avoid permanent byte-swapping while reading
// TCP/IP-data)

unsigned short ReadFrameBE_EMAC(void)
{
  unsigned short ReturnValue;

  ReturnValue = SwapBytes (*rptr++);
  return (ReturnValue);
}


// copies bytes from frame port to MCU-memory
// NOTES: * an odd number of byte may only be transfered
//          if the frame is read to the end!
//        * MCU-memory MUST start at word-boundary

void CopyFromFrame_EMAC(void *Dest, unsigned short Size)
{
  unsigned short * piDest;                       // Keil: Pointer added to correct expression

  piDest = Dest;                                 // Keil: Line added
  while (Size > 1) {
    *piDest++ = ReadFrame_EMAC();
    Size -= 2;
  }
  
  if (Size) {                                         // check for leftover byte...
    *(unsigned char *)piDest = (char)ReadFrame_EMAC();// the LAN-Controller will return 0
  }                                                   // for the highbyte
}

// does a dummy read on frame-I/O-port
// NOTE: only an even number of bytes is read!

void DummyReadFrame_EMAC(unsigned short Size)    // discards an EVEN number of bytes
{                                                // from RX-fifo
  while (Size > 1) {
    ReadFrame_EMAC();
    Size -= 2;
  }
}

// Reads the length of the received ethernet frame and checks if the 
// destination address is a broadcast message or not
// returns the frame length
unsigned short StartReadFrame(void) {
  unsigned short RxLen;

  if ((RX_DESC_STAT(RxDescIndex) & OWN_BIT) == 0) {
    RxLen = (RX_DESC_STAT(RxDescIndex) >> 16) & 0x03FFF; 
	rptr = 	(unsigned short *)RX_BUFADDR(RxDescIndex);
	return(RxLen);
  }
  return 0;

}

void EndReadFrame(void) {

  RX_DESC_STAT(RxDescIndex) = OWN_BIT;
  RxDescIndex++;
  if (RxDescIndex == NUM_RX_DESC)
	RxDescIndex = 0;
}

unsigned int CheckFrameReceived(void) {             // Packet received ?

  if ((RX_DESC_STAT(RxDescIndex) & OWN_BIT) == 0) 		
    return(1);
  else 
    return(0);
}

// requests space in EMAC memory for storing an outgoing frame

void RequestSend(unsigned short FrameSize)
{
  tptr = (unsigned short *)TX_BUFADDR(TxDescIndex);
  TX_DESC_CTRL(TxDescIndex)	= FrameSize;
}

// check if ethernet controller is ready to accept the
// frame we want to send

unsigned int Rdy4Tx(void)
{
  return (1);   // the ethernet controller transmits much faster
}               // than the CPU can load its buffers


// writes a word in little-endian byte order to TX_BUFFER
void WriteFrame_EMAC(unsigned short Data)
{
  *tptr++ = Data;
}

// copies bytes from MCU-memory to frame port
// NOTES: * an odd number of byte may only be transfered
//          if the frame is written to the end!
//        * MCU-memory MUST start at word-boundary

void CopyToFrame_EMAC(void *Source, unsigned int Size)
{
  unsigned short * piSource;
//  unsigned int idx;

  piSource = Source;
  Size = (Size + 1) & 0xFFFE;    // round Size up to next even number
  while (Size > 0) {
    WriteFrame_EMAC(*piSource++);
    Size -= 2;
  }
  TX_DESC_STAT(TxDescIndex) |= OWN_BIT;
  LPC_ETHERNET->DMA_TRANS_POLL_DEMAND = 1;   //  Wake Up the DMA if it's in Suspended Mode
  TxDescIndex++;
  if (TxDescIndex == NUM_TX_DESC)
    TxDescIndex = 0;
}

#endif

