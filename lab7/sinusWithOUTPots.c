/*
 * sinusWithOUTPotsAndDeadBand.c
 *
 *  Created on: Oct 3, 2018
 *      Author: ROG
 */


#include "DSP2833x_Device.h"
#include <math.h>
# define PI 3.14159265358979323846  /* pi */
/////////////////////////////////////////////////////////////////////////////////////////////////
                                                                                               //
////////////////////////////////////////////////////////////////////////////////////////////   //
                                                                                          //   //
 long switchingFrequency=70000;// = 48000;      // switching frequency in Hz                       //   //
                                                                                          //   //
 float fundamentalSinusoidalFrequency = 50;  // sinusoidal output frequency in Hz        //   //
                                                                                          //   //
 double phaseAngle = 0;   // starting angle of sinus                                       //   //
                                                                                          //   //
 float fundamentalSinusoidalMagnitude = 3.3;//Peaktopeak Value of output sinus waveform     //   //

 long RisingEdgeDelay = 50;         // Rising Edge Delay = TTBCLK x DBRED(=RisingEdgeDelay)

 long FallingEdgeDelay = 50;        // Falling Edge Delay = TTBCLK x DBFED(=FallingEdgeDelay)
                                    // TTBCLK = 13.3333ns to give 666ns delay we can use that number

 ///////////////////////////////////////////////////////////////////////////////////////////   //
                                                                                               //
 ////////////////////////////////////////////////////////////////////////////////////////////////
 long deviceClockFrequency = 150000000; // f28335 clock frequency in hz

 float maximumDeviceVoltage = 3.3;  // maximum voltage provided by device pin which 3.3V

 double counter = 0;

 double frequencyModulationRatio =0;
 float magnitudeModulationRatio = 0;

 float sinus=0;
 float sinus2=0;
 float sinus3=0;
 int CLKDIV = 1;
 int HSPCLKDIV = 1;

 float voltage_vr1;
 float voltage_vr2;
 float voltage_vr3;
 float voltage_vr4;

// Prototype statements for functions found within this file.
void Gpio_select(void);
extern void InitSysCtrl(void);
extern void InitPieCtrl(void);
extern void InitPieVectTable(void);
interrupt void ePWMA_compare_isr(void);
void Setup_ePWM(void);
extern void display_ADC(unsigned int);
extern void InitAdc(void);
interrupt void adc_isr(void);

//###########################################################################
//                      main code
//###########################################################################

