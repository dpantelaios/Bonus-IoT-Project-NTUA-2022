#include <avr/io.h>
#include <string.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


#define sensor_boards 1
#define board_no 1

#define FOSC 8000000 // Clock Speed gia thn seiriakh
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD-1
#define tx_buffer_size 128

uint8_t rx_ReadPos = 0;
uint8_t rx_WritePos = 0;

char string_to_send[tx_buffer_size];
char string_to_print[tx_buffer_size];
char conv_buffer[6]; //variance is at max 1024^2/4=262144 which has 6 digits
uint8_t tx_ReadPos = 0;
uint8_t tx_WritePos = 0;

int moistures[sensor_boards];
int temperatures[sensor_boards];
int moist_avg, moist_var, tmp_var;
float tmp_avg, tmp_var_f;
bool first;

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
			//PORTB=0x00;
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
	for (int i=0; i<18; ++i) {
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
    char c;
    bool failed;
    int counter =0;
	
	cli();
	//PORTB=PORTB^0xFF;
	
    if(!first) {
        for(int k=1; k<=sensor_boards; ++k){
			counter = 0;
            failed=false;

            sprintf(string_to_send, "ESP:ssid:\"Sens_Board%d\"\n", k); //connect to sensor boards and receive values
            //strcpy(string_to_send, "ESP:ssid:\"Sens_Board1\"\n");
			sendCommand(string_to_send);
			
            strcpy(string_to_send, "ESP:password:\"awesomePassword\"\n");
			//print_string(string_to_send);
            sendCommand(string_to_send);
			
			
			//strcpy(string_to_send, "ESP:debug: \"true\"\n");
			//sendCommand(string_to_send);
			//while(UCSRA&(1<<RXC))
				//usart_receive();
			
			//PORTB=0x00;
            //strcpy(string_to_send, "ESP:getAllValues\n");
			//serialWrite(string_to_send);
			//printResponse();
            //sendCommand(string_to_send);
			PORTB=PORTB^0xFF;
			strcpy(string_to_send, "ESP:sensorValue:\"Moist_Sensor\"[request]\n");
			//serialWrite(string_to_send);
			sendCommand(string_to_send);
			//printResponse();
            strcpy(string_to_send, "ESP:sensorValue:\"Tmp_Sensor\"[request]\n");
            sendCommand(string_to_send);
			//serialWrite(string_to_send);
            //printResponse();
			//PORTB=0xFF;
			//PORTB=0x00;
			//while(UCSRA&(1<<RXC))
			//usart_receive();
			//wait_msec(1000);
			//while(UCSRA&(1<<RXC))
			//usart_receive();
			clear_buffer();
			
            strcpy(string_to_send, "ESP:connect\n");
			sendCommand(string_to_send);
            //serialWrite(string_to_send);
			//printResponse();
			//PORTB=0x00;
			
            strcpy(string_to_send, "ESP:clientTransmit\n");
            sendCommand(string_to_send);
			//serialWrite(string_to_send);
			//printResponse();
			clear_buffer();
            strcpy(string_to_send, "ESP:getValue:\"Moist_Sensor\"\n");
			//sendCommand(string_to_send);
            serialWrite(string_to_send);
			//printResponse();
            
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
                moistures[k-1]=atoi(conv_buffer);
            }
            
			clear_buffer();
            strcpy(string_to_send, "ESP:getValue:\"Tmp_Sensor\"\n");
            serialWrite(string_to_send);
			//printResponse();

            //c=getChar();
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
				//print_string(conv_buffer);
				//print_string("\n");
                temperatures[k-1]=atof(conv_buffer);
            }    
        }


        //calculate averages and variances and transmit them to main node

        tmp_avg=0.0;
        moist_avg=0;
        for(int i=0; i<sensor_boards; ++i) {
            moist_avg += moistures[i];
            tmp_avg += temperatures[i];
        }
        moist_avg /= sensor_boards;
        tmp_avg /= sensor_boards;

        for(int i=0; i<sensor_boards; ++i) {
            moist_var += (moistures[i]-moist_avg)*(moistures[i]-moist_avg);
            tmp_var_f += (temperatures[i]-tmp_avg)*(temperatures[i]-tmp_avg);
        }
        moist_var /= sensor_boards;
        tmp_var_f /= sensor_boards;
        tmp_var = (int)tmp_var_f;
		
		//PORTB=0x00;

        //debug
        sprintf(string_to_send, "%d %.1f %d %d", moist_avg, tmp_avg, moist_var, tmp_var);
        print_string(string_to_send);
        //end_debug




        //strcpy(string_to_send, "ESP:ssid:\"Main_Board\"\n");
        //sendCommand(string_to_send);

        //strcpy(string_to_send, "ESP:password:\"awesomePassword\"\n");
        //sendCommand(string_to_send);
		PORTB=0xFF;
        sprintf(string_to_send, "ESP:sensorValue:\"Moist_avg1\"[%d]\n", moist_avg);
        sendCommand(string_to_send); //send command to set the value of the sensor
		//serialWrite(string_to_send);
		//printResponse();
		
		PORTB=0x00;
		
        sprintf(string_to_send, "ESP:sensorValue:\"Tmp_avg1\"[%.1f]\n", tmp_avg);
        sendCommand(string_to_send); //send command to set the value of the sensor

        sprintf(string_to_send, "ESP:sensorValue:\"Moist_var1\"[%d]\n", moist_var);
        sendCommand(string_to_send); //send command to set the value of the sensor

        sprintf(string_to_send, "ESP:sensorValue:\"Tmp_var1\"[%d]\n", tmp_var);
        sendCommand(string_to_send); //send command to set the value of the sensor
		
		strcpy(string_to_send, "ESP:connect\n");
		sendCommand(string_to_send);
		
		//strcpy(string_to_send, "ESP:clientTransmit\n");
		//sendCommand(string_to_send
		PORTB=0x00;
     
    }
    else
        first=false;

    //TCNT1 = 3036;
	sei();
	TCNT1 = 34286; //4s between interrupts
}


