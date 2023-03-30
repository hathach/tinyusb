/* 
 * The MIT License (MIT)
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#include "tusb_option.h"
#include "device/dcd.h"
#include "ch32_usbfs_reg.h"

#define CHECK_ALIGNED(x) { if(((uint32_t)x) &3) TU_ASSERT(0,);}
LN_USB_OTG_DEVICE *USBOTGD =    (LN_USB_OTG_DEVICE *) USBOTG_BASE;

#if 0
    #define LDEBUG Logger
#else
    #define LDEBUG(...)     {}
#endif

volatile uint8_t ep0_tx_tog = 0x01;
volatile uint8_t ep0_rx_tog = 0x01; 

int tog_ko_out=0; /*-*/
int tog_ko_in=0;



static uint8_t *getBufferAddress(int x, bool dir);
// Max number of bi-directional endpoints including EP0
#define EP_MAX 8
#define MAXIMUM_PACKET_LEN  64
#define SPEED_BITS          USBOTG_FULL_SPEED
#define REPORTED_SPEED      TUSB_SPEED_FULL

// Max number of bi-directional endpoints including EP0

typedef struct {
    bool     active;
    uint8_t  *buffer;
    uint16_t total_len;
    uint16_t xfered_so_far;
    uint16_t max_size;
    uint16_t current_transfer;
} xfer_ctl_t;

typedef struct {
    xfer_ctl_t eps[EP_MAX*2];  // out then in
    uint8_t    buffer[EP_MAX][128]; // 2* 64 bytes out then in
} all_xfer_t;

static int xmin(int a, int b)
{
    if(a<b) return a;
    return b;
}

#define XFER_CTL_BASE(_ep, _dir) (&(xfer_status.eps[_ep*2+_dir]))
static  __attribute__((aligned(4))) all_xfer_t xfer_status;

#include "dcd_usbfs_platform.h"


/**
*/
void dcd_edpt_close_all(uint8_t rhport) {
    (void)rhport;
}
/**
*/
/**
*/
void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
  TU_ASSERT(0,);  
}

/**
*/
void rxControl(int epnum, uint32_t mask, uint32_t set)
{
    uint32_t reg=USBOTGD->ep[epnum].rx_ctrl;
    reg&=~ (mask);
    reg|=set;
    USBOTGD->ep[epnum].rx_ctrl = reg;
}
void txControl(int epnum, uint32_t mask, uint32_t set)
{
    uint32_t reg=USBOTGD->ep[epnum].tx_ctrl;
    reg&=~ (mask);
    reg|=set;
    USBOTGD->ep[epnum].tx_ctrl = reg;
}
void txSet(int epnum, uint32_t val)
{
    USBOTGD->ep[epnum].tx_ctrl = val;
}
void rxSet(int epnum, uint32_t val)
{
    USBOTGD->ep[epnum].rx_ctrl = val;
}
void txLenSet(int epnum, uint32_t size)
{
    USBOTGD->ep[epnum].tx_length = size;
}

//------------

// Out then in
uint8_t *getBufferAddress(int x, bool is_in)
{
    TU_ASSERT( (x<EP_MAX), NULL);
    uint8_t *s = xfer_status.buffer[x];
    if(x)                   // shared TX/RX buffer for EP0
        return s+is_in*64;
    return s;
}
/**
0 not used
   1 4
   2 3
   5 6
   7
*/
//                          0         1   2   3  4  5 6  7    
const uint8_t  offset[8] ={ 0,        0,  1 , 1, 0, 2,2, 3};
const uint8_t  up[8]     ={ 0,        1,  0,  1, 0, 0,1, 0};

void setEndpointMode(int endpoint, bool mod, bool enable_tx, bool enable_rx)
{

    if(!endpoint) return;
    volatile uint8_t *adr = &(USBOTGD->mod[offset[endpoint]]);
    uint8_t value = *adr;
    if(up[endpoint])
    {
        value&=0x0F;
        value|=  ( mod + enable_tx*4+enable_rx*8)<<4;
    }else
    {
        value&=0xF0;
        value|= ( mod + enable_tx*4+enable_rx*8) ;
    }
    *adr=value;
}
/**
*/
void setEndpointDmaAddress(int endpoint, uint32_t adr)
{
    USBOTGD->dma[endpoint] = adr;
}

/**
*/
int usbd_ep_close(const uint8_t ep) {
    (void)ep;

    return 0;
}
/**
*/
void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
    (void)rhport;
    (void)ep_addr;

}
/**
*/
void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
    (void)rhport;
    (void)ep_addr;
   
}



