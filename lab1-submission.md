1.  What was the port number on the client side? 
  - 54801
2.  What was the port number on the server side? 
  - 9877
3.  Briefly explain how these port numbers were decided for the UDP protocol? 
  - The sevrer port number is static and was hardcoded as 9877 in the unp.h file. 
  - The client port is dynamic and temporary. When the client is ran, a random unused port number is assigned by the OS.
4.  How  large  is the  UDP  header? 
  - The UDP header is 8 bytes.
    - 2 bytes for source port
    - 2 bytes for destination port
    - 2 bytes for length
    - 2 bytes for checksum
5.  How  large  is  the  application  data?  (Answer  this  for  just  one  of  your  packets) 
  - For the packet "hello", the length is 6 bytes ('hello' + '\n')
6.  How large are all the headers in one packet?  Give just a single total number.  (Answer this for any one 
of your packets) 
  - 28 bytes for network and transport headers
    - 20 bytes for IP header, containing source and destination IPv4 addresses.
    - 8 bytes for UDP header (calculated previously)
  - 48 bytes including applcation data (6 bytes) and linux "pseudo-header" (14 bytes)
7.  Find the source IP address and the destination IP address from the IP header.  Explain why they are 127.0.0.1?
  - 127.0.0.1 is localhost, aka the loopback address. localhost is the address of the machine itself, meaning the source and destination IP addresses are the same. This makes sense because both the udpserver and client are running locally on the terminal.


```In  your  submission,  write  down  how  many  different  protocols  are visible  with  the  filter  active.  Also,  write  down  how  many  UDP  datagrams  your  program  should  have  sent, and  how  many  it  should  have  received.  Make  sure  to  record  the  input  and  output  from  your  terminal  as well  to  help  the  mentors.  How  many  datagrams  in  Wireshark  appear  to  be  from  either  your  udpserv01  or udpcli01 programs?  Write down this number as well.```

  - I see UDP/XML and NTP protocols with the filter active.
  - 2 datagrams are exchanged when sending a message through the programs - one from the client and one from the server.
  - Since I'm running this on WSL, there's a ton of datagrams between the windows network and wsl network using NTP. There's also some datagrams to a remote IPv6 addres using UDP/XML. They outnumber the datagrams from the UDP prgoram by a lot.

```Internet Checksums```

  - 