# TUN/TAP Sample Utility
This is an example of a simple TUN/TAP utility that creates a TUN/TAP device (based on CLI arguments) and responds to ping requests,
writing a short log message for every request it is able to parse.

## Usage
To build the binary, type:
```bash
make util
```

Here and below we assume that you are running all the commands in root termnal, because this utility and some make commands
(for example, `make ip`) requires root privileges. The best way to achieve this is to run `sudo su` before running commands
below.

To run the utility with default settings (TUN device), type:
```bash
make run-tun
```
(the utility needs root permissions to create a TUN/TAP device)


To make the utility create a TAP device, type:
```bash
make run-tap
```

You may also use a custom IPv4 address:
```bash
make run-tun ip4=172.29.1.2
```
...or an IPv6 address:
```bash
make run-tun ip6=fc00.1.2::2
```
Keep in mind that you have to pass the same variables to all make commands.
Also, keep in mind that we don't validate input data and, when parsing an IPv6 address, assume that
it has a `::` symbol before the last octet (so you can't use full IPv6 addresses)

Then, you need to run the following command in a separate terminal:
```bash
make ip
```

If you run the utility in TAP mode, after running the previous command, you need to type:
```bash
make arp dev=device-name
```

To clean the repository, type:
```bash
make clean
```

## Under the hood
The utility opens the `/dev/net/tun` file and uses the `ioctl` syscall to create a TUN/TAP device with the given name.

After that, you have to manually add an IP address for the newly created device to be able to ping it.

If you are using a TAP device, you also need to add a record to the ARP cache table; otherwise, even after assigning an
IP address to the device, it won't respond to ICMP requests because this utility can't handle ARP requests.

After all preparations are done, you can ping the device created by the utility. Either IP packets (TUN mode) or Ethernet
frames (TAP mode) will be read from the file descriptor the utility holds. Then, the utility will parse, log them and make a
response. To show that you are not pinging localhost, we add a 1-second delay before sending a response;
seeing this in the ping utility clearly shows that we are pinging exactly our utility.

## Testing
To run an automated demo, you may simply type:
```bash
make tun
```

...for testing TUN mode, or:
```bash
make tap
```

If you see a series of IPv4 and IPv6 pings in the terminals, then everything is ok!

## Credits
This utility was created with the help of the following resources:
 - [https://www.gabriel.urdhr.fr/2021/05/08/tuntap/](https://www.gabriel.urdhr.fr/2021/05/08/tuntap/)
 - [https://web.ecs.syr.edu/~wedu/seed/Labs/VPN/](https://web.ecs.syr.edu/~wedu/seed/Labs/VPN/)
 - [https://john-millikin.com/creating-tun-tap-interfaces-in-linux](https://john-millikin.com/creating-tun-tap-interfaces-in-linux)
 - [https://docs.kernel.org/networking/tuntap.html](https://docs.kernel.org/networking/tuntap.html)

We used some code examples from the first source to speed up the development of this utility.
Big thanks to the authors of all mentioned articles!
