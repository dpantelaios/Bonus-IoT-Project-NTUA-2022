#include <avr/io.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#define f_osc 8000000
#define baud  9600
#define ubrr_content ((f_osc/(16*baud))-1)
#define tx_buffer_size 128
#define rx_buffer_size 128
#define middle_boards 1

#define FOSC 8000000 // Clock Speed gia thn seiriakh
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD-1

char rx_buffer[rx_buffer_size];
uint8_t rx_ReadPos = 0;
uint8_t rx_WritePos = 0;

char string_to_send[tx_buffer_size];
char string_to_print[128];
char conv_buffer[6]; //variance is at max 1024^2/4=262144 which has 6 digits
uint8_t tx_ReadPos = 0;
uint8_t tx_WritePos = 0;

bool first;
bool dry[middle_boards];
int moist_avgs[middle_boards];
float tmp_avgs[middle_boards];
int moist_vars[middle_boards];
int tmp_vars[middle_boards];

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

void clear_buffer(){
	wait_msec(100);
	while(UCSRA&(1<<RXC))
	usart_receive(); //flush success out of read buffer
}

void serialWrite(char c[]) {
	for(uint8_t i=0; i<strlen(c); ++i) {
		usart_transmit(c[i]); //transmit command one character at a time
	}
}

void sendCommand(char command[]) {
	serialWrite(command);
	unsigned char c;
	
	c=usart_receive();
	while(c!='S'){ //wait until "success" reply from esp
		if(c=='F') { //if command execution failed re-transmit it
			while(UCSRA&(1<<RXC))
			usart_receive(); //flush fail out of read buffer
			serialWrite(command);
		}
		c=usart_receive();
	}
	while(UCSRA&(1<<RXC))
	usart_receive(); //flush success out of read buffer
}

void wait_ServedClient() {
	char c;
	c=usart_receive();
	while(c!='S') {
		c=usart_receive();
	}
	for(int i=0; i<5; ++i)
	usart_receive(); //flush ServedClient out of read buffer
}

void printResponse() {
	int i=0;
	char c;
	for (int i=0; i<13; ++i) {
		c=usart_receive();
		if(c=='\n')
		c='U';
		string_to_print[i]=c;
	}
	print_string(string_to_print);
}

ISR(TIMER1_OVF_vect) {
	cli();
    char c;
    bool failed;
    int counter =0, watering_pot = 0, leds;
	
	PORTB = PORTB^0xFF;
    if(!first) {
        for(int k=1; k<=middle_boards; ++k){
            failed=false;
			counter =0;
			
			//get moisture average
			PORTB = 0x00;
			clear_buffer(); //flush potential ServedClient
			sprintf(string_to_send, "ESP:getValue:\"Moist_avg%d\"\n", k);
			serialWrite(string_to_send);

            counter=0;
            while(usart_receive() != '"' && !failed); //scan input till you find ". The number will follow
            c=usart_receive(); //read most significant digit
            if(c=='F')
            failed=true;
            while(c != '"' && !failed){ // read the whole number (until " is read)
	            conv_buffer[counter++]=c;
	            c = usart_receive();
            }
            c = usart_receive(); // also flush '\n' out of read buffer
            if(!failed){
	            for(int i=5; i>=6-counter; i--){ // place number at the end of the buffer and fill the start of it with 0s so that it can be converted to an int
		            conv_buffer[i] = conv_buffer[i - (6-counter)];
	            }
	            for(int i=0; i<(6-counter); i++){
		            conv_buffer[i] = '0';
	            }
	            //print_string(conv_buffer);
	            moist_avgs[k-1]=atoi(conv_buffer);
            }
			
			//get temperature average
			clear_buffer(); //flush potential ServedClient
			sprintf(string_to_send, "ESP:getValue:\"Tmp_avg%d\"\n", k);
			serialWrite(string_to_send);
            
			counter = 0;
			while(usart_receive() != '"' && !failed);
			c = usart_receive();
			if(c=='F')
			failed=true;
			while(c != '"' && !failed){
				conv_buffer[counter++]=c;
				c = usart_receive();
			}
			c = usart_receive(); // also flush '\n' out of read buffer
			if(!failed){
				for(int i=5; i>=6-counter; i--){ // place number at the end of the buffer and fill the start of it with 0s so that it can be converted to an int
					conv_buffer[i] = conv_buffer[i - (6-counter)];
				}
				for(int i=0; i<(6-counter); i++){
					conv_buffer[i] = '0';
				}
				tmp_avgs[k-1]=atof(conv_buffer);
			}
		
		
			//get moisture variance
			clear_buffer(); //flush potential ServedClient
			sprintf(string_to_send, "ESP:getValue:\"Moist_var%d\"\n", k);
			serialWrite(string_to_send);
            
            counter=0;
            while(usart_receive() != '"' && !failed); //scan input till you find ". The number will follow
            c=usart_receive(); //read most significant digit
            if(c=='F')
            failed=true;
            while(c != '"' && !failed){ // read the whole number (until " is read)
	            conv_buffer[counter++]=c;
	            c = usart_receive();
            }
            c = usart_receive(); // also flush '\n' out of read buffer
            if(!failed){
	            for(int i=5; i>=6-counter; i--){ // place number at the end of the buffer and fill the start of it with 0s so that it can be converted to an int
		            conv_buffer[i] = conv_buffer[i - (6-counter)];
	            }
	            for(int i=0; i<(6-counter); i++){
		            conv_buffer[i] = '0';
	            }
	            //print_string(conv_buffer);
	            moist_vars[k-1]=atoi(conv_buffer);
            }
			
			
			//get temperature variance
			clear_buffer(); //flush potential ServedClient
			sprintf(string_to_send, "ESP:getValue:\"Tmp_var%d\"\n", k);
			serialWrite(string_to_send);
            
			counter=0;
			while(usart_receive() != '"' && !failed); //scan input till you find ". The number will follow
			c=usart_receive(); //read most significant digit
			if(c=='F')
			failed=true;
			while(c != '"' && !failed){ // read the whole number (until " is read)
				conv_buffer[counter++]=c;
				c = usart_receive();
			}
			c = usart_receive(); // also flush '\n' out of read buffer
			if(!failed){
				for(int i=5; i>=6-counter; i--){ // place number at the end of the buffer and fill the start of it with 0s so that it can be converted to an int
					conv_buffer[i] = conv_buffer[i - (6-counter)];
				}
				for(int i=0; i<(6-counter); i++){
					conv_buffer[i] = '0';
				}
				tmp_vars[k-1]=atoi(conv_buffer);
			}
        }
        if(!failed) {
            lcd_clear();
            leds = 1;
            watering_pot=0;
            for(int k=1; k<=middle_boards; ++k){//fix temperature!!!!!!
                sprintf(string_to_print, "T%d: %.1f ", k, tmp_avgs[k-1]);
                print_string(string_to_print);

                if(moist_avgs[k-1]>=640){
                    strcpy(string_to_print, "VDRY");
                    dry[k-1]=1;
                }
                else if(moist_avgs[k-1]>=410){
                    dry[k-1]=1;
                    strcpy(string_to_print, "DRY");
                }
                else if(moist_avgs[k-1]>=200) {
                    dry[k-1]=0;
                    strcpy(string_to_print, "HUM");
                }
                else{
                    dry[k-1]=0;
                    strcpy(string_to_print, "VHUM");
                }

                print_string(string_to_print);
				print('\n'); //change line wont work on actual LCD
				
				if(tmp_vars[k-1]>=15) {
					strcpy(string_to_print, "TMP VAR! ");
					print_string(string_to_print);
				}
				if(moist_vars[k-1]>=80) {
					strcpy(string_to_print, "MST VAR!");
					print_string(string_to_print);
				}
                
                if(dry[k-1])
                    watering_pot = watering_pot | leds;
                leds = leds << 1;
            }
            PORTB = watering_pot;
        }
    }
    else
        first=false;
	sei();
    TCNT1 = 3036;
}


