<h1> Ping using Raw Sockets </h1>
<img src="https://github.com/suyashdamle/Miscellaneous/blob/master/Ping/Screenshot%20from%202018-04-04%2012-35-35.png">


**Details of Implementation**


The function:

``` cpp
	void ping_and_receive()
```
does the main work of **manually creating the IP (v4) and the ICMP headers** according to the standard structures of these headers:


**IPv4 Header**

![IP header](https://github.com/suyashdamle/Miscellaneous/blob/master/Ping/IP_hdr.png)


**ICMP Header**

![ICMP header](https://github.com/suyashdamle/Miscellaneous/blob/master/Ping/icmp-basic-headers.png)






**NOTE:** One hase to prevent the kernel from adding the IP header by itself. The following code segment tells the kernel that the IP header is being included in the packet by the code:

```cpp
	int hdrincl=1; // FOR DISABLING IP HEADER INCLUSION
  	setsockopt(sd, IPPROTO_IP,IP_HDRINCL, &hdrincl, sizeof(hdrincl));
```

