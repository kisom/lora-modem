modem
=====

Use LoRa to send messages. Host frontend coming any day now. For now
9600 8N1 on the modem, using the commands below.

Callsigns must be 5 or 6 characters.

Messages are

SENDER_ID RECVR_ID message...


Commands
--------

+ !ID id - set the modem's ID to id.
+ ?ID - query the current ID
+ !ECHO - enable local echo
+ !NOECHO - disable local echo
+ >MESSAGE - transmit message

New messages will be printed with a trailing '<' and are hex-encoded.
