#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "yellow.h"
#include "green.h"
#include "red.h"
#include "floop.h"
#include "start.h"
#include "drum.h"
#include "timer.h"

#define BUF_THRESHOLD 96
#define AUDIO_BASE  0xFF203040

/*AUDIO VARIABLES*/
	
int floop_buffer[2152]= floop;
;
int floop_buf_size = 1076*2;

int drum_roll_buffer[41025] = drum;
;

int drum_roll_buf_size = 20513*2;

int timer_buffer[39492] = timer;
;
int timer_buf_size = 19764 * 2;

/*GLOBAL PIXEL BUFFER VARIABLE*/
volatile int pixel_buffer_start;

/*FUNCTION PROTOYPES*/

void clear_screen();

void plot_pixel(int x,int y, short int line_color);

void wait_for_vsync();

void draw_start();

void draw_answer(int n);

void draw_ball();

void sound(int*,int);

//step tells you what part of the game are you in

int step = -1;

/*

-1 = reset stage, if key 1 pressed after game has started   

0 = key 1 pressed, start game screen 

1 = key 0 is preseed, ball starts bouncing

2 = key 0 is pressed, ball stops bouncing and display answer 

*/


//Prototypes for interrupt code for keys

void delay();

void set_A9_IRQ_stack(void);

void config_GIC(void);

void config_KEYs(void);

void enable_A9_interrupts(void);

void pushbutton_ISR(void);

void config_interrupt(int, int);

void disable_A9_interrupts(void);


int main(void)

{

    disable_A9_interrupts(); // disable interrupts in the A9 processor

    set_A9_IRQ_stack(); // initialize the stack pointer for IRQ mode

    config_GIC(); // configure the general interrupt controller

    config_KEYs(); // configure pushbutton KEYs to generate interrupts

    enable_A9_interrupts(); // enable interrupts in the A9 processor

    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;




    *(pixel_ctrl_ptr + 1) = 0xC8000000;

	wait_for_vsync();

    pixel_buffer_start = *pixel_ctrl_ptr;

    clear_screen();

    *(pixel_ctrl_ptr + 1) = 0xC0000000;

    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
	
	int random_array[3] = {0,1,2};

	int n = random_array[rand()%3];

	//sound for start of game
	sound(floop_buffer,floop_buf_size);
	
    while (1)

    {

        /* Erase any boxes and lines that were drawn in the last iteration */

        clear_screen();

        
		//display start screen
        if (step<=0) {
			draw_start();
        }
		
		//boucing ball
        if (step==1) {
			draw_ball();
        }


		//display answer
        if (step == 2) {
			draw_answer(n);
        }


        wait_for_vsync();
		
		pixel_buffer_start = *(pixel_ctrl_ptr + 1); // new back buffer

        

    }



}


void wait_for_vsync(){

    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;

    volatile int * status =(int *)0xFF20302C;

    *pixel_ctrl_ptr = 1;

    while((*status & 0x01) != 0) status = status; //keep reading status

    

    

    return;

}

void clear_screen() {

    int x;

    int y;

    for (x = 0; x < 320; x++)

        for (y = 0; y < 240; y++)

            plot_pixel(x, y, 0x0000);

}



