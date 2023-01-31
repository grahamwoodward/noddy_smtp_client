# noddy_smtp_client

This is a very very noddy/simply SMTP client...I use the words SMTP client loosely. 

It'll establish an insecure connection to your specified SMTP server on the specified port and at a very basic level allow you to send an email.

There's no support for STARTTLS in this client.

To compile:
  gcc noddy_smtp_client

To run:
  ./a.out \<smtp server\> \<port\>

Following https://en.wikipedia.org/wiki/Simple_Mail_Transfer_Protocol#SMTP_transport_example

we see

S: 220 smtp.example.com ESMTP Postfix
C: HELO relay.example.org
S: 250 Hello relay.example.org, I am glad to meet you
C: MAIL FROM:<bob@example.org>
S: 250 Ok
C: RCPT TO:<alice@example.com>
S: 250 Ok
C: RCPT TO:<theboss@example.com>
S: 250 Ok
C: DATA
S: 354 End data with <CR><LF>.<CR><LF>
C: From: "Bob Example" <bob@example.org>
C: To: "Alice Example" <alice@example.com>
C: Cc: theboss@example.com
C: Date: Tue, 15 Jan 2008 16:02:43 -0500
C: Subject: Test message
C:
C: Hello Alice.
C: This is a test message with 5 header fields and 4 lines in the message body.
C: Your friend,
C: Bob
C: .
S: 250 Ok: queued as 12345
C: QUIT
S: 221 Bye
{The server closes the connection}
  
  The noddy_smtp_client will support the above.
