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

char rx_buffer[rx_buffer_size];
uint8_t rx_ReadPos = 0;
uint8_t rx_WritePos = 0;

char tx_buffer[tx_buffer_size];
char string_to_send[tx_buffer_size];
char conv_buffer[4];
uint8_t tx_ReadPos = 0;
uint8_t tx_WritePos = 0;
int counter =1, moist_sensor, tmp_sensor;
bool first = true;

int readDS1820();
void lcd_init_sim();
void lcd_clear();
void print(char c);

/*char getChar(void) {
    char ret = '\0';
    if(rx_ReadPos!=rx_WritePos) {
        ret = rx_buffer[rx_ReadPos++];
        if(rx_ReadPos >= rx_buffer_size)
            rx_ReadPos=0;
    }
    return ret;
}

ISR(USART_RXC_vect){
    rx_buffer[rx_WritePos++] = UDR;

    if(rx_WritePos >= rx_buffer_size){
        rx_WritePos = 0;
    }
}

void appendSerial(char c) { //write character to buffer
    tx_buffer[tx_WritePos++] = c;
    if(tx_WritePos>=tx_buffer_size)
        tx_WritePos = 0;
}

ISR(USART_TXC_vect){ //transmit single character
    if(tx_ReadPos != tx_WritePos){
        UDR = tx_buffer[tx_ReadPos++];
    }
    if(tx_ReadPos >= tx_buffer_size){
        tx_ReadPos = 0;
    }
}

void serialWrite(char c[]) {
    for(uint8_t i=0; i<strlen(c); ++i) {
        appendSerial(c[i]); //write all characters to the buffer
    }
    if(UCSRA & (1<<UDRE)) //if buffer has been emptied reset the transmission by sending a null character
        UDR = 0;
}

void sendCommand(char command[]) {
    serialWrite(command);
	
    char c;
    c=getChar();
    while(c!='S'){ //wait until "success" reply from esp
        if(c=='F') { //if command execution failed re-transmit it
            for(int i=0; i<5; ++i)
                getChar(); //flush fail out of read buffer
            serialWrite(command);
        }
        c=getChar();
    }
	PORTB=0xFF;
    for(int i=0; i<8; ++i)
        getChar(); //flush success out of read buffer
}
*/

void USART_init(void)
{
	UCSRA = 0;
	UCSRB = (1<<RXEN) | (1<<TXEN);
	UBRRH = 0;
	UBRRL = 51; //baudrate 9600
	UCSRC = (1 << URSEL) | (3 << UCSZ0);
}
unsigned char USART_receive(void)
{
	while((UCSRA & 0x80) == 0 ){}
	return UDR;
}

void USART_transmit(char input)
{
	while((UCSRA & 0x20) == 0){}
	UDR = input;
}

void serialWrite(char c[]) {
	for(uint8_t i=0; i<strlen(c); ++i) {
		USART_transmit(c[i]); //transmit command one character at a time
		print(c[i]); //debug
	}
}

void sendCommand(char command[]) {
	serialWrite(command);
	unsigned char c;
	
	c=USART_receive();
	PORTB=0xFF; //debug -- never reaches this part
	print(c); //debug
	while(c!='S'){ //wait until "success" reply from esp
		if(c=='F') { //if command execution failed re-transmit it
			for(int i=0; i<5; ++i)
				USART_receive(); //flush fail out of read buffer
			serialWrite(command);
		}
		c=USART_receive();
	}
	for(int i=0; i<8; ++i)
		USART_receive(); //flush success out of read buffer
}


ISR(TIMER1_OVF_vect) {
	PORTB=0xFF;
	while((UCSRA & 0x80) == 1)
		USART_receive(); //flush possible "Served Client" out of the system before sending new data
	
    if(!first) {
        /*itoa(moist_sensor, conv_buffer, 10); //convert value read to string to send it to ESP
        strcpy(string_to_send, "ESP:sensorValue:\"Moist_Sensor\"["); //create the string to send to set the sensor value
        strcat(string_to_send, conv_buffer);
        strcat(string_to_send, "]\n");*/
        sprintf(string_to_send, "ESP:sensorValue:\"Moist_Sensor\"[%d]\n", moist_sensor);

        sendCommand(string_to_send); //send command to set the value of the moisture sensor

        /*itoa(tmp_sensor, conv_buffer, 10);
        strcpy(string_to_send, "ESP:sensorValue:\"Tmp_Sensor\"[");
        strcat(string_to_send, conv_buffer);
        strcat(string_to_send, "]\n");*/

        sprintf(string_to_send, "ESP:sensorValue:\"Tmp_Sensor\"[%.1f]\n", tmp_sensor/2.0);
        sendCommand(string_to_send); //send command to set the value of the temperature sensor
    }
    else
        first=false;
    ADMUX = 0x40; // Vref: Vcc(5V for easyAVR6) and analog input from PINA0 (moisture sensor)
    ADCSRA = ADCSRA | 0x40; // ADC Enable, ADC Interrupt Enable and f = CLK/128

    TCNT1 = 3036;
}

ISR(ADC_vect) {
    moist_sensor = ADCW;
    tmp_sensor = readDS1820();
    if((tmp_sensor&0xFF00)==0xFF00){ //if temperature is negative convert it to the corresponding value
        tmp_sensor--;
        tmp_sensor = tmp_sensor&0x00FF;
        tmp_sensor = tmp_sensor^0x00FF;
        tmp_sensor = -tmp_sensor;
    }
    
    //for debugging
	lcd_clear();
	
	PORTB=0xFF;
	
    sprintf(conv_buffer, "%d", moist_sensor);
    for(int m=0; m<strlen(conv_buffer); ++m)
        print(conv_buffer[m]);

    print('\n');

    sprintf(conv_buffer, "%.1f", tmp_sensor/2.0);
    for(int m=0; m<strlen(conv_buffer); ++m)
        print(conv_buffer[m]);
    //debugging end

}


int main(){

    first = true;
    moist_sensor=tmp_sensor=0;

    DDRA = 0x00; // Port A input
	DDRB= 0xFF;
	
	DDRD=0xFF; //for debugging for lcd
    
	//UBRRH = (ubrr_content >> 8); //set USART Baud Rate Register
    //UBRRL = ubrr_content;
    
    //UCSRB = (1 << TXEN) | (1 << TXCIE); //Transmitter Enable and TX_interrupt enable
    //UCSRC = (1 << UCSZ1) | (1 << UCSZ0); //Char size(8 bits)

    //Receiver and Transmitter Enable, RX_interrupt enable, TX_interrupt enable

    //UCSRB = (1 << TXEN) | (1 << TXCIE) | (1 << RXEN) | (1 << RXCIE);
    //UCSRC = (1 << UCSZ1) | (1 << UCSZ0); //Char size(8 bits)
	
	USART_init();

    TCCR1B = 0x05; //CK/1024
	TCNT1 = 3036; //8s between interrupts
	TIMSK = 0x04; //enable overflow interrupt for TCNT1

    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0)|(1<<ADIE); //ADC enable, frequency = CLK/128 and enable interrupts

    lcd_init_sim();
	
	
    
    //strcpy(string_to_send, "ESP:restart\n");
    //sendCommand(string_to_send);
    
    strcpy(string_to_send, "ESP:ssid: \"Sens_Board1\"\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:addSensor: \"Moist_Sensor\"\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:addSensor:\"Tmp_Sensor\"\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:APStart\n");
    sendCommand(string_to_send);
	
	PORTB=0xFF;

    

    sei();
    while(1){}
}