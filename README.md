# noddy_smtp_client

This is a very very noddy/simply SMTP client...I use the words SMTP client loosely. 

It'll establish an insecure connection to your specified SMTP server on the specified port and at a very basic level allow you to send an email.

There's no support for STARTTLS in this client.

To compile:
  gcc noddy_smtp_client

To run:
  ./a.out \<smtp server\> \<port\>