void main(void)
{


        InitSysCtrl();      // Basic Core Initialization
        EALLOW;
        SysCtrlRegs.WDCR = 0x00AF;
        EDIS;

        DINT;               // Disable all interrupts

        Gpio_select();      // GPIO9,GPIO11,GPIO34 and GPIO49 as output (LEDs @ peripheral explorer)
        Setup_ePWM();
        InitPieCtrl();

        InitPieVectTable();
//////////////////////////////////////////////////////////////////////////////////////////
        InitAdc();

        AdcRegs.ADCTRL1.all = 0;
        AdcRegs.ADCTRL1.bit.SEQ_CASC = 1;   // cascaded Sequencer Mode
        AdcRegs.ADCTRL1.bit.CONT_RUN = 0;   // Single Run Mode
        AdcRegs.ADCTRL1.bit.ACQ_PS = 7;     // 8 x ADC-Clock
        AdcRegs.ADCTRL1.bit.CPS = 0;        // divide by 1

        AdcRegs.ADCTRL2.all = 0;
        AdcRegs.ADCTRL2.bit.EPWM_SOCA_SEQ1 = 1;     // ePWM_SOCA trigger
        AdcRegs.ADCTRL2.bit.INT_ENA_SEQ1 = 1; // enable ADC int for seq1
        AdcRegs.ADCTRL2.bit.INT_MOD_SEQ1 = 0; // interrupt after every EOS

        AdcRegs.ADCTRL3.bit.ADCCLKPS = 3;       // set FCLK to 12.5 MHz
        AdcRegs.ADCMAXCONV.all = 0x0003; // 4 conversions

        AdcRegs.ADCCHSELSEQ1.bit.CONV00 = 0; // 1st channel ADCINA0 //current A phase measurement
        AdcRegs.ADCCHSELSEQ1.bit.CONV01 = 1; // 2nd channel ADCINA1   //current B phase measurement
        AdcRegs.ADCCHSELSEQ1.bit.CONV02 = 2;          //current C phase measurement

        EPwm5Regs.TBCTL.all = 0xC032;   // Configure timer control register
        /*
         bit 15-14     11:     FREE/SOFT, 11 = ignore emulation suspend
         bit 13        0:      PHSDIR, 0 = count down after sync event
         bit 12-10     000:    CLKDIV, 000 => TBCLK = HSPCLK/1
         bit 9-7       000:    HSPCLKDIV, 000 => HSPCLK = SYSCLKOUT/1
         bit 6         0:      SWFSYNC, 0 = no software sync produced
         bit 5-4       11:     SYNCOSEL, 11 = sync-out disabled
         bit 3         0:      PRDLD, 0 = reload PRD on counter=0
         bit 2         0:      PHSEN, 0 = phase control disabled
         bit 1-0       00:     CTRMODE, 00 = count up mode
        */

        EPwm5Regs.TBPRD = 2499;//(0.5 * deviceClockFrequency) / (switchingFrequency * CLKDIV * HSPCLKDIV)-1; // TPPRD +1  =  TPWM / (HSPCLKDIV * CLKDIV * TSYSCLK)
                                //           =  20 �s / 6.667 ns

        EPwm5Regs.ETPS.all = 0x0100;            // Configure ADC start by ePWM2
        /*
         bit 15-14     00:     EPWMxSOCB, read-only
         bit 13-12     00:     SOCBPRD, don't care
         bit 11-10     00:     EPWMxSOCA, read-only
         bit 9-8       01:     SOCAPRD, 01 = generate SOCA on first event
         bit 7-4       0000:   reserved
         bit 3-2       00:     INTCNT, don't care
         bit 1-0       00:     INTPRD, don't care
        */

        EPwm5Regs.ETSEL.all = 0x0A00;           // Enable SOCA to ADC
        /*
         bit 15        0:      SOCBEN, 0 = disable SOCB
         bit 14-12     000:    SOCBSEL, don't care
         bit 11        1:      SOCAEN, 1 = enable SOCA
         bit 10-8      010:    SOCASEL, 010 = SOCA on PRD event
         bit 7-4       0000:   reserved
         bit 3         0:      INTEN, 0 = disable interrupt
         bit 2-0       000:    INTSEL, don't care
        */

 /////////////////////////////////////////////////////////////////////////////////////////
        EALLOW;
        PieVectTable.EPWM1_INT = &ePWMA_compare_isr;
        PieVectTable.ADCINT = &adc_isr;
        EDIS;
        PieCtrlRegs.PIEIER1.bit.INTx6 = 1;
        PieCtrlRegs.PIEIER3.bit.INTx1 = 1;

        IER |= 5;
        EINT;
        ERTM;

        while(1)
        {
                EALLOW;
                SysCtrlRegs.WDKEY = 0x55;   // service WD #1
                EDIS;
        }

}


