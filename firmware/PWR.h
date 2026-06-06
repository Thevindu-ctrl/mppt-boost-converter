#ifndef __STM_PWR_H__
#define __STM_PWR_H__

#define Set_LPMode_Sleep() SCB->SCR&=~(SCB_SCR_SLEEPONEXIT_Msk|SCB_SCR_SLEEPDEEP_Msk)
#define Enter_LPMode __WFI()



#define Sleep() {Set_LPMode_Sleep(); Enter_LPMode;}

#endif