int main() {
	first = true;
	DDRB = 0xFF;
	DDRD = 0xFF;
	
	lcd_init_sim();
	usart_init(MYUBRR);
	lcd_clear();
	
    for(int i=0; i<middle_boards; ++i) { //initialize_values
        moist_avgs[i]=0;
        tmp_avgs[i]=0.0;
        moist_vars[i]=0;
        tmp_vars[i]=0;
    }

    usart_transmit('\n'); //to flush serial
    
    strcpy(string_to_send, "ESP:restart\n");
    serialWrite(string_to_send);
    
    usart_receive(); //wait until restart is complete
    while(UCSRA&(1<<RXC)) //flush read buffer
		usart_receive();
    
    wait_msec(4000);
	
	while(UCSRA&(1<<RXC)) //flush read buffer
	usart_receive();
	
	strcpy(string_to_send, "ESP:ssid:\"Main_Board\"\n");
	sendCommand(string_to_send);
	
	strcpy(string_to_send, "ESP:addSensor: \"Moist_Sensor\"\n");
	sendCommand(string_to_send);

	strcpy(string_to_send, "ESP:addSensor: \"Tmp_Sensor\"\n");
	sendCommand(string_to_send);
	
	for(int i=1; i<=middle_boards; ++i) {

		sprintf(string_to_send, "ESP:addSensor: \"Moist_avg%d\"\n", i);
		sendCommand(string_to_send);

		sprintf(string_to_send, "ESP:addSensor: \"Tmp_avg%d\"\n", i);
		sendCommand(string_to_send);

		sprintf(string_to_send, "ESP:addSensor: \"Moist_var%d\"\n", i);
		sendCommand(string_to_send);

		sprintf(string_to_send, "ESP:addSensor: \"Tmp_var%d\"\n", i);
		sendCommand(string_to_send);
		
		sprintf(string_to_send, "ESP:sensorValue:\"Moist_avg%d\"[%d]\n", i, 0);
		sendCommand(string_to_send); //initialize value of sensors to 0
		
		sprintf(string_to_send, "ESP:sensorValue:\"Tmp_avg%d\"[%.1f]\n", i, 0.0);
		sendCommand(string_to_send);

		sprintf(string_to_send, "ESP:sensorValue:\"Moist_var%d\"[%d]\n", i, 0);
		sendCommand(string_to_send);

		sprintf(string_to_send, "ESP:sensorValue:\"Tmp_var%d\"[%d]\n", i, 0);
		sendCommand(string_to_send);
		
	}
	
	strcpy(string_to_send, "ESP:APStart\n");
	sendCommand(string_to_send);
	
	PORTB = 0xFF;
    TCCR1B = 0x05; //CK/1024
	TCNT1 = 3036; //8 seconds
	TIMSK = 0x04; //enable overflow interrupt for TCNT1
    sei();
	
    while(1){    
    }
}