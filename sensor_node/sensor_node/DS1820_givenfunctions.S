; File Name: one_wire.asm
; Title: one wire protocol
; Target mcu: atmega16
; Development board: easyAVR6
; Assembler: AVRStudio assembler
; Description:
; This file includes routines implementing the one wire protocol over the 4 pin of the microcontroller.
; Dependencies: wait.asm
; Routine: one_wire_receive_byte
; Description:
; This routine generates the necessary read
; time slots to receives a byte from the wire.
; return value: the received byte is returned in r24.
; registers affected: r27:r26 ,r25:r24
; routines called: one_wire_receive_bit

#include <avr/io.h>
#define _SFR_ASM_COMPAT 1
#define __SFR_OFFSET 0

one_wire_receive_byte:
ldi r27 ,8
clr r26
loop_:
rcall one_wire_receive_bit
lsr r26
sbrc r24 ,0
ldi r24 ,0x80
or r26 ,r24
dec r27
brne loop_
mov r24 ,r26
ret

; Routine: one_wire_receive_bit
; Description:
; This routine generates a read time slot across the wire.
; return value: The bit read is stored in the lsb of r24.
; if 0 is read or 1 if 1 is read.
; registers affected: r25:r24
; routines called: wait_usec
one_wire_receive_bit:
sbi DDRA ,4
cbi PORTA ,4 ; generate time slot
ldi r24 ,0x02
ldi r25 ,0x00
rcall wait_usec
cbi DDRA ,4 ; release the line
cbi PORTA ,4
ldi r24 ,10 ; wait 10 μs
ldi r25 ,0
rcall wait_usec
clr r24 ; sample the line
sbic PINA ,4
ldi r24 ,1
push r24
ldi r24 ,49 ; delay 49 μs to meet the standards
ldi r25 ,0 ; for a minimum of 60 μsec time slot
rcall wait_usec ; and a minimum of 1 μsec recovery time
pop r24
ret

; Routine: one_wire_transmit_byte
; Description:
; This routine transmits a byte across the wire.
; parameters:
; r24: the byte to be transmitted must be stored here.
; return value: None.
; registers affected: r27:r26 ,r25:r24
; routines called: one_wire_transmit_bit
one_wire_transmit_byte:
mov r26 ,r24
ldi r27 ,8
_one_more_:
clr r24
sbrc r26 ,0
ldi r24 ,0x01
rcall one_wire_transmit_bit
lsr r26
dec r27
brne _one_more_
ret

; Routine: one_wire_transmit_bit
; Description:
; This routine transmits a bit across the wire.
; parameters:
; r24: if we want to transmit 1
; then r24 should be 1, else r24 should
; be cleared to transmit 0.
; return value: None.
; registers affected: r25:r24
; routines called: wait_usec
one_wire_transmit_bit:
push r24 ; save r24
sbi DDRA ,4
cbi PORTA ,4 ; generate time slot
ldi r24 ,0x02
ldi r25 ,0x00
rcall wait_usec
pop r24 ; output bit
sbrc r24 ,0
sbi PORTA ,4
sbrs r24 ,0
cbi PORTA ,4
ldi r24 ,58 ; wait 58 μsec for the
ldi r25 ,0 ; device to sample the line
rcall wait_usec 
cbi DDRA ,4 ; recovery time
cbi PORTA ,4
ldi r24 ,0x01
ldi r25 ,0x00
rcall wait_usec
ret


; Routine: one_wire_reset
; Description:
; This routine transmits a reset pulse across the wire
; and detects any connected devices.
; parameters: None.
; return value: 1 is stored in r24
; if a device is detected, or 0 else.
; registers affected r25:r24
; routines called: wait_usec
one_wire_reset:
sbi DDRA ,4 ; 4 configured for output
cbi PORTA ,4 ; 480 μsec reset pulse
ldi r24 ,lo8(480)
ldi r25 ,hi8(480)
rcall wait_usec
cbi DDRA ,4 ; 4 configured for input
cbi PORTA ,4
ldi r24 ,100 ; wait 100 μsec for devices
ldi r25 ,0 ; to transmit the presence pulse
rcall wait_usec
in r24 ,PINA ; sample the line
push r24
ldi r24 ,lo8(380) ; wait for 380 μsec
ldi r25 ,hi8(380)
rcall wait_usec
pop r25 ; return 0 if no device was
clr r24 ; detected or 1 else
sbrs r25 ,4
ldi r24 ,0x01
ret

wait_msec:
push r24 ; 2 κύκλοι (0.250 μsec)
push r25 ; 2 κύκλοι
ldi r24 , lo8(998) ; φόρτωσε τον καταχ. r25:r24 με 998 (1 κύκλος - 0.125 μsec)
ldi r25 , hi8(998) ; 1 κύκλος (0.125 μsec)
rcall wait_usec ; 3 κύκλοι (0.375 μsec), προκαλεί συνολικά καθυστέρηση 998.375 μsec
pop r25 ; 2 κύκλοι (0.250 μsec)
pop r24 ; 2 κύκλοι
sbiw r24 , 1 ; 2 κύκλοι
brne wait_msec ; 1 ή 2 κύκλοι (0.125 ή 0.250 μsec)
ret ; 4 κύκλοι (0.500 μsec)

wait_usec:
sbiw r24 ,1 ; 2 κύκλοι (0.250 μsec)
nop ; 1 κύκλος (0.125 μsec)
nop ; 1 κύκλος (0.125 μsec)
nop ; 1 κύκλος (0.125 μsec)
nop ; 1 κύκλος (0.125 μsec)
brne wait_usec ; 1 ή 2 κύκλοι (0.125 ή 0.250 μsec)
ret ; 4 κύκλοι (0.500 μsec)

.global readDS1820
readDS1820:
rcall one_wire_reset ;returns 0 if there is no connected device
sbrs r24, 0
jmp nodevicedetected

ldi r24, 0xCC   ; we know there is only one device
rcall one_wire_transmit_byte
ldi r24, 0x44   ; start measuring temperature
rcall one_wire_transmit_byte

notdone:
rcall one_wire_receive_bit  ; LSB of r24 = 1 if read is complete
sbrs r24, 0
jmp notdone

rcall one_wire_reset
sbrs r24, 0
jmp nodevicedetected
ldi r24, 0xCC   ; we know there is only one device
rcall one_wire_transmit_byte
ldi r24, 0xBE   ; read 16bit temperature measurement
rcall one_wire_transmit_byte
rcall one_wire_receive_byte  ; read 8 LSB 
push r24
rcall one_wire_receive_byte  ; read 8 MSB 
mov r25, r24
pop r24
ret


;sbrs r25, 0
;jmp carry_one
;clc ; set carry flag = 0
;rol r24 ; bit 7-> sign, bit6-0 -> temperature value 
;ldi r25, 0x00
;ret

;carry_one:
;com r24 
;inc r24
;sec ; set carry flag = 1
;rol r24 ; bit 7-> sign, bit6-0 -> abs(temperature value) 
;ldi r25, 0x00
;ret

nodevicedetected:
ldi r25, 0x80
ldi r24, 0x00
ret