/**
*/
void dcd_init(uint8_t rhport) {
    (void)rhport;

    // enable clock
    dcd_fs_platform_init();    
    // 
    LDEBUG("OTG FS driver\n");
    dcd_fs_platform_delay(1);
    USBOTGD->DEVICE_CTRL = 0;
    

    for(int i=0;i<EP_MAX;i++)
    {
        USBOTGD->dma[i]= (uint32_t )getBufferAddress(i,false);
    }
    XFER_CTL_BASE(0,0)->max_size = 64;
    XFER_CTL_BASE(0,1)->max_size = 64;

    USBOTGD->INT_FG = 0xFF;
    USBOTGD->INT_FG = 0xFF; // clear pending interrupts

    USBOTGD->CTRL = USBOTG_CTRL_RESET+USBOTG_CTRL_CLR_ALL;
    dcd_fs_platform_delay(1);
    USBOTGD->DEV_ADDRESS = 0;
    USBOTGD->CTRL = 0;
    USBOTGD->INT_EN = USBOTG_INT_EN_BUS_RESET_IE + USBOTG_INT_EN_TRANSFER_IE + USBOTG_INT_EN_SUSPEND_IE; // + USBOTG_INT_NAK_IE;
    USBOTGD->CTRL = USBOTG_CTRL_PULLUP_ENABLE+ USBOTG_CTRL_DMA_ENABLE + USBOTG_CTRL_INT_BUSY;


    txSet(0, USBOTG_EP_RES_NACK);
    rxSet(0, USBOTG_EP_RES_ACK);

    USBOTGD->INT_FG = 0xff; // clear interrupt (again)
    USBOTGD->DEVICE_CTRL = USBOTG_DEVICE_CTRL_ENABLE;

}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) 
{
    (void)dev_addr;
    // Response with zlp status
    dcd_edpt_xfer(rhport, 0x80, NULL, 0);
}


/**
*/
bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const *desc_edpt) {
    (void)rhport;

    uint8_t const epnum = tu_edpt_number(desc_edpt->bEndpointAddress);
    uint8_t const dir = tu_edpt_dir(desc_edpt->bEndpointAddress);

    TU_ASSERT( (epnum < EP_MAX) , false);

    xfer_ctl_t *xfer = XFER_CTL_BASE(epnum, dir);
    xfer->max_size = tu_edpt_packet_size(desc_edpt);

    if (epnum != 0) // ep0 is set automatically
    {
        if(desc_edpt->bEndpointAddress & TUSB_DIR_IN_MASK ) // IN ?
        {
            setEndpointMode(epnum,false, true,true);
            txLenSet(epnum, 0);
            txSet(epnum, USBOTG_EP_RES_AUTOTOG | USBOTG_EP_RES_NACK );
        }
        else
        {
            setEndpointMode(epnum,false, true,true);
            rxSet(epnum, USBOTG_EP_RES_AUTOTOG | USBOTG_EP_RES_NACK); // ready to receive
        }
    }
    return true;
}

/*
     ep_in => it's a write actually from the device point of view
*/
bool dcd_edpt_xfer_ep_in( xfer_ctl_t *xfer, uint8_t epnum) 
{
    LDEBUG("Prepare for IN with %d bytes on EP %d tog tx= %d\n",xfer->total_len, epnum,ep0_tx_tog);
    int short_packet_size = xmin(xfer->total_len, xfer->max_size);
    if(!epnum) // ep0
    {
        if(!xfer->total_len) // ep0 zlp
        {            
            txLenSet(0, 0);
        }else
        {
            xfer->current_transfer = short_packet_size;
            txLenSet(0, short_packet_size);
            memcpy( getBufferAddress(0,1), xfer->buffer, short_packet_size );            
        }
        txSet(0, USBOTG_EP_RES_ACK | (ep0_tx_tog * USBOTG_EP_RES_TOG1 ));
        return true;
    }
    // Other EP
    if(!xfer->total_len) // epx zlp
    {
        xfer->current_transfer = 0;
        txLenSet(epnum, 0);                
    }else
    {
        xfer->current_transfer = short_packet_size;        
        txLenSet(epnum,  short_packet_size);
        // copy to Tx dma
        memcpy( getBufferAddress(epnum,1), xfer->buffer, short_packet_size );        
    }
    txControl(epnum,USBOTG_EP_RES_MASK ,USBOTG_EP_RES_ACK); // go!
    return true;
}
/*
     ep_out => it's a read actually from the device point of view
*/
bool dcd_edpt_xfer_ep_out( xfer_ctl_t *xfer, uint8_t epnum) 
{
    LDEBUG("Prepare for OUT with %d bytes on EP %d tog_rx=%d\n",xfer->total_len, epnum,ep0_rx_tog);
    if (!epnum) // ep0
    {
        if(xfer->total_len)
        {           
            if(xfer->total_len>xfer->max_size)
            {
                TU_ASSERT(0, false);
            }
        }        
        rxSet(0,USBOTG_EP_RES_ACK | ep0_rx_tog*USBOTG_EP_RES_TOG1);
        return true;
    }
    // other op
    rxControl(epnum,USBOTG_EP_RES_MASK ,USBOTG_EP_RES_ACK  );
    return true;
}
           
