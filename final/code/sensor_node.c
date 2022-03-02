#include <avr/io.h>
#include <string.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#define f_osc 8000000
#define baud   9600
#define ubrr_content ((f_osc/(16*baud))-1)
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


char getChar(void) {
    char ret = '\0';
    if(rx_ReadPos!=rx_WritePos) {
        ret = rx_buffer[rx_ReadPos++];
        if(rx_ReadPos >= rx_buffer_size)
            rx_ReadPos=0;
    }
    return ret;
}

ISR(USART_RX_vect){
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

ISR(USART_TX_vect){ //transmit single character
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
            for(int i=0; i<4; ++i)
                getChar(); //flush fail out of read buffer
            serialWrite(command);
        }
        c=getChar();
    }
    for(int i=0; i<7; ++i)
        getChar(); //flush success out of read buffer
}

ISR(TIMER1_OVF_vect) {
    if(!first) {
        /*itoa(moist_sensor, conv_buffer, 10); //convert value read to string to send it to ESP
        strcpy(string_to_send, "ESP:sensorValue:\"Moist_Sensor\"["); //create the string to send to set the sensor value
        strcat(string_to_send, conv_buffer);
        strcat(string_to_send, "]\n");*/
        sprintf("ESP:sensorValue:\"Moist_Sensor\"[%d]\n", moist_sensor);

        sendCommand(string_to_send); //send command to set the value of the moisture sensor

        /*itoa(tmp_sensor, conv_buffer, 10);
        strcpy(string_to_send, "ESP:sensorValue:\"Tmp_Sensor\"[");
        strcat(string_to_send, conv_buffer);
        strcat(string_to_send, "]\n");*/

        sprintf("ESP:sensorValue:\"Tmp_Sensor\"[%d]\n", tmp_sensor);
        sendCommand(string_to_send); //send command to set the value of the temperature sensor
    }
    else
        first=false;
    ADMUX = 0x40; // Vref: Vcc(5V for easyAVR6) and analog input from PINA0 (moisture sensor)
    ADCSRA = ADCSRA | 0x40; // ADC Enable, ADC Interrupt Enable and f = CLK/128

    TCNT1 = 3036;
}

ISR(ADC_vect) {
    if(counter++ == 1){
        moist_sensor = ADCW;
        ADMUX = 0x42; // Vref: Vcc(5V for easyAVR6) and analog input from PINA4 (temperature sensor)
        ADCSRA = ADCSRA | 0x40;
    }
    else{
        tmp_sensor = ADCW;
        counter = 1;
    }
}


int main(){

    strcpy(string_to_send, "ESP:restart\n");
    sendCommand(string_to_send);
    //delay???
    strcpy(string_to_send, "ESP:ssid:\"Sens_Board1\"\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:addSensor:\"Moist_Sensor\"\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:addSensor:\"Tmp_Sensor\"\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:APStart\n");
    sendCommand(string_to_send);

    DDRA = 0x00; // Port A input
    UBRRH = (ubrr_content >> 8); //set USART Baud Rate Register
    UBRRL = ubrr_content;
    
    UCSRB = (1 << TXEN) | (1 << TXCIE); //Transmitter Enable and TX_interrupt enable
    UCSRC = (1 << UCSZ1) | (1 << UCSZ0); //Char size(8 bits)

    TCCR1B = 0x05; //CK/1024
	TCNT1 = 3036;
	TIMSK = 0x04; //enable overflow interrupt for TCNT1

    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0)|(1<<ADIE); //ADC enable, frequency = CLK/128 and enable interrupts

    sei();
    while(1){}
}