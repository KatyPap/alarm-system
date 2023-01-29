#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define ped 10
#define threashold 20

void activation();		// Συνάρτηση ενεργοποίησης του συναγερμού
void deactivation();	// Συνάρτηση απενεργοποίησης του συναγερμού

int alarm = 0;			
int interr = 0;			
int tries = 0;			
int password = 0;		
int password_digit = 0;			

int main(void)
{
	//1. Έξοδοι
	PORTD.DIR |= PIN0_bm; // Έξοδος για τον PWD
		
	//2. Switches για την εισαγωγή του κωδικού απενεργοποίησης
	PORTF.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc; // switch για το 2ο, 3ο ψηφίο του συναγερμού
	PORTF.PIN6CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc; // switch για το 1ο, 4ο ψηφίο του συναγερμού
	
	//3. Enable interrupts
	sei();
		
	//4. Αρχική κατάσταση
	while(password < 4)
	{
		_delay_ms(ped);
	}
	
    while (1) 
    {
		password = 0;
		password_digit = 0;
		// Ο κωδικός δώθηκε για ενεργοποίηση
		activation();
					
		// Αρχικοποίηση του ADC για να λειτουργήσει σε Free-Running mode
		ADC0.CTRLA |= ADC_RESSEL_10BIT_gc; //10-bit resolution
		ADC0.CTRLA |= ADC_FREERUN_bm; //Free-Running mode enabled
		ADC0.CTRLA |= ADC_ENABLE_bm; //Enable ADC
		ADC0.MUXPOS |= ADC_MUXPOS_AIN7_gc;
		//Enable Debug Mode
		ADC0.DBGCTRL |= ADC_DBGRUN_bm;
		//Window Comparator Mode
		ADC0.WINLT |= threashold; // Threshold για τον εντοπισμό εισβολέα
		ADC0.INTCTRL |= ADC_WCMP_bm; //Enable Interrupts for WCM
		ADC0.CTRLE |= ADC_WINCM0_bm; //Interrupt when RESULT < WINLT
		ADC0.COMMAND |= ADC_STCONV_bm; //Start Conversion
		
		if (ADC0.RES < ADC0.WINLT)
		{
			ADC0.CTRLA = 0;
			deactivation();
		}
	}
}


// Συναρτήσεις

void activation()
{
	// Ενεργοποίηση του TCA0 ως timmer
	TCA0.SINGLE.CNT = 0;
	TCA0.SINGLE.CTRLB = 0;
	TCA0.SINGLE.CMP0 = ped;
	TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc;
	TCA0.SINGLE.CTRLA |= 1;
	TCA0.SINGLE.INTCTRL = TCA_SINGLE_CMP0_bm;
}

void deactivation()
{
	TCA0.SINGLE.CNT = 0;
	TCA0.SINGLE.CTRLB = 0;
	TCA0.SINGLE.CMP0 = ped;
	TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc;
	TCA0.SINGLE.CTRLA |= 1;
	TCA0.SINGLE.INTCTRL = TCA_SINGLE_CMP0_bm;
	while(TCA0.SINGLE.CMP0 < ped && tries < 3)
	{
		if (password == 4)
		{
			TCA0.SINGLE.CNT = 0;
			TCA0.SINGLE.CTRLA = 0; // disable τον timmer
		}
		if else(password == 0)
		{tries++;}
	}
	if(TCA0.SINGLE.CMP0 == ped || tries == 3)
	{
		TCA0.SINGLE.CNT = 0;
		TCA0.SINGLE.CTRLA = 0; // disable τον timmer
		alarm = 1;
		while(password != 4)
		{
			// Ο TCA0 τώρα λειτουργεί ως PWM
			TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc;
			TCA0.SINGLE.PER = 254;
			TCA0.SINGLE.CMP0 = 127;
			TCA0.SINGLE.CTRLB |= TCA_SINGLE_WGMODE_SINGLESLOPE_gc;
			// enable interrupt Overflow
			TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
			// enable interrupt COMP0
			TCA0.SINGLE.INTCTRL |= TCA_SINGLE_CMP0_bm;
			TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm; //Enable
		}
		TCA0.SINGLE.CTRLA = 0; // disable τον PWM
		// Βγήκε από τη while
		// Άρα δόθηκε ο σωστός κωδικός για απενεργοποίηση
		password = 0;
		password_digit = 0;
		TCA0.SINGLE.CNT = 0;
		TCA0.SINGLE.CTRLA = 0; // disable τον timmer
		// Άρα πάμε πίσω στην αρχική κατάσταση αναμονής
		while(password < 4)
		{
			_delay_ms(ped);
		}
		alarm = 0;
	}
}

// ISRs

ISR(ADC0_WCOMP_vect){
	// interrupt για τον ADC
	int intflags = ADC0.INTFLAGS;
	ADC0.INTFLAGS = intflags;
	PORTD.OUT |= PIN0_bm;
}

ISR(PORTF_PORT_vect){				
	// Interrupt για την εισαγωγή του κωδικού
	int intflags = PORTF.INTFLAGS;
	PORTF.INTFLAGS=intflags;
	password_digit++;
	if(intflags & 0x20)
	{
		if(password_digit == 2 || password_digit == 3)
		{password++;}
		else
		{
			password = 0;
			password_digit = 0;
		}
	}
	if(intflags & 0x40)
	{
		if(password_digit == 1 || password_digit == 4)
		{password++;}
		else
		{
			password = 0;
			password_digit = 0;
		}
	}
}

ISR(TCA0_OVF_vect)
{
	int intflags = TCA0.SINGLE.INTFLAGS;
	TCA0.SINGLE.INTFLAGS = intflags;
	PORTD.OUT |= PIN0_bm; // ενεργοποιείιται στο rising edge
}

ISR(TCA0_CMP0_vect){
	int intflags = TCA0.SINGLE.INTFLAGS;
	TCA0.SINGLE.INTFLAGS = intflags;
	if(alarm == 0) // λειτουργεί σαν timmer
	{
		interr = 1;
	}
	if(alarm == 1) // λειτουργεί σαν PWM
	{
		PORTD.OUTCLR |= PIN0_bm; // ενεργοποιείιται στο falling edge
	}
}