void Gpio_select(void)
{
    EALLOW;
    GpioCtrlRegs.GPAMUX1.all = 0;       // GPIO15 ... GPIO0 = General Puropse I/O
    GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1; // ePWM1A active
    GpioCtrlRegs.GPAMUX1.bit.GPIO1 = 1; // ePWM1B active
    GpioCtrlRegs.GPAMUX1.bit.GPIO2 = 1; // ePWM2A active
    GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 1; // ePWM2B active
    GpioCtrlRegs.GPAMUX1.bit.GPIO4 = 1; // ePWM3A active
    GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 1; // ePWM3B active
    GpioCtrlRegs.GPAMUX2.all = 0;       // GPIO31 ... GPIO16 = General Purpose I/O
    GpioCtrlRegs.GPBMUX1.all = 0;       // GPIO47 ... GPIO32 = General Purpose I/O
    GpioCtrlRegs.GPBMUX2.all = 0;       // GPIO63 ... GPIO48 = General Purpose I/O
    GpioCtrlRegs.GPCMUX1.all = 0;       // GPIO79 ... GPIO64 = General Purpose I/O
    GpioCtrlRegs.GPCMUX2.all = 0;       // GPIO87 ... GPIO80 = General Purpose I/O

    GpioCtrlRegs.GPADIR.all = 0;
    GpioCtrlRegs.GPBDIR.all = 0;        // GPIO63-32 as inputs
    GpioCtrlRegs.GPCDIR.all = 0;        // GPIO87-64 as inputs
    EDIS;
}


void Setup_ePWM(void){
    EALLOW;
    SysCtrlRegs.WDKEY = 0xAA;   // service WD #2
    EDIS;

    EPwm1Regs.TBCTL.all = 0;
    EPwm1Regs.TBCTL.bit.CTRMODE = 2;         // Count up and down operation (10) = 2
    EPwm1Regs.AQCTLA.all = 0x0060;          //set ePWM1A to 1 on �CMPA - up match�
                                           //clear ePWM1A on event �CMPA - down match�
    EPwm1Regs.AQCTLB.all = 0x0090;         //clear ePWM1B on �CMPA - up match�
                                            //set ePWM1B to 1 on event �CMPA - down match�
                                            // we made reverse action to obtain complementary wave
    EPwm1Regs.DBRED = RisingEdgeDelay;                  // Rising Edge Delay = TTBCLK x DBRED
    EPwm1Regs.DBFED = FallingEdgeDelay;                  // Falling Edge Delay = TTBCLK x DBFED
                                           // TTBCLK = 13.3333ns to give 666ns delay we can use that number
    EPwm1Regs.DBCTL.all = 0x000B;          // S5 = S4 = S2 = 0    S0 = S1 = S3 = 1  RED & FED active also Active high complementary mode also PWMxA is source for RED and FED

    EPwm2Regs.TBCTL.all = 0;
    EPwm2Regs.TBCTL.bit.CTRMODE = 2;         // Count up and down operation (10) = 2
    EPwm2Regs.AQCTLA.all = 0x0060;         //set ePWM1A to 1 on �CMPA - up match�
                                           //clear ePWM1A on event �CMPA - down match�
    EPwm2Regs.AQCTLB.all = 0x0090;         //clear ePWM1B on �CMPA - up match�
                                            //set ePWM1B to 1 on event �CMPA - down match�
                                            // we made reverse action to obtain complementary wave
    EPwm2Regs.DBRED = RisingEdgeDelay;                  // Rising Edge Delay = TTBCLK x DBRED
    EPwm2Regs.DBFED = FallingEdgeDelay;                  // Falling Edge Delay = TTBCLK x DBFED
                                           // TTBCLK = 13.3333ns to give 666ns delay we can use that number

    EPwm3Regs.TBCTL.all = 0;
    EPwm3Regs.TBCTL.bit.CTRMODE = 2;         // Count up and down operation (10) = 2
    EPwm3Regs.AQCTLA.all = 0x0060;          //set ePWM1A to 1 on �CMPA - up match�
                                           //clear ePWM1A on event �CMPA - down match�
    EPwm3Regs.AQCTLB.all = 0x0090;         //clear ePWM1B on �CMPA - up match�
                                            //set ePWM1B to 1 on event �CMPA - down match�
                                            // we made reverse action to obtain complementary wave
    EPwm3Regs.DBRED = RisingEdgeDelay;                  // Rising Edge Delay = TTBCLK x DBRED
    EPwm3Regs.DBFED = FallingEdgeDelay;                  // Falling Edge Delay = TTBCLK x DBFED
                                           // TTBCLK = 13.3333ns to give 666ns delay we can use that number

    EPwm5Regs.TBCTL.all = 0;
////    EPwm5Regs.TBCTL.bit.CTRMODE = 2;         // Count up and down operation (10) = 2
    EPwm5Regs.TBCTL.all = 0xC032;


    EPwm1Regs.TBCTL.bit.CLKDIV = 0;
    EPwm2Regs.TBCTL.bit.CLKDIV = 0;
    EPwm3Regs.TBCTL.bit.CLKDIV = 0;
    CLKDIV = 1;
    EPwm1Regs.TBCTL.bit.HSPCLKDIV = 0;
    EPwm2Regs.TBCTL.bit.HSPCLKDIV = 0;
    EPwm3Regs.TBCTL.bit.HSPCLKDIV = 0;
    HSPCLKDIV = 1;


    EPwm1Regs.TBPRD = (0.5 * deviceClockFrequency) / (switchingFrequency * CLKDIV * HSPCLKDIV);        //the maximum number for TBPRD is (216 -1) or 65535
    EPwm2Regs.TBPRD = (0.5 * deviceClockFrequency) / (switchingFrequency * CLKDIV * HSPCLKDIV);
    EPwm3Regs.TBPRD = (0.5 * deviceClockFrequency) / (switchingFrequency * CLKDIV * HSPCLKDIV);
    EPwm5Regs.TBPRD = (0.5 * deviceClockFrequency) / (switchingFrequency * CLKDIV * HSPCLKDIV);

    EPwm1Regs.CMPA.half.CMPA = EPwm1Regs.TBPRD / 2; // 50% duty cycle first
    EPwm2Regs.CMPA.half.CMPA = EPwm2Regs.TBPRD / 2; // 50% duty cycle first
    EPwm3Regs.CMPA.half.CMPA = EPwm3Regs.TBPRD / 2; // 50% duty cycle first

    EPwm1Regs.ETSEL.all = 0;
    EPwm1Regs.ETSEL.bit.INTEN = 1;      // interrupt enable for ePWM1
    EPwm1Regs.ETSEL.bit.INTSEL = 1;     // interrupt on CMPA up match
    EPwm1Regs.ETPS.bit.INTPRD = 1;      // interrupt on first event

//
    EPwm5Regs.ETSEL.all = 0;
////    EPwm5Regs.ETSEL.bit.INTEN = 1;      // interrupt enable for ePWM1
////    EPwm5Regs.ETSEL.bit.INTSEL = 1;     // interrupt on CMPA up match
////    EPwm5Regs.ETPS.bit.INTPRD = 1;      // interrupt on first event
//
    EPwm5Regs.ETPS.all = 0x0100;
    EPwm5Regs.ETSEL.all = 0x0A00;


}

