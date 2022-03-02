#include <avr/io.h>
#include <string.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#define f_osc 8000000
#define baud  9600
#define ubrr_content ((f_osc/(16*baud))-1)
#define tx_buffer_size 128
#define rx_buffer_size 128
#define sensor_boards 2

char rx_buffer[rx_buffer_size];
uint8_t rx_ReadPos = 0;
uint8_t rx_WritePos = 0;

char tx_buffer[tx_buffer_size];
char string_to_send[tx_buffer_size];
char conv_buffer[6]; //variance is at max 1024^2/4=262144 which has 6 digits
uint8_t tx_ReadPos = 0;
uint8_t tx_WritePos = 0;

int moistures[sensor_boards];
int temperatures[sensor_boards];
int moist_avg, moist_var, tmp_avg, tmp_var;
bool first;


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

void wait_ServedClient() {
    char c;
    c=getChar();
    while(c!='S') {
        c=getChar();
    }
    for(int i=0; i<13; ++i)
        getChar(); //flush ServedClient out of read buffer
}

ISR(TIMER1_OVF_vect) {
    char c;
    bool failed;
    int counter =0, temp;


    

    if(!first) {
        for(int k=0; k<sensor_boards; ++k){
            failed=false;
            /*strcpy(string_to_send, "ESP:ssid:\"Sens_Board");
            itoa(k, conv_buffer, 10);
            strcat(string_to_send, conv_buffer);
            strcat(string_to_send, "\"\n");*/
            sprintf("ESP:ssid:\"Sens_Board%d\"\n", k);
            sendCommand(string_to_send);

            strcpy(string_to_send, "ESP:password:\"awesomePassword\"\n");
            sendCommand(string_to_send);
            strcpy(string_to_send, "ESP:sensorValue:\"Moist_Sensor\"[request]\n");
            sendCommand(string_to_send);
            strcpy(string_to_send, "ESP:sensorValue:\"Tmp_Sensor\"[request]\n");
            sendCommand(string_to_send);
            strcpy(string_to_send, "ESP:connect\n");
            sendCommand(string_to_send);
            strcpy(string_to_send, "ESP:clientTransmit\n");
            sendCommand(string_to_send);
            strcpy(string_to_send, "ESP:getAllValues\n");
            serialWrite(string_to_send);
            //c=getChar();
            while(getChar() != '"' && !failed) { //scan input till you find ". The number will follow
               // c = getChar();
            }
            c=getChar(); //read most significant digit
            if(c=='F')
                failed=true;
            while(c != '"' && !failed){ // read the whole number (until " is read)
                conv_buffer[counter++]=c;
                c = getChar();
            }
            if(!failed){ //fix failed case!!!!!!!!!
                for(int i=5; i>=6-counter; i--){
                    conv_buffer[i] = conv_buffer[i - (6-counter)];
                }
                for(int i=0; i<(6-counter); i++){
                    conv_buffer[i] = '0';
                }
                moistures[k]=atoi(conv_buffer);
            }
            

            //c=getChar();
            while(getChar() != '"' && !failed) {
                //c = getChar();
            }
            c = getChar();
            if(c=='F')
                failed=true;
            while(c != '"' && !failed){
                conv_buffer[counter++]=c;
                c = getChar();
            }
            if(!failed){
                for(int i=5; i>=6-counter; i--){
                    conv_buffer[i] = conv_buffer[i - (6-counter)];
                }
                for(int i=0; i<(6-counter); i++){
                    conv_buffer[i] = '0';
                }
                temperatures[k]=atoi(conv_buffer);
            }
            
        }
        tmp_avg=moist_avg=0;
        for(int i=0; i<sensor_boards; ++i) {
            moist_avg += moistures[i];
            tmp_avg += temperatures[i];
        }
        moist_avg /= sensor_boards;
        tmp_avg /= sensor_boards;

        for(int i=0; i<sensor_boards; ++i) {
            moist_var += (moistures[i]-moist_avg)*(moistures[i]-moist_avg);
            tmp_var += (temperatures[i]-tmp_avg)*(temperatures[i]-tmp_avg);
        }
        moist_var /= sensor_boards;
        tmp_var /= sensor_boards;

        strcpy(string_to_send, "ESP:ssid:\"Middle_Board1\"\n");
        sendCommand(string_to_send);

        /*itoa(moist_avg, conv_buffer, 10); //convert value calculated to string to send it to ESP
        strcpy(string_to_send, "ESP:sensorValue:\"Moist_avg\"["); //create the string to send to set the sensor value
        strcat(string_to_send, conv_buffer);
        strcat(string_to_send, "]\n");*/
        sprintf("ESP:sensorValue:\"Moist_avg\"[%d]\n", moist_avg);
        sendCommand(string_to_send); //send command to set the value of the sensor

        /*itoa(tmp_avg, conv_buffer, 10); //convert value calculated to string to send it to ESP
        strcpy(string_to_send, "ESP:sensorValue:\"Tmp_avg\"["); //create the string to send to set the sensor value
        strcat(string_to_send, conv_buffer);
        strcat(string_to_send, "]\n");*/
        sprintf("ESP:sensorValue:\"Tmp_avg\"[%d]\n", tmp_avg);
        sendCommand(string_to_send); //send command to set the value of the sensor

        /*itoa(moist_var, conv_buffer, 10); //convert value calculated to string to send it to ESP
        strcpy(string_to_send, "ESP:sensorValue:\"Moist_var\"["); //create the string to send to set the sensor value
        strcat(string_to_send, conv_buffer);
        strcat(string_to_send, "]\n");*/
        
        sprintf("ESP:sensorValue:\"Moist_var\"[%d]\n", moist_var);
        sendCommand(string_to_send); //send command to set the value of the sensor

        /*itoa(tmp_var, conv_buffer, 10); //convert value calculated to string to send it to ESP
        strcpy(string_to_send, "ESP:sensorValue:\"Tmp_var\"["); //create the string to send to set the sensor value
        strcat(string_to_send, conv_buffer);
        strcat(string_to_send, "]\n");*/
        sprintf("ESP:sensorValue:\"Tmp_var\"[%d]\n", tmp_var);
        sendCommand(string_to_send); //send command to set the value of the sensor

        strcpy(string_to_send, "ESP:APStart\n");
        sendCommand(string_to_send);

     
    }
    else
        first=false;

    TCNT1 = 3036;
}


int main(){
    UBRRH = (ubrr_content >> 8); //set USART Baud Rate Register
    UBRRL = ubrr_content;

    //Receiver and Transmitter Enable, RX_interrupt enable, TX_interrupt enable
    first=true;
    UCSRB = (1 << TXEN) | (1 << TXCIE) | (1 << RXEN) | (1 << RXCIE);
    UCSRC = (1 << UCSZ1) | (1 << UCSZ0); //Char size(8 bits)

    for(int i=0; i<6; ++i) {
        moistures[i]=0;
        temperatures[i]=0;
    }

    strcpy(string_to_send, "ESP:restart\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:addSensor:\"Moist_Sensor\"\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:addSensor:\"Tmp_Sensor\"\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:addSensor:\"Moist_avg\"\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:addSensor:\"Tmp_avg\"\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:addSensor:\"Moist_var\"\n");
    sendCommand(string_to_send);

    strcpy(string_to_send, "ESP:addSensor:\"Tmp_var\"\n");
    sendCommand(string_to_send);


    TCCR1B = 0x05; //CK/1024
	TCNT1 = 3036;
	TIMSK = 0x04; //enable overflow interrupt for TCNT1
    sei();

    while(1){}
}