/**
*/
bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes) {
    (void)rhport;
  
    
    uint8_t const epnum = tu_edpt_number(ep_addr);
    uint8_t const dir = tu_edpt_dir(ep_addr);

    xfer_ctl_t *xfer = XFER_CTL_BASE(epnum, dir);
    if(xfer->active)
    {
        //TU_ASSERT(0);
    }
    xfer->active=true;
    xfer->buffer = buffer;    
    xfer->total_len = total_bytes;
    xfer->xfered_so_far = 0;
    xfer->current_transfer = 0;
    
    if(dir) // ep_in
    {
        return dcd_edpt_xfer_ep_in(xfer, epnum );
    }
    else 
    {
        return dcd_edpt_xfer_ep_out(xfer, epnum );
    }
}
//
//
//
void dcd_int_reset(void)
{
    USBOTGD->INT_EN &= ~USBOTG_INT_EN_BUS_RESET_IE;
    XFER_CTL_BASE(0,0)->max_size = 64;
    XFER_CTL_BASE(0,1)->max_size = 64;
  
    XFER_CTL_BASE(0,0)->active = false;
    XFER_CTL_BASE(0,1)->active = false;

    rxSet(0, USBOTG_EP_RES_NACK );
    txSet(0, USBOTG_EP_RES_NACK );
    txLenSet(0,0);

    for(int i=1;i<EP_MAX;i++) // ep0 is not configurable
    {
        setEndpointMode(i,false, false, false);        
    }

    // init ep to a safe value
    USBOTGD->dma[0] = (uint32_t )getBufferAddress(0,false);
    for(int ep = 1; ep< EP_MAX;ep++)
    {
        USBOTGD->dma[ep] = (uint32_t )getBufferAddress(ep,false); 
        rxSet(ep, USBOTG_EP_RES_NYET | USBOTG_EP_RES_AUTOTOG );
        txSet(ep, USBOTG_EP_RES_NYET | USBOTG_EP_RES_AUTOTOG );               
    }
    ep0_tx_tog = true;
    ep0_rx_tog = true;
   
    dcd_event_bus_reset(0, TUSB_SPEED_FULL, true);
}
/**
*/
void dcd_edpt0_status_complete(uint8_t rhport, tusb_control_request_t const *request) 
{
    (void)rhport;

    if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
        request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
        request->bRequest == TUSB_REQ_SET_ADDRESS) 
    {
        USBOTGD->DEV_ADDRESS = (uint8_t)request->wValue;
    }
}


