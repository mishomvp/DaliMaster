#include <avr/io.h>
#include <util/delay.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "dali.h"
#include "suart.h"
#include "Interpreter.h"
#include "startup_values.h"

const char* username = "";
const char* password = "";

int main()
{
	//input buffer for terminal
	byte buffer[MAX_BUFFER_LENGTH] = {[0 ... MAX_BUFFER_LENGTH-1] = 0};
	fifo_t fifo;	
	word frame;
	int ret;
	int i = 0;	

	//buffer for temporary strings
	byte temp[MAX_BUFFER_LENGTH+1];
	memset(&temp, 0, MAX_BUFFER_LENGTH+1); //clear temp buffer for new strings

	suart_init();
	dali_init();
	fifo_init(&fifo, buffer, MAX_BUFFER_LENGTH);

	load_startup_values();
	send_startup_values(); //default ARC values are stored in EEPROM --> send them to get all lamps into a default setup

	sei();
	//wait until 7 sec no messages via uart --> boot time has ended
	for(i = 0; i < 7000; i++)
	{
		while(suart_getc_nowait () > -1)
		{
			i = 0;
		}
		
		_delay_ms(1);
	}
	
	//now login
	if(username[0] != 0)
	{
		suart_putstring(username);
		suart_putc('\n');
		_delay_ms(1000);
		
		suart_putstring(password);		
	}	

	//if no username exist ---> simply login via pressing enter (no password is set in openwrt router)
	suart_putc('\n');

	//wdt_enable(WDTO_2S ); //watchdog not needed

	for(;;)
	{
		ret = 0;		
		byte c = suart_getc_wait () & 0xFF; //until no char arrived no action is needed!
	  	fifo_put(&fifo, c); //put last char into fifo
		if(c == '\n' || c == '\r') // '\n' is end of command. if char is not '\n' or '\r' begin at front of loop
		{ //command has to be evaluated
			_inline_fifo_get_chars( &fifo, temp, fifo.count); //copy whole fifo into temp (buffer is a ringbuffer, so cannot be used)
			if(strlen((const char*)temp) > 2) //single '\r' or '\n' has not to be evaluated
			{	
				ret = decode_command_to_frame((char*)temp, &frame); //frame is 2 bytes long. decode_command_to_frame is returning type of frame (SIMPLE, REPEAT_TWICE, QUERY)
				if(ret > 0)
				{
					if(_MODE_SIMPLE_ == ret)
					{ 
						if(_ERR_OK_ == dali_send(frame))
							suart_putstring("ACK\n");
						else
							suart_putstring("ERROR\n");
					}
					else if(_MODE_REPEAT_TWICE_ == ret)
					{
						if(_ERR_OK_ == dali_send_with_repeat(frame))
							suart_putstring("ACK\n");
						else
							suart_putstring("ERROR\n");
					}
					else if(_MODE_QUERY_ == ret)
					{
						byte ans;
						if(_ERR_OK_ == dali_query(frame, &ans))
						{
							suart_putstring("ANS ");
							suart_putc(nibble_to_ascii(ans >> 4));
							suart_putc(nibble_to_ascii(ans & 0x0F));
							suart_putc('\n');
						}
						else
							suart_putstring("ERROR\n");
					}
					else if(ret != _ERR_NACK && ret != _ERR_ACK)
						suart_putstring("UNKOWN_MODE\n");
				}
				else
				{	
					suart_putstring("NACK\n");
				}
			}

			memset(&temp, 0, MAX_BUFFER_LENGTH+1); //clear temp buffer for new strings
		}	
	}
	
	return 0;
}
