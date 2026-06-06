#ifndef _LEDS_H_
#define _LEDS_H_

#define led_on()		GPIOB->BSRR=GPIO_BSRR_BS_3
#define led_off()		GPIOB->BSRR=GPIO_BSRR_BR_3


	//GPIOA
	#define seg_T		0x100
	#define seg_M		0x800

	//GPIOB
	#define seg_B		0x01
	#define seg_BL	0x80
	#define seg_BR	0x40
	#define seg_TR	0x02
	#define seg_TL	0x20
	
	
void init_led(void);
void init_led_display(void);
void output_led_display(int d);

#endif
