// Analysis-only stand-in for Microchip XC32 <xc.h>: just the SFRs dcd_pic.c and dcd_pic32mz.c use.
// Bit positions follow the PIC32 Family Reference Manual Section 27 (USB OTG) and DS60001320H (PIC32MZ EF).
#ifndef PVS_STUB_XC_H
#define PVS_STUB_XC_H

#include <stdint.h>

typedef uint32_t xc_sfr_t;

#define XC_SFR(name) \
  extern volatile xc_sfr_t name; \
  extern volatile xc_sfr_t name##CLR; \
  extern volatile xc_sfr_t name##SET; \
  extern volatile xc_sfr_t name##INV

XC_SFR(U1OTGIR); XC_SFR(U1OTGIE); XC_SFR(U1PWRC); XC_SFR(U1IR); XC_SFR(U1IE); XC_SFR(U1EIR); XC_SFR(U1STAT);
XC_SFR(U1CON); XC_SFR(U1ADDR); XC_SFR(U1BDTP1); XC_SFR(U1BDTP2); XC_SFR(U1BDTP3); XC_SFR(U1EP0);
XC_SFR(U2EP0);
XC_SFR(IEC0); XC_SFR(IEC1); XC_SFR(IEC7);
XC_SFR(IFS0); XC_SFR(IFS1); XC_SFR(IFS4); XC_SFR(IFS7);

typedef struct { xc_sfr_t USBPWR:1, USUSPND:1, :1, USBBUSY:1, USLPGRD:1, :2, UACTPND:1; } U1PWRCbits_t;
typedef struct { xc_sfr_t USBEN:1, PPBRST:1, RESUME:1, HOSTEN:1, USBRST:1, PKTDIS:1, SE0:1, JSTATE:1; } U1CONbits_t;
typedef struct { xc_sfr_t USBIE:1, USB1IE:1, USB2IE:1; } IECbits_t;

extern volatile U1PWRCbits_t U1PWRCbits;
extern volatile U1CONbits_t U1CONbits;
extern volatile IECbits_t IEC0bits, IEC1bits, IEC7bits;

#define _U1OTGIR_ACTVIF_MASK        0x10u
#define _U1OTGIE_ACTVIE_MASK        0x10u
#define _U1PWRC_USBPWR_MASK         0x01u
#define _U1PWRC_USUSPEND_MASK       0x02u
#define _U1IR_URSTIF_MASK           0x01u
#define _U1IR_UERRIF_MASK           0x02u
#define _U1IR_SOFIF_MASK            0x04u
#define _U1IR_TRNIF_MASK            0x08u
#define _U1IR_IDLEIF_MASK           0x10u
#define _U1IR_STALLIF_MASK          0x80u
#define _U1IE_URSTIE_MASK           0x01u
#define _U1IE_UERRIE_MASK           0x02u
#define _U1IE_TRNIE_MASK            0x08u
#define _U1IE_IDLEIE_MASK           0x10u
#define _U1IE_RESUMEIE_MASK         0x20u
#define _U1IE_STALLIE_MASK          0x80u
#define _U1STAT_PPBI_MASK           0x04u
#define _U1STAT_DIR_POSITION        3
#define _U1STAT_DIR_MASK            0x08u
#define _U1STAT_ENDPT0_POSITION     4
#define _U1CON_USBEN_SOFEN_MASK     0x01u
#define _U1CON_PPBRST_MASK          0x02u
#define _U1CON_RESUME_MASK          0x04u
#define _U1CON_PKTDIS_TOKBUSY_MASK  0x20u
#define _U1EP0_EPHSHK_MASK          0x01u
#define _U1EP0_EPSTALL_MASK         0x02u
#define _U1EP0_EPTXEN_MASK          0x04u
#define _U1EP0_EPRXEN_MASK          0x08u
#define _U1EP0_EPCONDIS_MASK        0x10u

// Interrupt controller bits: positions vary per device, any distinct bit serves analysis
#define _IEC0_USBIE_MASK            0x01u
#define _IEC1_USBIE_MASK            0x01u
#define _IEC1_USB1IE_MASK           0x02u
#define _IEC7_USB2IE_MASK           0x04u
#define _IFS0_USBIF_MASK            0x01u
#define _IFS1_USBIF_MASK            0x01u
#define _IFS1_USB1IF_MASK           0x02u
#define _IFS7_USB2IF_MASK           0x04u

// PIC32MZ Hi-Speed USB
#define _USB_BASE_ADDRESS           0xBF8E3000u
#define _IFS4_USBIF_MASK            0x10u
#define _CP0_STATUS_EXL_MASK        0x00000002u
#define _CP0_STATUS_IPL_MASK        0x0001FC00u
unsigned int _CP0_GET_STATUS(void);

typedef struct {
  uint32_t :1, EP1RXIE:1, EP2RXIE:1, EP3RXIE:1, EP4RXIE:1, EP5RXIE:1, EP6RXIE:1, EP7RXIE:1, :8;
  uint32_t SUSPIF:1, RESUMEIF:1, RESETIF:1, SOFIF:1, CONNIF:1, DISCONIF:1, SESSRQIF:1, VBUSERRIF:1;
  uint32_t SUSPIE:1, RESUMEIE:1, RESETIE:1, SOFIE:1, CONNIE:1, DISCONIE:1, SESSRQIE:1, VBUSERRIE:1;
} __USBCSR2bits_t;
typedef struct {
  uint32_t USBWKUPEN:1, USBRIE:1, USBIE:1, SENDMONEN:1, BSVALMONEN:1, ASVALMONEN:1, VBUSMONEN:1, PHYIDEN:1;
  uint32_t USBIDVAL:1, USBIDOVEN:1, :14, USBWKUP:1, USBRF:1, USBIF:1;
} __USBCRCONbits_t;
extern volatile __USBCSR2bits_t USBCSR2bits;
extern volatile __USBCRCONbits_t USBCRCONbits;

#endif