interrupt void ePWMA_compare_isr(void) {
    EALLOW;
    SysCtrlRegs.WDKEY = 0xAA;   // service WD #2
    EDIS;
    /////////////////////////



    EPwm1Regs.TBCTL.all = 0;
    EPwm1Regs.TBCTL.bit.CTRMODE = 2;         // Count up and down operation (10) = 2
    EPwm1Regs.AQCTLA.all = 0x0060;          //set ePWM1A to 1 on �CMPA - up match�
                                           //clear ePWM1A on event �CMPA - down match�

    EPwm2Regs.TBCTL.all = 0;
    EPwm2Regs.TBCTL.bit.CTRMODE = 2;         // Count up and down operation (10) = 2
    EPwm2Regs.AQCTLA.all = 0x0060;          //set ePWM1A to 1 on �CMPA - up match�
                                           //clear ePWM1A on event �CMPA - down match�
    EPwm3Regs.TBCTL.all = 0;
    EPwm3Regs.TBCTL.bit.CTRMODE = 2;         // Count up and down operation (10) = 2
    EPwm3Regs.AQCTLA.all = 0x0060;          //set ePWM1A to 1 on �CMPA - up match�
                                           //clear ePWM1A on event �CMPA - down match�
//
//
//    EPwm5Regs.TBCTL.all = 0;
//    EPwm5Regs.TBCTL.bit.CTRMODE = 2;         // Count up and down operation (10) = 2
//    EPwm5Regs.AQCTLA.all = 0x0090;          //set ePWM1A to 1 on �CMPA - up match�
//                                           //clear ePWM1A on event �CMPA - down match�
//

    EPwm1Regs.TBCTL.bit.CLKDIV = 0;
    EPwm2Regs.TBCTL.bit.CLKDIV = 0;
    EPwm3Regs.TBCTL.bit.CLKDIV = 0;
    CLKDIV = 1;
    EPwm1Regs.TBCTL.bit.HSPCLKDIV = 0;
    EPwm2Regs.TBCTL.bit.HSPCLKDIV = 0;
    EPwm3Regs.TBCTL.bit.HSPCLKDIV = 0;
    HSPCLKDIV = 1;

    EPwm1Regs.TBPRD = (0.5 * deviceClockFrequency) / (switchingFrequency * CLKDIV * HSPCLKDIV) ;        //the maximum number for TBPRD is (216 -1) or 65535
    EPwm2Regs.TBPRD = (0.5 * deviceClockFrequency) / (switchingFrequency * CLKDIV * HSPCLKDIV) ;
    EPwm3Regs.TBPRD = (0.5 * deviceClockFrequency) / (switchingFrequency * CLKDIV * HSPCLKDIV) ;


    frequencyModulationRatio = fundamentalSinusoidalFrequency / switchingFrequency;

    magnitudeModulationRatio = fundamentalSinusoidalMagnitude / maximumDeviceVoltage;
    //////////////////////////////////////////////////

    sinus = (sin(2 * PI * (frequencyModulationRatio) * counter + phaseAngle) + 1) / 2;

    EPwm1Regs.CMPA.half.CMPA = EPwm1Regs.TBPRD - (magnitudeModulationRatio) * EPwm1Regs.TBPRD * sinus - 1;//

    sinus2 = (sin(2 * PI * (frequencyModulationRatio) * counter + 2 * PI / 3 + phaseAngle) + 1) / 2;

    EPwm2Regs.CMPA.half.CMPA = EPwm2Regs.TBPRD - (magnitudeModulationRatio) * EPwm2Regs.TBPRD * sinus2 - 1; //

    sinus3 = (sin(2 * PI * (frequencyModulationRatio) * counter + 4 * PI / 3 + phaseAngle) + 1) / 2;

    EPwm3Regs.CMPA.half.CMPA = EPwm3Regs.TBPRD - (magnitudeModulationRatio) * EPwm3Regs.TBPRD * sinus3 - 1;

    counter +=1;
    if( counter > ((1 / frequencyModulationRatio)-1)) counter = 0;

    EPwm1Regs.ETCLR.bit.INT = 1;
    PieCtrlRegs.PIEACK.all = 4;

}
interrupt void adc_isr(void){
    EALLOW;
    SysCtrlRegs.WDKEY = 0xAA;   // service WD #2
    EDIS;

    int AphaseVoltage = (AdcMirror.ADCRESULT0) * 3.3 / 4095;

    int BphaseVoltage = (AdcMirror.ADCRESULT1) * 3.3 / 4095;

    int CphaseVoltage = (AdcMirror.ADCRESULT2) * 3.3 / 4095;

    int AphaseCurrent = (AphaseVoltage - 1.5) / 8.2 / 2 / (4 * 10^6);

    int BphaseCurrent = (BphaseVoltage - 1.5) / 8.2 / 2 / (4 * 10^6);

    int CphaseCurrent = (CphaseVoltage - 1.5) / 8.2 / 2 / (4 * 10^6);

    AdcRegs.ADCTRL2.bit.RST_SEQ1 = 1;
    AdcRegs.ADCST.bit.INT_SEQ1_CLR = 1;
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;

}
//===========================================================================
// End of SourceCode.
//===========================================================================






