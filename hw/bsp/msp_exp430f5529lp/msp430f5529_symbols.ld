/* ============================================================================ */
/* Copyright (c) 2019, Texas Instruments Incorporated                           */
/*  All rights reserved.                                                        */
/*                                                                              */
/*  Redistribution and use in source and binary forms, with or without          */
/*  modification, are permitted provided that the following conditions          */
/*  are met:                                                                    */
/*                                                                              */
/*  *  Redistributions of source code must retain the above copyright           */
/*     notice, this list of conditions and the following disclaimer.            */
/*                                                                              */
/*  *  Redistributions in binary form must reproduce the above copyright        */
/*     notice, this list of conditions and the following disclaimer in the      */
/*     documentation and/or other materials provided with the distribution.     */
/*                                                                              */
/*  *  Neither the name of Texas Instruments Incorporated nor the names of      */
/*     its contributors may be used to endorse or promote products derived      */
/*     from this software without specific prior written permission.            */
/*                                                                              */
/*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" */
/*  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,       */
/*  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR      */
/*  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR            */
/*  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,       */
/*  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,         */
/*  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; */
/*  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,    */
/*  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR     */
/*  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,              */
/*  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                          */
/* ============================================================================ */

/* This file supports MSP430F5529 devices. */
/* Version: 1.207 */