void plot_pixel(int x, int y, short int line_color){

    *(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;

}

/*AUDIO FUNCTION*/

void sound(int sound_buffer[], int BUF_SIZE)

{



	volatile int * audio_ptr = (int *) AUDIO_BASE;



	/* used for audio playback */

	int fifospace;

	int play = 1, buffer_index = 0;

	int left_buffer[BUF_SIZE];

	int right_buffer[BUF_SIZE];

	 	

	while (play)

		{

			fifospace = *(audio_ptr + 1);	 		// read the audio port fifospace register

			if ( (fifospace & 0x00FF0000) > BUF_THRESHOLD ) 	// check WSRC

			{

				// output data until the buffer is empty or the audio-out FIFO is full

				while ( (fifospace & 0x00FF0000) && (buffer_index < BUF_SIZE))

				{

					*(audio_ptr + 2) = sound_buffer[buffer_index];

					*(audio_ptr + 3) = sound_buffer[buffer_index];

					++buffer_index;



					if (buffer_index == BUF_SIZE)

					{

						play = 0;

					}

					fifospace = *(audio_ptr + 1);	// read the audio port fifospace register

				}

			}

		}

}


/* setup the KEY interrupts in the FPGA */

void config_KEYs() {

    volatile int * KEY_ptr = (int *) 0xFF200050; // pushbutton KEY base address

    *(KEY_ptr + 2) = 0xF; // enable interrupts for the two KEYs

}



// Define the IRQ exception handler

void __attribute__((interrupt)) __cs3_isr_irq(void) {

    // Read the ICCIAR from the CPU Interface in the GIC

    int interrupt_ID = *((int *)0xFFFEC10C);

    if (interrupt_ID == 73) // check if interrupt is from the KEYs

    pushbutton_ISR();

    else

    while (1); // if unexpected, then stay here

    // Write to the End of Interrupt Register (ICCEOIR)

    *((int *)0xFFFEC110) = interrupt_ID;

}

// Define the remaining exception handlers

void __attribute__((interrupt)) __cs3_reset(void) {

    while (1);

}

void __attribute__((interrupt)) __cs3_isr_undef(void) {

    while (1);

}

void __attribute__((interrupt)) __cs3_isr_swi(void) {

    while (1);

}

void __attribute__((interrupt)) __cs3_isr_pabort(void) {

    while (1);

}

void __attribute__((interrupt)) __cs3_isr_dabort(void) {

    while (1);

}

void __attribute__((interrupt)) __cs3_isr_fiq(void) {

    while (1);

}

//Turn off interrupts in the ARM processor

void disable_A9_interrupts(void) {

    int status = 0b11010011;

    asm("msr cpsr, %[ps]" : : [ps] "r"(status));

}

//Initialize the banked stack pointer register for IRQ mode

void set_A9_IRQ_stack(void) {

    int stack, mode;

    stack = 0xFFFFFFFF - 7; // top of A9 onchip memory, aligned to 8 bytes

    /* change processor to IRQ mode with interrupts disabled */

    mode = 0b11010010;

    asm("msr cpsr, %[ps]" : : [ps] "r"(mode));

    /* set banked stack pointer */

    asm("mov sp, %[ps]" : : [ps] "r"(stack));

    /* go back to SVC mode before executing subroutine return! */

    mode = 0b11010011;

    asm("msr cpsr, %[ps]" : : [ps] "r"(mode));

}

//Turn on interrupts in the ARM processor

void enable_A9_interrupts(void) {

    int status = 0b01010011;

    asm("msr cpsr, %[ps]" : : [ps] "r"(status));

}

/*

* Configure the Generic Interrupt Controller (GIC)

*/

void config_GIC(void) {

    config_interrupt (73, 1); // configure the FPGA KEYs interrupt (73)

    // Set Interrupt Priority Mask Register (ICCPMR). Enable interrupts of all

    // priorities

    *((int *) 0xFFFEC104) = 0xFFFF;

    // Set CPU Interface Control Register (ICCICR). Enable signaling of

    // interrupts

    *((int *) 0xFFFEC100) = 1;

    // Configure the Distributor Control Register (ICDDCR) to send pending

    // interrupts to CPUs

    *((int *) 0xFFFED000) = 1;

}

/*

* Configure Set Enable Registers (ICDISERn) and Interrupt Processor Target

* Registers (ICDIPTRn). The default (reset) values are used for other registers

* in the GIC.

*/

void config_interrupt(int N, int CPU_target) {

    int reg_offset, index, value, address;

    /* Configure the Interrupt Set-Enable Registers (ICDISERn).

    * reg_offset = (integer_div(N / 32) * 4

    * value = 1 << (N mod 32) */

    reg_offset = (N >> 3) & 0xFFFFFFFC;

    index = N & 0x1F;

    value = 0x1 << index;

    address = 0xFFFED100 + reg_offset;

    /* Now that we know the register address and value, set the appropriate bit */

    *(int *)address |= value;

    /* Configure the Interrupt Processor Targets Register (ICDIPTRn)

    * reg_offset = integer_div(N / 4) * 4

    * index = N mod 4 */

    reg_offset = (N & 0xFFFFFFFC);

    index = N & 0x3;

    address = 0xFFFED800 + reg_offset + index;

    /* Now that we know the register address and value, write to (only) the

    * appropriate byte */

    *(char *)address = (char)CPU_target;

}

void pushbutton_ISR(void) {

    /* KEY base address */

    volatile int * KEY_ptr = (int *) 0xFF200050;

    int press;

    press = *(KEY_ptr + 3); // read the pushbutton interrupt register

    *(KEY_ptr + 3) = press; // Clear the interrupt

    if (press & 0x1 && step <= 0) { //key0 and game has started

        step = 1; //ball bouncing
		sound(timer_buffer,timer_buf_size);

    }

    else if (press & 0x2) { //key1
	
		step = -1; //to reset
		sound(floop_buffer,floop_buf_size);
		


    }
 	else if (press & 0x1 && step == 1) { //key 0 and ball was bouncing
		step = 2; //stop ball bouncing, move onto displaying answer
		sound(drum_roll_buffer,drum_roll_buf_size);
	}
					
    return;

}

/*DRAW FUNCTIONS*/

void draw_start(){
	
	short int start_screen[] = start;
	int i = 0;
	int j = 0;
	for (int k = 0; k < 320 * 2 * 240 -1; k+=2){
		int red = ((start_screen[k+1]&0xF8)>>3) <<11;
		int green = (((start_screen[k] &0xE0) >> 5)) | ((start_screen[k+1] & 0x7) << 3);
		int blue = (start_screen[k] & 0x1f);
		
		short int colour = red|((green<<5)|blue);
		
		plot_pixel(i,j,colour);
		
		i += 1;
		if(i == 320){
			i = 0;
			j += 1;
		}
	}
}


void draw_ball(){

	int i = 0;
	int j = 0;
	for (int k = 0; k < 320 * 2 * 240 -1; k+=2){
		int red = ((ball[k+1]&0xF8)>>3) <<11;
		int green = (((ball[k] &0xE0) >> 5)) | ((ball[k+1] & 0x7) << 3);
		int blue = (ball[k] & 0x1f);
		
		short int colour = red|((green<<5)|blue);
		
		plot_pixel(i,j,colour);
		
		i += 1;
		if(i == 320){
			i = 0;
			j += 1;
		}
	}
}


void draw_answer(int n){
  
	if(n == 0){
		short int answer_screen[] = GREEN;
		
	int i = 0;
	int j = 0;
	
	for (int k = 0; k < 320 * 2 * 240 -1; k+=2){
		int red = ((answer_screen[k+1]&0xF8)>>3) <<11;
		int green = (((answer_screen[k] &0xE0) >> 5)) | ((answer_screen[k+1] & 0x7) << 3);
		int blue = (answer_screen[k] & 0x1f);
		
		short int colour = red|((green<<5)|blue);
		
		plot_pixel(i,j,colour);
		
		i += 1;
		if(i == 320){
			i = 0;
			j += 1;
		}
	}
	}
	if(n == 1){
		short int answer_screen[] = YELLOW;
		
	int i = 0;
	int j = 0;
	
	for (int k = 0; k < 320 * 2 * 240 -1; k+=2){
		int red = ((answer_screen[k+1]&0xF8)>>3) <<11;
		int green = (((answer_screen[k] &0xE0) >> 5)) | ((answer_screen[k+1] & 0x7) << 3);
		int blue = (answer_screen[k] & 0x1f);
		
		short int colour = red|((green<<5)|blue);
		
		plot_pixel(i,j,colour);
		
		i += 1;
		if(i == 320){
			i = 0;
			j += 1;
		}
	}
	}
	if(n == 2){
		short int answer_screen[] = RED;
		
	int i = 0;
	int j = 0;
	
	for (int k = 0; k < 320 * 2 * 240 -1; k+=2){
		int red = ((answer_screen[k+1]&0xF8)>>3) <<11;
		int green = (((answer_screen[k] &0xE0) >> 5)) | ((answer_screen[k+1] & 0x7) << 3);
		int blue = (answer_screen[k] & 0x1f);
		
		short int colour = red|((green<<5)|blue);
		
		plot_pixel(i,j,colour);
		
		i += 1;
		if(i == 320){
			i = 0;
			j += 1;
		}
	}
	}
	
}

	



