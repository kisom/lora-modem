#include <Arduino.h>
#include <string.h>

#include <feather/feather.h>
#include <feather/wing/wing.h>
#include <feather/wing/rfm95.h>
#include <Streaming.h>

#if defined(PAGER)
#include <feather/wing/oled.h>

OLED	display;
#endif


#if defined(FEATHER_M4)
FeatherM4	board;
RFM95		radio(6, 5, 9);
#elif defined(FEATHER_M0)
// The FeatherM0 variant has an onboard radio.
FeatherM0	board;
RFM95		radio;
#endif

// RFM95(uint8_t cs, uint8_t irq, uint8_t rst) allows setting the pins explicitly.


const int	BUFLEN = 255;

// usrbuf is the incoming buffer from the user.
char		usrbuf[BUFLEN] = {0};
uint8_t		usridx = 0;

// rfmbuf is the buffer from the radio.
char		rfmbuf[BUFLEN] = {0};
uint8_t		rfmlen = 0;


// ident is the callsign or identity of the user.
const uint8_t	MAX_IDENT = 6;
char		ident[MAX_IDENT+1] = "";
uint8_t		identlen = 5;


static bool	localEcho = false;


void
setup()
{
	// Wait for serial connection because there's not much to do
	// otherwise.
	Serial.begin(9600);
	while (!Serial) ;
	registerWing(&radio);
#if defined(PAGER)
	registerWing(&display);
#endif

	if (!initialiseWings()) {
		Serial.println("!BOOT FAILED");
		while (true) ;
	}

	Serial.println("!BOOT OK");
	scheduleWingTasks();

#if defined(PAGER)
	display.print(0, "BOOT OK");
#endif
}


void
processInput()
{
	char	rfbuf[252] = {0};
	char	rfbuflen = 0;
	char	cmd[8] = {0};
	uint8_t	cmdlen = 0;

	if (usridx == 0) {
		return;
	}

	switch (usrbuf[0]) {
	case '>':
		if (identlen < 5) {
			Serial.println("* NO/INVALID IDENT");
			break;
		}
		memcpy(rfbuf, ident, identlen);
		rfbuf[identlen] = 0x20;
		rfbuflen = identlen + 1;
		memcpy(rfbuf+rfbuflen, usrbuf+1, usridx-1);
		rfbuflen += (usridx - 1);
		radio.transmit((uint8_t *)rfbuf, rfbuflen, NULL);
		Serial.println("*TX SENT");
#if defined(PAGER)
		display.print(2, "TX SENT");
#endif
		break;
	case '?':
		while (usrbuf[cmdlen+1] != ' ' && usrbuf[cmdlen+1] != 0 && cmdlen < 8) {
			cmd[cmdlen] = usrbuf[cmdlen+1];
			cmdlen++;
		}

		if (cmdlen >= 8) {
			Serial.println("!INVALID COMMAND");
			break;
		}

		if (strncmp(cmd, "ID", 3) == 0) {
			char	buf[14] = {0};
			sprintf(buf, "*ID:%s", ident);
			Serial.println(buf);
			break;
		}
		else {
			Serial.println("!COMMAND UNK");
			break;
		}
		break;
	case '!':
		while (usrbuf[cmdlen+1] != ' ' && usrbuf[cmdlen+1] != 0 && cmdlen < 8) {
			cmd[cmdlen] = usrbuf[cmdlen+1];
			cmdlen++;
		}

		if (cmdlen >= 8) {
			Serial.println("!INVALID COMMAND");
			break;
		}

		if (strncmp(cmd, "ID", 3) == 0) {
			identlen = 0;
			uint8_t	i = cmdlen + 2;
			while ((i < usridx)		&&
			       (identlen < MAX_IDENT)	&&
			       (usrbuf[i] != ' ')	&&
			       (usrbuf[i] != 0x0))	{
				ident[identlen++] = usrbuf[i++];
			}
			Serial.println("*ID SET");
			break;
		}
		else if (strncmp(cmd, "ECHO", 5) == 0) {
			localEcho = true;
			break;
		}
		else if (strncmp(cmd, "NOECHO", 7) == 0) {
			localEcho = false;
			break;
		}
		
	default:
		Serial.print("!0x");
		Serial.print(usrbuf[0], HEX);
		Serial.println("???");
	}

	// Finally, reset the usrbuf.
	usridx = 0;
	memset(usrbuf, 0, BUFLEN);
}


void
loop()
{
	char	ch;
	char	hex[2];
	uint8_t	idscanlen = 0;
	bool	idscan = false;
	uint8_t	rcvrStart = 0;

	// Try to read as much off the serial port as possible before
	// potentially dumping data back onto it.
	while (Serial.available()) {
		ch = Serial.read();
		if ((ch == 0x08) && usridx) {
			usrbuf[usridx-1] = 0;
			usridx--;
			if (localEcho) {
				Serial.print(ch);
			}
			continue;
		}

		// New line or carriage return signal end of user processing.
		if ((ch == 0x0a) || (ch == 0x0d)) {
			if (localEcho) {
				Serial.println();
			}
			processInput();
			continue;
		} else {
			if (localEcho) {
				Serial.print(ch);
			}
		}

		// If the serial input is too long, reset the buffer and
		// reset.
		if (usridx == (BUFLEN - 1)) {
			Serial.println("!BUF OVERFLOW");
			usridx = 0;
			return;
		}

		usrbuf[usridx++] = ch;
	}

	if (radio.available()) {
		// Message received.
		rfmlen = BUFLEN;
		if (!radio.receive((uint8_t *)(rfmbuf), &rfmlen, NULL)) {
			Serial.println("!RX FAILED");
			rfmlen = 0;
			return;
		}

		for (idscanlen = 0; idscanlen < rfmlen; idscanlen++) {
			// Done scanning receiver.
			if (idscan && (rfmbuf[idscanlen] == 0x20)) {
				break;
			}
			else if (rfmbuf[idscanlen] == 0x20) {
				idscan = true;
			}
			else if (idscan && !rcvrStart) {
				rcvrStart = idscanlen;
			}
		}

		// Filter out messages not meant for this node.
		if ((idscanlen-rcvrStart) != identlen) {
			return;
		}

		if (strncmp(rfmbuf+rcvrStart, ident, identlen) != 0) {
			return;
		}


		Serial.print("<");
		for (int i = 0; i < rfmlen; i++) {
			sprintf(hex, "%02x", rfmbuf[i]);
			Serial.print(hex);
		}
		Serial.println();

#if defined(PAGER)
		char	sender[7] = {0};
		char	tline[21] = {0};
		memcpy(sender, rfmbuf, rcvrStart-1);
		sprintf(tline, "DE %s", sender);
		display.iprint(0, tline);
		display.print(1, rfmbuf+idscanlen+1);
#endif
		rfmlen = 0;
	}
}