/**
    It's ep_out so it's a read
*/
void dcd_int_out0(void)
{
    int rx_len = USBOTGD->RX_LEN;    
    xfer_ctl_t *xfer = XFER_CTL_BASE(0, false);    
    
    if(!xfer->total_len) // zlp
    {     
        rxSet(0, USBOTG_EP_RES_NACK +  + ep0_rx_tog * USBOTG_EP_RES_TOG1);  // done
        ep0_rx_tog ^= 1;
        dcd_event_xfer_complete(0, 0, xfer->xfered_so_far, XFER_RESULT_SUCCESS, true);
        xfer->active=false;
        return;
    }
    if(rx_len)
    {
        if(xfer->buffer==NULL)
        {
            TU_ASSERT(0,);
        }else
        {
            // copy!
            memcpy( xfer->buffer+ xfer->xfered_so_far, getBufferAddress(0, false), rx_len);
            xfer->xfered_so_far += rx_len;
        }
    }
   

    if(rx_len==xfer->max_size) // more ?
    {
        rxSet(0,  USBOTG_EP_RES_ACK + ep0_rx_tog * USBOTG_EP_RES_TOG1);
    }else
    {
        rxSet(0,  USBOTG_EP_RES_NACK + ep0_rx_tog * USBOTG_EP_RES_TOG1);
        xfer->active=false;
        dcd_event_xfer_complete(0, 0, xfer->xfered_so_far, XFER_RESULT_SUCCESS, true);
    }
    
}
// out-> write
void dcd_int_out(int end_num)
{
    xfer_ctl_t *xfer = XFER_CTL_BASE(end_num,false);    
    int rx_len = USBOTGD->RX_LEN;

    // copy from DMA area to final buffer
    memcpy( xfer->buffer+ xfer->xfered_so_far, getBufferAddress(end_num, false), rx_len);
    xfer->xfered_so_far += rx_len;
    // More ?
    if ( rx_len < xfer->max_size ||  xfer->xfered_so_far==xfer->total_len)
    {
        // nope
        rxControl(end_num, USBOTG_EP_RES_MASK, USBOTG_EP_RES_NACK ); 
        dcd_event_xfer_complete(0, end_num, xfer->xfered_so_far, XFER_RESULT_SUCCESS, true);       
    }else // yes more
    {
        rxControl(end_num,USBOTG_EP_RES_MASK ,USBOTG_EP_RES_ACK);
    }    
}
//
// ep_in, it's a write
//
void dcd_int_in0(void)
{    
    xfer_ctl_t *xfer = XFER_CTL_BASE(0, 1);                    
    // assume one transfer is enough (?)
    xfer->xfered_so_far += xfer->current_transfer;            
    if(!xfer->total_len)
    {
        txSet(0, USBOTG_EP_RES_NACK +  + ep0_tx_tog * USBOTG_EP_RES_TOG1);  // done
        xfer->active=false;
        dcd_event_xfer_complete(0, TUSB_DIR_IN_MASK  , xfer->xfered_so_far, XFER_RESULT_SUCCESS, true);
        return;
    }
    ep0_tx_tog ^=1;  
    int left = xfer->total_len-xfer->xfered_so_far;
    if(left==0) // done!
    {        
        txSet(0, USBOTG_EP_RES_NACK +  + ep0_tx_tog * USBOTG_EP_RES_TOG1);  // done
        xfer->active=false;
        dcd_event_xfer_complete(0, TUSB_DIR_IN_MASK  , xfer->xfered_so_far, XFER_RESULT_SUCCESS, true);           
    }else // next
    {
        TU_ASSERT(0,);
        txSet(0,  USBOTG_EP_RES_ACK + ep0_tx_tog * USBOTG_EP_RES_TOG1); //
    }
    return;    
}
// in => tx
void dcd_int_in(int end_num)
{
    uint8_t endp = end_num |  TUSB_DIR_IN_MASK ;
    xfer_ctl_t *xfer = XFER_CTL_BASE(end_num, 1);                    
    // finish or queue the next one ?
    xfer->xfered_so_far+=xfer->current_transfer;
    xfer->current_transfer = 0;
    int left = xfer->total_len-xfer->xfered_so_far;
    if(left>0)
    {
        left = xmin(left,xfer->max_size);
        memcpy( getBufferAddress(end_num, true), xfer->buffer+xfer->xfered_so_far, left);
        xfer->current_transfer = left;
        txLenSet(end_num, left);
        txControl(end_num, USBOTG_EP_RES_MASK, USBOTG_EP_RES_ACK);
    }else
    {
        txControl(end_num, USBOTG_EP_RES_MASK, USBOTG_EP_RES_NACK);
        dcd_event_xfer_complete(0, endp , xfer->xfered_so_far, XFER_RESULT_SUCCESS, true);
    }    
}

/**
*/
void dcd_int_handler(uint8_t rhport) 
{
    (void)rhport;

    
    uint8_t fg = USBOTGD->INT_FG;
    uint8_t st = USBOTGD->INT_ST;

    //
    if(fg &  USBOTG_INT_FG_BUS_RESET)
    {
        dcd_int_reset();
        USBOTGD->INT_FG = USBOTG_INT_FG_BUS_RESET +(1<<3); // CLEAR SOF too
        return;
    }
    //
    if(fg & USBOTG_INT_FG_TRANSFER_COMPLETE)
    {
            int token_type = (st >> 4)&3; // 00 OUT Packet, 1 SOF, 2 IN packet , 3 SETUP packet
            int end_num = st & 0xf;            

            switch(token_type)
            {
                case PID_SOF:
                    break;
                case PID_OUT : // it's a read 
                    if(st & USBOTG_INT_ST_TOG_OK)
                    {
                        if(!end_num)
                            dcd_int_out0();
                        else
                            dcd_int_out(end_num);
                    }else
                    {
                        tog_ko_out++;
                    }
                    break;
                case PID_IN : // IN, it's a write
                    if(st & USBOTG_INT_ST_TOG_OK)
                    {
                        if(!end_num)
                            dcd_int_in0();
                        else
                            dcd_int_in(end_num);;
                    }else
                    {
                        tog_ko_in++;
                    }
                    break;
                case PID_SETUP : // SETUP
                    ep0_tx_tog = 1;
                    ep0_rx_tog = 1;
                    dcd_event_setup_received(0, getBufferAddress(0,false), true);     
                    break;
                default:
                    TU_ASSERT(0,);
                    break;
            }
            USBOTGD->INT_FG = USBOTG_INT_FG_TRANSFER_COMPLETE;
            return;      
                    
    }
    if(fg & USBOTG_INT_FG_SUSPEND)
    {
        USBOTGD->INT_FG = USBOTG_INT_FG_SUSPEND;
        return;      
    }
    TU_ASSERT(0,);
}


