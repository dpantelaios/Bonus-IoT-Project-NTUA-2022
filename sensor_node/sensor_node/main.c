#include <avr/io.h>
#include <string.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdio.h>

#define f_osc 8000000
#define baud   9600
#define ubrr_content ((f_osc/(16UL*baud))-1)
#define tx_buffer_size 128
#define rx_buffer_size 128

#define FOSC 8000000 // Clock Speed gia thn seiriakh
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD-1

char rx_buffer[rx_buffer_size];
uint8_t rx_ReadPos = 0;
uint8_t rx_WritePos = 0;

char tx_buffer[tx_buffer_size];
char string_to_send[tx_buffer_size];
char string_to_print[tx_buffer_size];
char conv_buffer[4];
uint8_t tx_ReadPos = 0;
uint8_t tx_WritePos = 0;
int counter =1, moist_sensor, tmp_sensor;
bool first = true;

int readDS1820();
void lcd_init_sim();
void lcd_clear();
void print(char c);

void print_string(char str[]) {
	for(uint8_t i=0; i<strlen(str); ++i) {
		print(str[i]);
	}
}

void wait_msec(int msecs);

void usart_init(unsigned int ubrr){
	UCSRA=0;
	UCSRB=(1<<RXEN)|(1<<TXEN);
	UBRRH=(unsigned char)(ubrr>>8);
	UBRRL=(unsigned char)ubrr;
	UCSRC=(1 << URSEL) | (3 << UCSZ0);
	return;
}

void usart_transmit(uint8_t data){
	while(!(UCSRA&(1<<UDRE)));
	UDR=data;
}

uint8_t usart_receive(){
	while(!(UCSRA&(1<<RXC)));
	return UDR;
}

void serialWrite(char c[]) {
	for(uint8_t i=0; i<strlen(c); ++i) {
		usart_transmit(c[i]); //transmit command one character at a time
		//print(c[i]); //debug
	}
}

void sendCommand(char command[]) {
	serialWrite(command);
	unsigned char c;
	
	c=usart_receive();
	//PORTB=0xFF; //debug -- never reaches this part
	//print(c); //debug
	while(c!='S'){ //wait until "success" reply from esp
		if(c=='F') { //if command execution failed re-transmit it
			while(UCSRA&(1<<RXC))
			usart_receive(); //flush fail out of read buffer
			//PORTB=0xFF;
			PORTB=0x00;
			serialWrite(command);
		}
		c=usart_receive();
	}
	//PORTB=0xFF;
	while(UCSRA&(1<<RXC))
	usart_receive(); //flush success out of read buffer
}


void printResponse() {
	int i=0;
	char c;
	for (int i=0; i<19; ++i) {
		c=usart_receive();
		if(c=='\n')
		c='U';
		string_to_print[i]=c;
	}
	print_string(string_to_print);
}

void wait_ServedClient() {
	char c;
	c=usart_receive();
	while(c!='S') {
		c=usart_receive();
	}
	while(UCSRA&(1<<RXC))
	usart_receive(); //flush ServedClient out of read buffer
}

ISR(TIMER1_OVF_vect) {
	//PORTB=0x00;
	
	//while((UCSRA & 0x80) == 1)
		//usart_receive(); //flush possible "Served Client" out of the system before sending new data
	
	PORTB=PORTB^0xFF;
    if(!first) {
        /*itoa(moist_sensor, conv_buffer, 10); //convert value read to string to send it to ESP
        strcpy(string_to_send, "ESP:sensorValue:\"Moist_Sensor\"["); //create the string to send to set the sensor value
        strcat(string_to_send, conv_buffer);
        strcat(string_to_send, "]\n");*/
        sprintf(string_to_send, "ESP:sensorValue:\"Moist_Sensor\"[%d]\n", moist_sensor);
		
		print_string(string_to_send);
        sendCommand(string_to_send); //send command to set the value of the moisture sensor

        /*itoa(tmp_sensor, conv_buffer, 10);
        strcpy(string_to_send, "ESP:sensorValue:\"Tmp_Sensor\"[");
        strcat(string_to_send, conv_buffer);
        strcat(string_to_send, "]\n");*/

        sprintf(string_to_send, "ESP:sensorValue:\"Tmp_Sensor\"[%.1f]\n", tmp_sensor/2.0);
        sendCommand(string_to_send); //send command to set the value of the temperature sensor
    }
    else{
        first=false;
	}
    ADMUX = 0x40; // Vref: Vcc(5V for easyAVR6) and analog input from PINA0 (moisture sensor)
    ADCSRA = ADCSRA | 0x40; // ADC Enable, ADC Interrupt Enable and f = CLK/128

    //TCNT1 = 3036;
	TCNT1 = 34286; //4s between interrupts
}

ISR(ADC_vect) {
	//PORTB=PORTB^0xFF;
    moist_sensor = ADCW;
    //tmp_sensor = readDS1820();
	tmp_sensor=20;
    if((tmp_sensor&0xFF00)==0xFF00){ //if temperature is negative convert it to the corresponding value
        tmp_sensor--;
        tmp_sensor = tmp_sensor&0x00FF;
        tmp_sensor = tmp_sensor^0x00FF;
        tmp_sensor = -tmp_sensor;
    }
    
    //for debugging
	lcd_clear();
	
	//PORTB=0x00;
	/*
    sprintf(conv_buffer, "%d", moist_sensor);
    for(int m=0; m<strlen(conv_buffer); ++m)
        print(conv_buffer[m]);

    print('\n');

    sprintf(conv_buffer, "%.1f", tmp_sensor/2.0);
    for(int m=0; m<strlen(conv_buffer); ++m)
        print(conv_buffer[m]);
    //debugging end
	*/
}


int main(){

    first = true;
    moist_sensor=tmp_sensor=0;

    DDRA = 0x00; // Port A input
	DDRB= 0xFF;
	
	DDRD=0xFF; //for debugging for lcd	
	PORTB = 0xFF;
	
    TCCR1B = 0x05; //CK/1024
	//TCNT1 = 3036; //8s between interrupts
	TCNT1 = 34286; //4s between interrupts
	TIMSK = 0x04; //enable overflow interrupt for TCNT1

    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0)|(1<<ADIE); //ADC enable, frequency = CLK/128 and enable interrupts

    lcd_init_sim();
	
	usart_init(MYUBRR);
	usart_transmit('\n');
    
    strcpy(string_to_send, "ESP:restart\n");
    serialWrite(string_to_send);
	
	usart_receive(); //wait until restart is complete
	while(UCSRA&(1<<RXC))
	usart_receive();
	
	wait_msec(2000);
	
	PORTB=0xFF;
    strcpy(string_to_send, "ESP:ssid:\"Sens_Board1\"\n");
    serialWrite(string_to_send);
	//printResponse();
	
    strcpy(string_to_send, "ESP:addSensor: \"Moist_Sensor\"\n");
    sendCommand(string_to_send);
	

    strcpy(string_to_send, "ESP:addSensor: \"Tmp_Sensor\"\n");
    sendCommand(string_to_send);
	
	
    strcpy(string_to_send, "ESP:APStart\n");
    serialWrite(string_to_send);
	//printResponse();
	PORTB=0x00;
	
    sei();
	PORTB=0x00;
    while(1){}
}