int main(){
    DDRB=0xFF;
    DDRD=0xFF;
	
	//PORTB=0xFF;

	first=true;
	
    lcd_init_sim();
	usart_init(MYUBRR);
	
    for(int i=0; i<sensor_boards; ++i) { //initialize moistures and temperatures
        moistures[i]=0;
        temperatures[i]=0;
    }

	//PORTB=0xFF;
    usart_transmit('\n'); //to flush serial
    	
    strcpy(string_to_send, "ESP:restart\n");
    serialWrite(string_to_send);
    
    usart_receive(); //wait until restart is complete
    while(UCSRA&(1<<RXC)) //flush read buffer
    usart_receive();
	
	wait_msec(2000);
	

    strcpy(string_to_send, "ESP:addSensor: \"Moist_Sensor\"\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:addSensor: \"Tmp_Sensor\"\n");
    sendCommand(string_to_send);

    sprintf(string_to_send, "ESP:addSensor: \"Moist_avg%d\"\n", board_no);
    sendCommand(string_to_send);

    sprintf(string_to_send, "ESP:addSensor: \"Tmp_avg%d\"\n", board_no);
    sendCommand(string_to_send);

    sprintf(string_to_send, "ESP:addSensor: \"Moist_var%d\"\n", board_no);
    sendCommand(string_to_send);

    sprintf(string_to_send, "ESP:addSensor: \"Tmp_var%d\"\n", board_no);
    sendCommand(string_to_send);

	PORTB=0xFF;

    TCCR1B = 0x05; //CK/1024
	//TCNT1 = 3036; //8 seconds
	TCNT1 = 34286; //4s between interrupts
	TIMSK = 0x04; //enable overflow interrupt for TCNT1
    sei();

    while(1){}
}