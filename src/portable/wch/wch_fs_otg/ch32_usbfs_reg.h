
#define USBOTG_BASE 0x50000000

typedef struct
{
    uint16_t tx_length;    
    uint8_t  tx_ctrl;
    uint8_t  rx_ctrl;
}endpoints;



typedef struct
{
    uint8_t CTRL;       // 00
    uint8_t DEVICE_CTRL;// 01
    uint8_t INT_EN;     // 02
    uint8_t DEV_ADDRESS;// 03
    
    uint8_t STATUS;      // 04 -unsused ?
    uint8_t MIS_ST;      // 05
    uint8_t INT_FG;      // 06
    uint8_t INT_ST;      // 07

    uint16_t RX_LEN;     // 08
    uint16_t padder2;    // 0A
    uint8_t  mod[4];     // 0C 41 23 56 7_
    uint32_t dma[8];     //0x10
    endpoints ep[8];    //0x30

    uint32_t padder;     // 50
    uint32_t OTG_CR;     // 54
    uint32_t OTG_SR;     // 58
} LN_USB_OTG_DEVICEx ;

typedef volatile LN_USB_OTG_DEVICEx LN_USB_OTG_DEVICE;

#define USBOTG_CTRL_DMA_ENABLE              (1<<0)
#define USBOTG_CTRL_CLR_ALL                 (1<<1)
#define USBOTG_CTRL_RESET                   (1<<2)
#define USBOTG_CTRL_INT_BUSY                (1<<3)
#define USBOTG_CTRL_PULLUP_ENABLE           (1<<5)
#define USBOTG_CTRL_DEVICE_MODE             0

#define USBOTG_CTRL_VALUE               ( USBOTG_CTRL_DEVICE_MODE+ USBOTG_CTRL_DMA_ENABLE +USBOTG_CTRL_PULLUP_ENABLE)
#define USBOTG_CTRL_VALUE_RESET         ( USBOTG_CTRL_VALUE+ USBOTG_CTRL_RESET+USBOTG_CTRL_CLR_ALL )



#define USBOTG_DEVICE_CTRL_ENABLE           (1<<0)


#define USBOTG_INT_EN_BUS_RESET_IE          (1<<0)
#define USBOTG_INT_EN_TRANSFER_IE           (1<<1)
#define USBOTG_INT_EN_SUSPEND_IE            (1<<2)
#define USBOTG_INT_EN_NAK_IE                (1<<6)
#define USBOTG_INT_EN_SOF_IE                (1<<7)

// #define USBOTG_MIST_DEV_ATTACHED            (1<<0) host mode
// #define USBOTG_MIST_DM_LEVEL_STATUS         (1<<1) host mode
#define USBOTG_MIST_SUSPEND_STATUS          (1<<2)
#define USBOTG_MIST_BUS_RESET_STATUS        (1<<3)
#define USBOTG_MIST_RX_NOT_EMPTY_STATUS     (1<<4)
#define USBOTG_MIST_RX_NOT_BUSY_STATUS      (1<<5)
// 6 &7 -> host mode
#define USBOTG_INT_FG_BUS_RESET             (1<<0)
#define USBOTG_INT_FG_TRANSFER_COMPLETE     (1<<1)
#define USBOTG_INT_FG_SUSPEND               (1<<2)
#define USBOTG_INT_FG_TOG_OK                (1<<6)
#define USBOTG_INT_FG_NAK                   (1<<7)


#define USBOTG_INT_ST_CURRENT_ENDP_MASK     (0xf)
#define USBOTG_INT_ST_TOKEN_PID_MASK        (0x30)
#define USBOTG_INT_ST_TOG_OK                (1<<6)

#define USBOTG_EP_RES_MASK                  (3<<0)
#define USBOTG_EP_RES_ACK                   (0<<0)
#define USBOTG_EP_RES_NACK                  (2<<0)
#define USBOTG_EP_RES_NYET                  (1<<0)
#define USBOTG_EP_RES_STALL                 (3<<0)
#define USBOTG_EP_RES_NAK                   USBOTG_EP_RES_NACK

#define USBOTG_EP_RES_AUTOTOG               (1<<3)
#define USBOTG_EP_RES_TOG1                  (1<<2)
#define USBOTG_EP_RES_TOG0                  (0<<2)


// 00: OUT, 01:SOF, 10:IN, 11:SETUP
#define PID_OUT   0
#define PID_SOF   1
#define PID_IN    2
#define PID_SETUP 3