/************************************************************
* STANDARD BITS
************************************************************/
/************************************************************
* STATUS REGISTER BITS
************************************************************/
/************************************************************
* PERIPHERAL FILE MAP
************************************************************/
/************************************************************
* ADC12 PLUS
************************************************************/
PROVIDE(ADC12CTL0          = 0x0700);
PROVIDE(ADC12CTL0_L        = 0x0700);
PROVIDE(ADC12CTL0_H        = 0x0701);
PROVIDE(ADC12CTL1          = 0x0702);
PROVIDE(ADC12CTL1_L        = 0x0702);
PROVIDE(ADC12CTL1_H        = 0x0703);
PROVIDE(ADC12CTL2          = 0x0704);
PROVIDE(ADC12CTL2_L        = 0x0704);
PROVIDE(ADC12CTL2_H        = 0x0705);
PROVIDE(ADC12IFG           = 0x070A);
PROVIDE(ADC12IFG_L         = 0x070A);
PROVIDE(ADC12IFG_H         = 0x070B);
PROVIDE(ADC12IE            = 0x070C);
PROVIDE(ADC12IE_L          = 0x070C);
PROVIDE(ADC12IE_H          = 0x070D);
PROVIDE(ADC12IV            = 0x070E);
PROVIDE(ADC12IV_L          = 0x070E);
PROVIDE(ADC12IV_H          = 0x070F);
PROVIDE(ADC12MEM0          = 0x0720);
PROVIDE(ADC12MEM0_L        = 0x0720);
PROVIDE(ADC12MEM0_H        = 0x0721);
PROVIDE(ADC12MEM1          = 0x0722);
PROVIDE(ADC12MEM1_L        = 0x0722);
PROVIDE(ADC12MEM1_H        = 0x0723);
PROVIDE(ADC12MEM2          = 0x0724);
PROVIDE(ADC12MEM2_L        = 0x0724);
PROVIDE(ADC12MEM2_H        = 0x0725);
PROVIDE(ADC12MEM3          = 0x0726);
PROVIDE(ADC12MEM3_L        = 0x0726);
PROVIDE(ADC12MEM3_H        = 0x0727);
PROVIDE(ADC12MEM4          = 0x0728);
PROVIDE(ADC12MEM4_L        = 0x0728);
PROVIDE(ADC12MEM4_H        = 0x0729);
PROVIDE(ADC12MEM5          = 0x072A);
PROVIDE(ADC12MEM5_L        = 0x072A);
PROVIDE(ADC12MEM5_H        = 0x072B);
PROVIDE(ADC12MEM6          = 0x072C);
PROVIDE(ADC12MEM6_L        = 0x072C);
PROVIDE(ADC12MEM6_H        = 0x072D);
PROVIDE(ADC12MEM7          = 0x072E);
PROVIDE(ADC12MEM7_L        = 0x072E);
PROVIDE(ADC12MEM7_H        = 0x072F);
PROVIDE(ADC12MEM8          = 0x0730);
PROVIDE(ADC12MEM8_L        = 0x0730);
PROVIDE(ADC12MEM8_H        = 0x0731);
PROVIDE(ADC12MEM9          = 0x0732);
PROVIDE(ADC12MEM9_L        = 0x0732);
PROVIDE(ADC12MEM9_H        = 0x0733);
PROVIDE(ADC12MEM10         = 0x0734);
PROVIDE(ADC12MEM10_L       = 0x0734);
PROVIDE(ADC12MEM10_H       = 0x0735);
PROVIDE(ADC12MEM11         = 0x0736);
PROVIDE(ADC12MEM11_L       = 0x0736);
PROVIDE(ADC12MEM11_H       = 0x0737);
PROVIDE(ADC12MEM12         = 0x0738);
PROVIDE(ADC12MEM12_L       = 0x0738);
PROVIDE(ADC12MEM12_H       = 0x0739);
PROVIDE(ADC12MEM13         = 0x073A);
PROVIDE(ADC12MEM13_L       = 0x073A);
PROVIDE(ADC12MEM13_H       = 0x073B);
PROVIDE(ADC12MEM14         = 0x073C);
PROVIDE(ADC12MEM14_L       = 0x073C);
PROVIDE(ADC12MEM14_H       = 0x073D);
PROVIDE(ADC12MEM15         = 0x073E);
PROVIDE(ADC12MEM15_L       = 0x073E);
PROVIDE(ADC12MEM15_H       = 0x073F);
PROVIDE(ADC12MCTL0         = 0x0710);
PROVIDE(ADC12MCTL1         = 0x0711);
PROVIDE(ADC12MCTL2         = 0x0712);
PROVIDE(ADC12MCTL3         = 0x0713);
PROVIDE(ADC12MCTL4         = 0x0714);
PROVIDE(ADC12MCTL5         = 0x0715);
PROVIDE(ADC12MCTL6         = 0x0716);
PROVIDE(ADC12MCTL7         = 0x0717);
PROVIDE(ADC12MCTL8         = 0x0718);
PROVIDE(ADC12MCTL9         = 0x0719);
PROVIDE(ADC12MCTL10        = 0x071A);
PROVIDE(ADC12MCTL11        = 0x071B);
PROVIDE(ADC12MCTL12        = 0x071C);
PROVIDE(ADC12MCTL13        = 0x071D);
PROVIDE(ADC12MCTL14        = 0x071E);
PROVIDE(ADC12MCTL15        = 0x071F);
/************************************************************
* Comparator B
************************************************************/
PROVIDE(CBCTL0             = 0x08C0);
PROVIDE(CBCTL0_L           = 0x08C0);
PROVIDE(CBCTL0_H           = 0x08C1);
PROVIDE(CBCTL1             = 0x08C2);
PROVIDE(CBCTL1_L           = 0x08C2);
PROVIDE(CBCTL1_H           = 0x08C3);
PROVIDE(CBCTL2             = 0x08C4);
PROVIDE(CBCTL2_L           = 0x08C4);
PROVIDE(CBCTL2_H           = 0x08C5);
PROVIDE(CBCTL3             = 0x08C6);
PROVIDE(CBCTL3_L           = 0x08C6);
PROVIDE(CBCTL3_H           = 0x08C7);
PROVIDE(CBINT              = 0x08CC);
PROVIDE(CBINT_L            = 0x08CC);
PROVIDE(CBINT_H            = 0x08CD);
PROVIDE(CBIV               = 0x08CE);
/*************************************************************
* CRC Module
*************************************************************/
PROVIDE(CRCDI              = 0x0150);
PROVIDE(CRCDI_L            = 0x0150);
PROVIDE(CRCDI_H            = 0x0151);
PROVIDE(CRCDIRB            = 0x0152);
PROVIDE(CRCDIRB_L          = 0x0152);
PROVIDE(CRCDIRB_H          = 0x0153);
PROVIDE(CRCINIRES          = 0x0154);
PROVIDE(CRCINIRES_L        = 0x0154);
PROVIDE(CRCINIRES_H        = 0x0155);
PROVIDE(CRCRESR            = 0x0156);
PROVIDE(CRCRESR_L          = 0x0156);
PROVIDE(CRCRESR_H          = 0x0157);
/************************************************************
* DMA_X
************************************************************/
PROVIDE(DMACTL0            = 0x0500);
PROVIDE(DMACTL1            = 0x0502);
PROVIDE(DMACTL2            = 0x0504);
PROVIDE(DMACTL3            = 0x0506);
PROVIDE(DMACTL4            = 0x0508);
PROVIDE(DMAIV              = 0x050E);
PROVIDE(DMA0CTL            = 0x0510);
PROVIDE(DMA0SA             = 0x0512);
PROVIDE(DMA0SAL            = 0x0512);
PROVIDE(DMA0DA             = 0x0516);
PROVIDE(DMA0DAL            = 0x0516);
PROVIDE(DMA0SZ             = 0x051A);
PROVIDE(DMA1CTL            = 0x0520);
PROVIDE(DMA1SA             = 0x0522);
PROVIDE(DMA1SAL            = 0x0522);
PROVIDE(DMA1DA             = 0x0526);
PROVIDE(DMA1DAL            = 0x0526);
PROVIDE(DMA1SZ             = 0x052A);
PROVIDE(DMA2CTL            = 0x0530);
PROVIDE(DMA2SA             = 0x0532);
PROVIDE(DMA2SAL            = 0x0532);
PROVIDE(DMA2DA             = 0x0536);
PROVIDE(DMA2DAL            = 0x0536);
PROVIDE(DMA2SZ             = 0x053A);
/*************************************************************
* Flash Memory
*************************************************************/
PROVIDE(FCTL1              = 0x0140);
PROVIDE(FCTL1_L            = 0x0140);
PROVIDE(FCTL1_H            = 0x0141);
PROVIDE(FCTL3              = 0x0144);
PROVIDE(FCTL3_L            = 0x0144);
PROVIDE(FCTL3_H            = 0x0145);
PROVIDE(FCTL4              = 0x0146);
PROVIDE(FCTL4_L            = 0x0146);
PROVIDE(FCTL4_H            = 0x0147);
/************************************************************
* HARDWARE MULTIPLIER 32Bit
************************************************************/
PROVIDE(MPY                = 0x04C0);
PROVIDE(MPY_L              = 0x04C0);
PROVIDE(MPY_H              = 0x04C1);
PROVIDE(MPYS               = 0x04C2);
PROVIDE(MPYS_L             = 0x04C2);
PROVIDE(MPYS_H             = 0x04C3);
PROVIDE(MAC                = 0x04C4);
PROVIDE(MAC_L              = 0x04C4);
PROVIDE(MAC_H              = 0x04C5);
PROVIDE(MACS               = 0x04C6);
PROVIDE(MACS_L             = 0x04C6);
PROVIDE(MACS_H             = 0x04C7);
PROVIDE(OP2                = 0x04C8);
PROVIDE(OP2_L              = 0x04C8);
PROVIDE(OP2_H              = 0x04C9);
PROVIDE(RESLO              = 0x04CA);
PROVIDE(RESLO_L            = 0x04CA);
PROVIDE(RESLO_H            = 0x04CB);
PROVIDE(RESHI              = 0x04CC);
PROVIDE(RESHI_L            = 0x04CC);
PROVIDE(RESHI_H            = 0x04CD);
PROVIDE(SUMEXT             = 0x04CE);
PROVIDE(SUMEXT_L           = 0x04CE);
PROVIDE(SUMEXT_H           = 0x04CF);
PROVIDE(MPY32L             = 0x04D0);
PROVIDE(MPY32L_L           = 0x04D0);
PROVIDE(MPY32L_H           = 0x04D1);
PROVIDE(MPY32H             = 0x04D2);
PROVIDE(MPY32H_L           = 0x04D2);
PROVIDE(MPY32H_H           = 0x04D3);
PROVIDE(MPYS32L            = 0x04D4);
PROVIDE(MPYS32L_L          = 0x04D4);
PROVIDE(MPYS32L_H          = 0x04D5);
PROVIDE(MPYS32H            = 0x04D6);
PROVIDE(MPYS32H_L          = 0x04D6);
PROVIDE(MPYS32H_H          = 0x04D7);
PROVIDE(MAC32L             = 0x04D8);
PROVIDE(MAC32L_L           = 0x04D8);
PROVIDE(MAC32L_H           = 0x04D9);
PROVIDE(MAC32H             = 0x04DA);
PROVIDE(MAC32H_L           = 0x04DA);
PROVIDE(MAC32H_H           = 0x04DB);
PROVIDE(MACS32L            = 0x04DC);
PROVIDE(MACS32L_L          = 0x04DC);
PROVIDE(MACS32L_H          = 0x04DD);
PROVIDE(MACS32H            = 0x04DE);
PROVIDE(MACS32H_L          = 0x04DE);
PROVIDE(MACS32H_H          = 0x04DF);
PROVIDE(OP2L               = 0x04E0);
PROVIDE(OP2L_L             = 0x04E0);
PROVIDE(OP2L_H             = 0x04E1);
PROVIDE(OP2H               = 0x04E2);
PROVIDE(OP2H_L             = 0x04E2);
PROVIDE(OP2H_H             = 0x04E3);
PROVIDE(RES0               = 0x04E4);
PROVIDE(RES0_L             = 0x04E4);
PROVIDE(RES0_H             = 0x04E5);
PROVIDE(RES1               = 0x04E6);
PROVIDE(RES1_L             = 0x04E6);
PROVIDE(RES1_H             = 0x04E7);
PROVIDE(RES2               = 0x04E8);
PROVIDE(RES2_L             = 0x04E8);
PROVIDE(RES2_H             = 0x04E9);
PROVIDE(RES3               = 0x04EA);
PROVIDE(RES3_L             = 0x04EA);
PROVIDE(RES3_H             = 0x04EB);
PROVIDE(MPY32CTL0          = 0x04EC);
PROVIDE(MPY32CTL0_L        = 0x04EC);
PROVIDE(MPY32CTL0_H        = 0x04ED);
/************************************************************
* DIGITAL I/O Port1/2 Pull up / Pull down Resistors
************************************************************/
PROVIDE(PAIN               = 0x0200);
PROVIDE(PAIN_L             = 0x0200);
PROVIDE(PAIN_H             = 0x0201);
PROVIDE(PAOUT              = 0x0202);
PROVIDE(PAOUT_L            = 0x0202);
PROVIDE(PAOUT_H            = 0x0203);
PROVIDE(PADIR              = 0x0204);
PROVIDE(PADIR_L            = 0x0204);
PROVIDE(PADIR_H            = 0x0205);
PROVIDE(PAREN              = 0x0206);
PROVIDE(PAREN_L            = 0x0206);
PROVIDE(PAREN_H            = 0x0207);
PROVIDE(PADS               = 0x0208);
PROVIDE(PADS_L             = 0x0208);
PROVIDE(PADS_H             = 0x0209);
PROVIDE(PASEL              = 0x020A);
PROVIDE(PASEL_L            = 0x020A);
PROVIDE(PASEL_H            = 0x020B);
PROVIDE(PAIES              = 0x0218);
PROVIDE(PAIES_L            = 0x0218);
PROVIDE(PAIES_H            = 0x0219);
PROVIDE(PAIE               = 0x021A);
PROVIDE(PAIE_L             = 0x021A);
PROVIDE(PAIE_H             = 0x021B);
PROVIDE(PAIFG              = 0x021C);
PROVIDE(PAIFG_L            = 0x021C);
PROVIDE(PAIFG_H            = 0x021D);
PROVIDE(P1IV               = 0x020E);
PROVIDE(P2IV               = 0x021E);
/************************************************************
* DIGITAL I/O Port3/4 Pull up / Pull down Resistors
************************************************************/
PROVIDE(PBIN               = 0x0220);
PROVIDE(PBIN_L             = 0x0220);
PROVIDE(PBIN_H             = 0x0221);
PROVIDE(PBOUT              = 0x0222);
PROVIDE(PBOUT_L            = 0x0222);
PROVIDE(PBOUT_H            = 0x0223);
PROVIDE(PBDIR              = 0x0224);
PROVIDE(PBDIR_L            = 0x0224);
PROVIDE(PBDIR_H            = 0x0225);
PROVIDE(PBREN              = 0x0226);
PROVIDE(PBREN_L            = 0x0226);
PROVIDE(PBREN_H            = 0x0227);
PROVIDE(PBDS               = 0x0228);
PROVIDE(PBDS_L             = 0x0228);
PROVIDE(PBDS_H             = 0x0229);
PROVIDE(PBSEL              = 0x022A);
PROVIDE(PBSEL_L            = 0x022A);
PROVIDE(PBSEL_H            = 0x022B);
/************************************************************
* DIGITAL I/O Port5/6 Pull up / Pull down Resistors
************************************************************/
PROVIDE(PCIN               = 0x0240);
PROVIDE(PCIN_L             = 0x0240);
PROVIDE(PCIN_H             = 0x0241);
PROVIDE(PCOUT              = 0x0242);
PROVIDE(PCOUT_L            = 0x0242);
PROVIDE(PCOUT_H            = 0x0243);
PROVIDE(PCDIR              = 0x0244);
PROVIDE(PCDIR_L            = 0x0244);
PROVIDE(PCDIR_H            = 0x0245);
PROVIDE(PCREN              = 0x0246);
PROVIDE(PCREN_L            = 0x0246);
PROVIDE(PCREN_H            = 0x0247);
PROVIDE(PCDS               = 0x0248);
PROVIDE(PCDS_L             = 0x0248);
PROVIDE(PCDS_H             = 0x0249);
PROVIDE(PCSEL              = 0x024A);
PROVIDE(PCSEL_L            = 0x024A);
PROVIDE(PCSEL_H            = 0x024B);
/************************************************************
* DIGITAL I/O Port7/8 Pull up / Pull down Resistors
************************************************************/
PROVIDE(PDIN               = 0x0260);
PROVIDE(PDIN_L             = 0x0260);
PROVIDE(PDIN_H             = 0x0261);
PROVIDE(PDOUT              = 0x0262);
PROVIDE(PDOUT_L            = 0x0262);
PROVIDE(PDOUT_H            = 0x0263);
PROVIDE(PDDIR              = 0x0264);
PROVIDE(PDDIR_L            = 0x0264);
PROVIDE(PDDIR_H            = 0x0265);
PROVIDE(PDREN              = 0x0266);
PROVIDE(PDREN_L            = 0x0266);
PROVIDE(PDREN_H            = 0x0267);
PROVIDE(PDDS               = 0x0268);
PROVIDE(PDDS_L             = 0x0268);
PROVIDE(PDDS_H             = 0x0269);
PROVIDE(PDSEL              = 0x026A);
PROVIDE(PDSEL_L            = 0x026A);
PROVIDE(PDSEL_H            = 0x026B);
/************************************************************
* DIGITAL I/O PortJ Pull up / Pull down Resistors
************************************************************/
PROVIDE(PJIN               = 0x0320);
PROVIDE(PJIN_L             = 0x0320);
PROVIDE(PJIN_H             = 0x0321);
PROVIDE(PJOUT              = 0x0322);
PROVIDE(PJOUT_L            = 0x0322);
PROVIDE(PJOUT_H            = 0x0323);
PROVIDE(PJDIR              = 0x0324);
PROVIDE(PJDIR_L            = 0x0324);
PROVIDE(PJDIR_H            = 0x0325);
PROVIDE(PJREN              = 0x0326);
PROVIDE(PJREN_L            = 0x0326);
PROVIDE(PJREN_H            = 0x0327);
PROVIDE(PJDS               = 0x0328);
PROVIDE(PJDS_L             = 0x0328);
PROVIDE(PJDS_H             = 0x0329);
/************************************************************
* PORT MAPPING CONTROLLER
************************************************************/
PROVIDE(PMAPKEYID          = 0x01C0);
PROVIDE(PMAPKEYID_L        = 0x01C0);
PROVIDE(PMAPKEYID_H        = 0x01C1);
PROVIDE(PMAPCTL            = 0x01C2);
PROVIDE(PMAPCTL_L          = 0x01C2);
PROVIDE(PMAPCTL_H          = 0x01C3);
/************************************************************
* PORT 4 MAPPING CONTROLLER
************************************************************/
PROVIDE(P4MAP01            = 0x01E0);
PROVIDE(P4MAP01_L          = 0x01E0);
PROVIDE(P4MAP01_H          = 0x01E1);
PROVIDE(P4MAP23            = 0x01E2);
PROVIDE(P4MAP23_L          = 0x01E2);
PROVIDE(P4MAP23_H          = 0x01E3);
PROVIDE(P4MAP45            = 0x01E4);
PROVIDE(P4MAP45_L          = 0x01E4);
PROVIDE(P4MAP45_H          = 0x01E5);
PROVIDE(P4MAP67            = 0x01E6);
PROVIDE(P4MAP67_L          = 0x01E6);
PROVIDE(P4MAP67_H          = 0x01E7);
/************************************************************
* PMM - Power Management System
************************************************************/
PROVIDE(PMMCTL0            = 0x0120);
PROVIDE(PMMCTL0_L          = 0x0120);
PROVIDE(PMMCTL0_H          = 0x0121);
PROVIDE(PMMCTL1            = 0x0122);
PROVIDE(PMMCTL1_L          = 0x0122);
PROVIDE(PMMCTL1_H          = 0x0123);
PROVIDE(SVSMHCTL           = 0x0124);
PROVIDE(SVSMHCTL_L         = 0x0124);
PROVIDE(SVSMHCTL_H         = 0x0125);
PROVIDE(SVSMLCTL           = 0x0126);
PROVIDE(SVSMLCTL_L         = 0x0126);
PROVIDE(SVSMLCTL_H         = 0x0127);
PROVIDE(SVSMIO             = 0x0128);
PROVIDE(SVSMIO_L           = 0x0128);
PROVIDE(SVSMIO_H           = 0x0129);
PROVIDE(PMMIFG             = 0x012C);
PROVIDE(PMMIFG_L           = 0x012C);
PROVIDE(PMMIFG_H           = 0x012D);
PROVIDE(PMMRIE             = 0x012E);
PROVIDE(PMMRIE_L           = 0x012E);
PROVIDE(PMMRIE_H           = 0x012F);
PROVIDE(PM5CTL0            = 0x0130);
PROVIDE(PM5CTL0_L          = 0x0130);
PROVIDE(PM5CTL0_H          = 0x0131);
/*************************************************************
* RAM Control Module
*************************************************************/
PROVIDE(RCCTL0             = 0x0158);
PROVIDE(RCCTL0_L           = 0x0158);
PROVIDE(RCCTL0_H           = 0x0159);
/************************************************************
* Shared Reference
************************************************************/
PROVIDE(REFCTL0            = 0x01B0);
PROVIDE(REFCTL0_L          = 0x01B0);
PROVIDE(REFCTL0_H          = 0x01B1);
/************************************************************
* Real Time Clock
************************************************************/
PROVIDE(RTCCTL01           = 0x04A0);
PROVIDE(RTCCTL01_L         = 0x04A0);
PROVIDE(RTCCTL01_H         = 0x04A1);
PROVIDE(RTCCTL23           = 0x04A2);
PROVIDE(RTCCTL23_L         = 0x04A2);
PROVIDE(RTCCTL23_H         = 0x04A3);
PROVIDE(RTCPS0CTL          = 0x04A8);
PROVIDE(RTCPS0CTL_L        = 0x04A8);
PROVIDE(RTCPS0CTL_H        = 0x04A9);
PROVIDE(RTCPS1CTL          = 0x04AA);
PROVIDE(RTCPS1CTL_L        = 0x04AA);
PROVIDE(RTCPS1CTL_H        = 0x04AB);
PROVIDE(RTCPS              = 0x04AC);
PROVIDE(RTCPS_L            = 0x04AC);
PROVIDE(RTCPS_H            = 0x04AD);
PROVIDE(RTCIV              = 0x04AE);
PROVIDE(RTCTIM0            = 0x04B0);
PROVIDE(RTCTIM0_L          = 0x04B0);
PROVIDE(RTCTIM0_H          = 0x04B1);
PROVIDE(RTCTIM1            = 0x04B2);
PROVIDE(RTCTIM1_L          = 0x04B2);
PROVIDE(RTCTIM1_H          = 0x04B3);
PROVIDE(RTCDATE            = 0x04B4);
PROVIDE(RTCDATE_L          = 0x04B4);
PROVIDE(RTCDATE_H          = 0x04B5);
PROVIDE(RTCYEAR            = 0x04B6);
PROVIDE(RTCYEAR_L          = 0x04B6);
PROVIDE(RTCYEAR_H          = 0x04B7);
PROVIDE(RTCAMINHR          = 0x04B8);
PROVIDE(RTCAMINHR_L        = 0x04B8);
PROVIDE(RTCAMINHR_H        = 0x04B9);
PROVIDE(RTCADOWDAY         = 0x04BA);
PROVIDE(RTCADOWDAY_L       = 0x04BA);
PROVIDE(RTCADOWDAY_H       = 0x04BB);
/************************************************************
* SFR - Special Function Register Module
************************************************************/
PROVIDE(SFRIE1             = 0x0100);
PROVIDE(SFRIE1_L           = 0x0100);
PROVIDE(SFRIE1_H           = 0x0101);
PROVIDE(SFRIFG1            = 0x0102);
PROVIDE(SFRIFG1_L          = 0x0102);
PROVIDE(SFRIFG1_H          = 0x0103);
PROVIDE(SFRRPCR            = 0x0104);
PROVIDE(SFRRPCR_L          = 0x0104);
PROVIDE(SFRRPCR_H          = 0x0105);
/************************************************************
* SYS - System Module
************************************************************/
PROVIDE(SYSCTL             = 0x0180);
PROVIDE(SYSCTL_L           = 0x0180);
PROVIDE(SYSCTL_H           = 0x0181);
PROVIDE(SYSBSLC            = 0x0182);
PROVIDE(SYSBSLC_L          = 0x0182);
PROVIDE(SYSBSLC_H          = 0x0183);
PROVIDE(SYSJMBC            = 0x0186);
PROVIDE(SYSJMBC_L          = 0x0186);
PROVIDE(SYSJMBC_H          = 0x0187);
PROVIDE(SYSJMBI0           = 0x0188);
PROVIDE(SYSJMBI0_L         = 0x0188);
PROVIDE(SYSJMBI0_H         = 0x0189);
PROVIDE(SYSJMBI1           = 0x018A);
PROVIDE(SYSJMBI1_L         = 0x018A);
PROVIDE(SYSJMBI1_H         = 0x018B);
PROVIDE(SYSJMBO0           = 0x018C);
PROVIDE(SYSJMBO0_L         = 0x018C);
PROVIDE(SYSJMBO0_H         = 0x018D);
PROVIDE(SYSJMBO1           = 0x018E);
PROVIDE(SYSJMBO1_L         = 0x018E);
PROVIDE(SYSJMBO1_H         = 0x018F);
PROVIDE(SYSBERRIV          = 0x0198);
PROVIDE(SYSBERRIV_L        = 0x0198);
PROVIDE(SYSBERRIV_H        = 0x0199);
PROVIDE(SYSUNIV            = 0x019A);
PROVIDE(SYSUNIV_L          = 0x019A);
PROVIDE(SYSUNIV_H          = 0x019B);
PROVIDE(SYSSNIV            = 0x019C);
PROVIDE(SYSSNIV_L          = 0x019C);
PROVIDE(SYSSNIV_H          = 0x019D);
PROVIDE(SYSRSTIV           = 0x019E);
PROVIDE(SYSRSTIV_L         = 0x019E);
PROVIDE(SYSRSTIV_H         = 0x019F);
/************************************************************
* Timer0_A5
************************************************************/
PROVIDE(TA0CTL             = 0x0340);
PROVIDE(TA0CCTL0           = 0x0342);
PROVIDE(TA0CCTL1           = 0x0344);
PROVIDE(TA0CCTL2           = 0x0346);
PROVIDE(TA0CCTL3           = 0x0348);
PROVIDE(TA0CCTL4           = 0x034A);
PROVIDE(TA0R               = 0x0350);
PROVIDE(TA0CCR0            = 0x0352);
PROVIDE(TA0CCR1            = 0x0354);
PROVIDE(TA0CCR2            = 0x0356);
PROVIDE(TA0CCR3            = 0x0358);
PROVIDE(TA0CCR4            = 0x035A);
PROVIDE(TA0IV              = 0x036E);
PROVIDE(TA0EX0             = 0x0360);
/************************************************************
* Timer1_A3
************************************************************/
PROVIDE(TA1CTL             = 0x0380);
PROVIDE(TA1CCTL0           = 0x0382);
PROVIDE(TA1CCTL1           = 0x0384);
PROVIDE(TA1CCTL2           = 0x0386);
PROVIDE(TA1R               = 0x0390);
PROVIDE(TA1CCR0            = 0x0392);
PROVIDE(TA1CCR1            = 0x0394);
PROVIDE(TA1CCR2            = 0x0396);
PROVIDE(TA1IV              = 0x03AE);
PROVIDE(TA1EX0             = 0x03A0);
/************************************************************
* Timer2_A3
************************************************************/
PROVIDE(TA2CTL             = 0x0400);
PROVIDE(TA2CCTL0           = 0x0402);
PROVIDE(TA2CCTL1           = 0x0404);
PROVIDE(TA2CCTL2           = 0x0406);
PROVIDE(TA2R               = 0x0410);
PROVIDE(TA2CCR0            = 0x0412);
PROVIDE(TA2CCR1            = 0x0414);
PROVIDE(TA2CCR2            = 0x0416);
PROVIDE(TA2IV              = 0x042E);
PROVIDE(TA2EX0             = 0x0420);
/************************************************************
* Timer0_B7
************************************************************/
PROVIDE(TB0CTL             = 0x03C0);
PROVIDE(TB0CCTL0           = 0x03C2);
PROVIDE(TB0CCTL1           = 0x03C4);
PROVIDE(TB0CCTL2           = 0x03C6);
PROVIDE(TB0CCTL3           = 0x03C8);
PROVIDE(TB0CCTL4           = 0x03CA);
PROVIDE(TB0CCTL5           = 0x03CC);
PROVIDE(TB0CCTL6           = 0x03CE);
PROVIDE(TB0R               = 0x03D0);
PROVIDE(TB0CCR0            = 0x03D2);
PROVIDE(TB0CCR1            = 0x03D4);
PROVIDE(TB0CCR2            = 0x03D6);
PROVIDE(TB0CCR3            = 0x03D8);
PROVIDE(TB0CCR4            = 0x03DA);
PROVIDE(TB0CCR5            = 0x03DC);
PROVIDE(TB0CCR6            = 0x03DE);
PROVIDE(TB0EX0             = 0x03E0);
PROVIDE(TB0IV              = 0x03EE);
/************************************************************
* USB
************************************************************/
PROVIDE(USBKEYID           = 0x0900);
PROVIDE(USBKEYID_L         = 0x0900);
PROVIDE(USBKEYID_H         = 0x0901);
PROVIDE(USBCNF             = 0x0902);
PROVIDE(USBCNF_L           = 0x0902);
PROVIDE(USBCNF_H           = 0x0903);
PROVIDE(USBPHYCTL          = 0x0904);
PROVIDE(USBPHYCTL_L        = 0x0904);
PROVIDE(USBPHYCTL_H        = 0x0905);
PROVIDE(USBPWRCTL          = 0x0908);
PROVIDE(USBPWRCTL_L        = 0x0908);
PROVIDE(USBPWRCTL_H        = 0x0909);
PROVIDE(USBPLLCTL          = 0x0910);
PROVIDE(USBPLLCTL_L        = 0x0910);
PROVIDE(USBPLLCTL_H        = 0x0911);
PROVIDE(USBPLLDIVB         = 0x0912);
PROVIDE(USBPLLDIVB_L       = 0x0912);
PROVIDE(USBPLLDIVB_H       = 0x0913);
PROVIDE(USBPLLIR           = 0x0914);
PROVIDE(USBPLLIR_L         = 0x0914);
PROVIDE(USBPLLIR_H         = 0x0915);
PROVIDE(USBIEPCNF_0        = 0x0920);
PROVIDE(USBIEPCNT_0        = 0x0921);
PROVIDE(USBOEPCNF_0        = 0x0922);
PROVIDE(USBOEPCNT_0        = 0x0923);
PROVIDE(USBIEPIE           = 0x092E);
PROVIDE(USBOEPIE           = 0x092F);
PROVIDE(USBIEPIFG          = 0x0930);
PROVIDE(USBOEPIFG          = 0x0931);
PROVIDE(USBVECINT          = 0x0932);
PROVIDE(USBVECINT_L        = 0x0932);
PROVIDE(USBVECINT_H        = 0x0933);
PROVIDE(USBMAINT           = 0x0936);
PROVIDE(USBMAINT_L         = 0x0936);
PROVIDE(USBMAINT_H         = 0x0937);
PROVIDE(USBTSREG           = 0x0938);
PROVIDE(USBTSREG_L         = 0x0938);
PROVIDE(USBTSREG_H         = 0x0939);
PROVIDE(USBFN              = 0x093A);
PROVIDE(USBFN_L            = 0x093A);
PROVIDE(USBFN_H            = 0x093B);
PROVIDE(USBCTL             = 0x093C);
PROVIDE(USBIE              = 0x093D);
PROVIDE(USBIFG             = 0x093E);
PROVIDE(USBFUNADR          = 0x093F);
PROVIDE(USBIEPSIZXY_7      = 0x23FF);
PROVIDE(USBIEPBCTY_7       = 0x23FE);
PROVIDE(USBIEPBBAY_7       = 0x23FD);
PROVIDE(USBIEPBCTX_7       = 0x23FA);
PROVIDE(USBIEPBBAX_7       = 0x23F9);
PROVIDE(USBIEPCNF_7        = 0x23F8);
PROVIDE(USBIEPSIZXY_6      = 0x23F7);
PROVIDE(USBIEPBCTY_6       = 0x23F6);
PROVIDE(USBIEPBBAY_6       = 0x23F5);
PROVIDE(USBIEPBCTX_6       = 0x23F2);
PROVIDE(USBIEPBBAX_6       = 0x23F1);
PROVIDE(USBIEPCNF_6        = 0x23F0);
PROVIDE(USBIEPSIZXY_5      = 0x23EF);
PROVIDE(USBIEPBCTY_5       = 0x23EE);
PROVIDE(USBIEPBBAY_5       = 0x23ED);
PROVIDE(USBIEPBCTX_5       = 0x23EA);
PROVIDE(USBIEPBBAX_5       = 0x23E9);
PROVIDE(USBIEPCNF_5        = 0x23E8);
PROVIDE(USBIEPSIZXY_4      = 0x23E7);
PROVIDE(USBIEPBCTY_4       = 0x23E6);
PROVIDE(USBIEPBBAY_4       = 0x23E5);
PROVIDE(USBIEPBCTX_4       = 0x23E2);
PROVIDE(USBIEPBBAX_4       = 0x23E1);
PROVIDE(USBIEPCNF_4        = 0x23E0);
PROVIDE(USBIEPSIZXY_3      = 0x23DF);
PROVIDE(USBIEPBCTY_3       = 0x23DE);
PROVIDE(USBIEPBBAY_3       = 0x23DD);
PROVIDE(USBIEPBCTX_3       = 0x23DA);
PROVIDE(USBIEPBBAX_3       = 0x23D9);
PROVIDE(USBIEPCNF_3        = 0x23D8);
PROVIDE(USBIEPSIZXY_2      = 0x23D7);
PROVIDE(USBIEPBCTY_2       = 0x23D6);
PROVIDE(USBIEPBBAY_2       = 0x23D5);
PROVIDE(USBIEPBCTX_2       = 0x23D2);
PROVIDE(USBIEPBBAX_2       = 0x23D1);
PROVIDE(USBIEPCNF_2        = 0x23D0);
PROVIDE(USBIEPSIZXY_1      = 0x23CF);
PROVIDE(USBIEPBCTY_1       = 0x23CE);
PROVIDE(USBIEPBBAY_1       = 0x23CD);
PROVIDE(USBIEPBCTX_1       = 0x23CA);
PROVIDE(USBIEPBBAX_1       = 0x23C9);
PROVIDE(USBIEPCNF_1        = 0x23C8);
PROVIDE(USBOEPSIZXY_7      = 0x23BF);
PROVIDE(USBOEPBCTY_7       = 0x23BE);
PROVIDE(USBOEPBBAY_7       = 0x23BD);
PROVIDE(USBOEPBCTX_7       = 0x23BA);
PROVIDE(USBOEPBBAX_7       = 0x23B9);
PROVIDE(USBOEPCNF_7        = 0x23B8);
PROVIDE(USBOEPSIZXY_6      = 0x23B7);
PROVIDE(USBOEPBCTY_6       = 0x23B6);
PROVIDE(USBOEPBBAY_6       = 0x23B5);
PROVIDE(USBOEPBCTX_6       = 0x23B2);
PROVIDE(USBOEPBBAX_6       = 0x23B1);
PROVIDE(USBOEPCNF_6        = 0x23B0);
PROVIDE(USBOEPSIZXY_5      = 0x23AF);
PROVIDE(USBOEPBCTY_5       = 0x23AE);
PROVIDE(USBOEPBBAY_5       = 0x23AD);
PROVIDE(USBOEPBCTX_5       = 0x23AA);
PROVIDE(USBOEPBBAX_5       = 0x23A9);
PROVIDE(USBOEPCNF_5        = 0x23A8);
PROVIDE(USBOEPSIZXY_4      = 0x23A7);
PROVIDE(USBOEPBCTY_4       = 0x23A6);
PROVIDE(USBOEPBBAY_4       = 0x23A5);
PROVIDE(USBOEPBCTX_4       = 0x23A2);
PROVIDE(USBOEPBBAX_4       = 0x23A1);
PROVIDE(USBOEPCNF_4        = 0x23A0);
PROVIDE(USBOEPSIZXY_3      = 0x239F);
PROVIDE(USBOEPBCTY_3       = 0x239E);
PROVIDE(USBOEPBBAY_3       = 0x239D);
PROVIDE(USBOEPBCTX_3       = 0x239A);
PROVIDE(USBOEPBBAX_3       = 0x2399);
PROVIDE(USBOEPCNF_3        = 0x2398);
PROVIDE(USBOEPSIZXY_2      = 0x2397);
PROVIDE(USBOEPBCTY_2       = 0x2396);
PROVIDE(USBOEPBBAY_2       = 0x2395);
PROVIDE(USBOEPBCTX_2       = 0x2392);
PROVIDE(USBOEPBBAX_2       = 0x2391);
PROVIDE(USBOEPCNF_2        = 0x2390);
PROVIDE(USBOEPSIZXY_1      = 0x238F);
PROVIDE(USBOEPBCTY_1       = 0x238E);
PROVIDE(USBOEPBBAY_1       = 0x238D);
PROVIDE(USBOEPBCTX_1       = 0x238A);
PROVIDE(USBOEPBBAX_1       = 0x2389);
PROVIDE(USBOEPCNF_1        = 0x2388);
PROVIDE(USBSUBLK           = 0x2380);
PROVIDE(USBIEP0BUF         = 0x2378);
PROVIDE(USBOEP0BUF         = 0x2370);
PROVIDE(USBTOPBUFF         = 0x236F);
PROVIDE(USBSTABUFF         = 0x1C00);
/************************************************************
* UNIFIED CLOCK SYSTEM
************************************************************/
PROVIDE(UCSCTL0            = 0x0160);
PROVIDE(UCSCTL0_L          = 0x0160);
PROVIDE(UCSCTL0_H          = 0x0161);
PROVIDE(UCSCTL1            = 0x0162);
PROVIDE(UCSCTL1_L          = 0x0162);
PROVIDE(UCSCTL1_H          = 0x0163);
PROVIDE(UCSCTL2            = 0x0164);
PROVIDE(UCSCTL2_L          = 0x0164);
PROVIDE(UCSCTL2_H          = 0x0165);
PROVIDE(UCSCTL3            = 0x0166);
PROVIDE(UCSCTL3_L          = 0x0166);
PROVIDE(UCSCTL3_H          = 0x0167);
PROVIDE(UCSCTL4            = 0x0168);
PROVIDE(UCSCTL4_L          = 0x0168);
PROVIDE(UCSCTL4_H          = 0x0169);
PROVIDE(UCSCTL5            = 0x016A);
PROVIDE(UCSCTL5_L          = 0x016A);
PROVIDE(UCSCTL5_H          = 0x016B);
PROVIDE(UCSCTL6            = 0x016C);
PROVIDE(UCSCTL6_L          = 0x016C);
PROVIDE(UCSCTL6_H          = 0x016D);
PROVIDE(UCSCTL7            = 0x016E);
PROVIDE(UCSCTL7_L          = 0x016E);
PROVIDE(UCSCTL7_H          = 0x016F);
PROVIDE(UCSCTL8            = 0x0170);
PROVIDE(UCSCTL8_L          = 0x0170);
PROVIDE(UCSCTL8_H          = 0x0171);
/************************************************************
* USCI A0
************************************************************/
PROVIDE(UCA0CTLW0          = 0x05C0);
PROVIDE(UCA0CTLW0_L        = 0x05C0);
PROVIDE(UCA0CTLW0_H        = 0x05C1);
PROVIDE(UCA0BRW            = 0x05C6);
PROVIDE(UCA0BRW_L          = 0x05C6);
PROVIDE(UCA0BRW_H          = 0x05C7);
PROVIDE(UCA0MCTL           = 0x05C8);
PROVIDE(UCA0STAT           = 0x05CA);
PROVIDE(UCA0RXBUF          = 0x05CC);
PROVIDE(UCA0TXBUF          = 0x05CE);
PROVIDE(UCA0ABCTL          = 0x05D0);
PROVIDE(UCA0IRCTL          = 0x05D2);
PROVIDE(UCA0IRCTL_L        = 0x05D2);
PROVIDE(UCA0IRCTL_H        = 0x05D3);
PROVIDE(UCA0ICTL           = 0x05DC);
PROVIDE(UCA0ICTL_L         = 0x05DC);
PROVIDE(UCA0ICTL_H         = 0x05DD);
PROVIDE(UCA0IV             = 0x05DE);
/************************************************************
* USCI B0
************************************************************/
PROVIDE(UCB0CTLW0          = 0x05E0);
PROVIDE(UCB0CTLW0_L        = 0x05E0);
PROVIDE(UCB0CTLW0_H        = 0x05E1);
PROVIDE(UCB0BRW            = 0x05E6);
PROVIDE(UCB0BRW_L          = 0x05E6);
PROVIDE(UCB0BRW_H          = 0x05E7);
PROVIDE(UCB0STAT           = 0x05EA);
PROVIDE(UCB0RXBUF          = 0x05EC);
PROVIDE(UCB0TXBUF          = 0x05EE);
PROVIDE(UCB0I2COA          = 0x05F0);
PROVIDE(UCB0I2COA_L        = 0x05F0);
PROVIDE(UCB0I2COA_H        = 0x05F1);
PROVIDE(UCB0I2CSA          = 0x05F2);
PROVIDE(UCB0I2CSA_L        = 0x05F2);
PROVIDE(UCB0I2CSA_H        = 0x05F3);
PROVIDE(UCB0ICTL           = 0x05FC);
PROVIDE(UCB0ICTL_L         = 0x05FC);
PROVIDE(UCB0ICTL_H         = 0x05FD);
PROVIDE(UCB0IV             = 0x05FE);
/************************************************************
* USCI A1
************************************************************/
PROVIDE(UCA1CTLW0          = 0x0600);
PROVIDE(UCA1CTLW0_L        = 0x0600);
PROVIDE(UCA1CTLW0_H        = 0x0601);
PROVIDE(UCA1BRW            = 0x0606);
PROVIDE(UCA1BRW_L          = 0x0606);
PROVIDE(UCA1BRW_H          = 0x0607);
PROVIDE(UCA1MCTL           = 0x0608);
PROVIDE(UCA1STAT           = 0x060A);
PROVIDE(UCA1RXBUF          = 0x060C);
PROVIDE(UCA1TXBUF          = 0x060E);
PROVIDE(UCA1ABCTL          = 0x0610);
PROVIDE(UCA1IRCTL          = 0x0612);
PROVIDE(UCA1IRCTL_L        = 0x0612);
PROVIDE(UCA1IRCTL_H        = 0x0613);
PROVIDE(UCA1ICTL           = 0x061C);
PROVIDE(UCA1ICTL_L         = 0x061C);
PROVIDE(UCA1ICTL_H         = 0x061D);
PROVIDE(UCA1IV             = 0x061E);
/************************************************************
* USCI B1
************************************************************/
PROVIDE(UCB1CTLW0          = 0x0620);
PROVIDE(UCB1CTLW0_L        = 0x0620);
PROVIDE(UCB1CTLW0_H        = 0x0621);
PROVIDE(UCB1BRW            = 0x0626);
PROVIDE(UCB1BRW_L          = 0x0626);
PROVIDE(UCB1BRW_H          = 0x0627);
PROVIDE(UCB1STAT           = 0x062A);
PROVIDE(UCB1RXBUF          = 0x062C);
PROVIDE(UCB1TXBUF          = 0x062E);
PROVIDE(UCB1I2COA          = 0x0630);
PROVIDE(UCB1I2COA_L        = 0x0630);
PROVIDE(UCB1I2COA_H        = 0x0631);
PROVIDE(UCB1I2CSA          = 0x0632);
PROVIDE(UCB1I2CSA_L        = 0x0632);
PROVIDE(UCB1I2CSA_H        = 0x0633);
PROVIDE(UCB1ICTL           = 0x063C);
PROVIDE(UCB1ICTL_L         = 0x063C);
PROVIDE(UCB1ICTL_H         = 0x063D);
PROVIDE(UCB1IV             = 0x063E);
/************************************************************
* WATCHDOG TIMER A
************************************************************/
PROVIDE(WDTCTL             = 0x015C);
PROVIDE(WDTCTL_L           = 0x015C);
PROVIDE(WDTCTL_H           = 0x015D);
/************************************************************
* TLV Descriptors
************************************************************/
/************************************************************
* Interrupt Vectors (offset from 0xFF80)
************************************************************/
/************************************************************
* End of Modules
************************************************************/
