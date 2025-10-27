# HevFsh

[![status](https://github.com/heiher/hev-fsh/actions/workflows/build.yaml/badge.svg?branch=main&event=push)](https://github.com/heiher/hev-fsh)

Fsh helps you access local shell and TCP services behind a NAT or firewall.

**Features**
* Remote shell.
* TCP port forwarding.
* SOCKS5 service.
* IPv4/IPv6. (dual stack)
* End-to-end encryption. (Linux only, it depends on kernel TLS)

```
    +-------------+      +-------------+
    | Connector 1 |      | Connector 2 |
    +-------------+      +-------------+
           ^                    ^
           |                    |
           +------+      +------+
           .      |      |      .
           .      v      v      .
           .     +--------+     .
       (Token 1) | Server | (Token 2)
           .     +--------+     .
           .      ^      ^      .
           .      |      |      .
           +------+      +------+
           |                    |
           v                    v
    +-------------+      +-------------+
    | Forwarder A |      | Forwarder B |
    |   (TCP)     |      |   (Shell)   |
    +-------------+      +-------------+
           ^
           |
           v
     +----------+
     | Upstream |
     |  Server  |
     +----------+
```

## How to Build

```bash
git clone --recursive git://github.com/heiher/hev-fsh
cd hev-fsh
make
```

## How to Run

**Server**:
```bash
fsh -s [SERVER_ADDR:SERVER_PORT]

# Listen on 0.0.0.0:6339 and log to stdout
fsh -s

# Listen on specific address:port
fsh -s 10.0.0.1:8000

# With token allow list
fsh -s -a tokens-allow-list
```

**Forwarder**:
* **Terminal**
    ```bash
    fsh -f [-u USER] SERVER_ADDR[:SERVER_PORT/TOKEN]

    # Set token by server
    fsh -f 10.0.0.1

    # With port and set token by client
    fsh -f 10.0.0.1:8000/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4

    # Specific user (Need run as root)
    fsh -f -u jack 10.0.0.1

    # Need login with username and password (Need run as root)
    # If not run as root, current user used without login
    fsh -f 10.0.0.1
    ```
* **TCP Port**
    ```bash
    fsh -f -p [-w ADDR:PORT,... | -b ADDR:PORT,...] SERVER_ADDR[:SERVER_PORT/TOKEN

    # Accept all TCP ports
    fsh -f -p 10.0.0.1

    # Accept the TCP ports in white list (others rejected)
    fsh -f -p -w 192.168.0.1:22,192.168.1.3:80 10.0.0.1

    # Reject the TCP ports in black list (others allowed)
    fsh -f -p -b 192.168.0.1:22,192.168.1.3:80 10.0.0.1
    ```
* **Socks v5**
    ```bash
    fsh -f -x SERVER_ADDR[:SERVER_PORT/TOKEN
    ```

**Connector**:
* **Terminal**
    ```bash
    fsh SERVER_ADDR[:SERVER_PORT]/TOKEN

    # Connect to forwarder's terminal
    fsh 10.0.0.1/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4
    ```
* **TCP Port**
    ```bash
    fsh -p [LOCAL_ADDR:]LOCAL_PORT:REMOTE_ADD:REMOTE_PORT SERVER_ADDR[:SERVER_PORT]/TOKEN
    fsh -p REMOTE_ADD:REMOTE_PORT SERVER_ADDR[:SERVER_PORT]/TOKEN

    # Map the TCP port to forwarder's network service
    fsh -p 2200:192.168.0.1:22 10.0.0.1/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4
    fsh -p 0.0.0.0:2200:192.168.0.1:22 10.0.0.1/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4

    # Splice to stdio (Support SSH ProxyCommand)
    fsh -p 192.168.0.1:22 10.0.0.1/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4
    ```
* **Socks v5**
    ```bash
    fsh -x [LOCAL_ADDR:]LOCAL_PORT SERVER_ADDR[:SERVER_PORT]/TOKEN
    ```

**Common**:
```bash
fsh [-4 | -6] [-k KEY] [-t TIMEOUT] [-l LOG] [-c TCP_CONGESTION] [-v]

# Resolve names to IPv4 addresses only
fsh -4

# Resolve names to IPv6 addresses only
fsh -6

# End-to-end encryption
# key: random 20-byte
fsh -k /path/to/key

# Session timeout (seconds)
fsh -t 1000

# Log to file
fsh -l /var/log/fsh.log

# TCP congestion control
fsh -c bbr

# Log verbose
fsh -v

# Ugly kTLS workaround for kTLS + splice on older Linux kernels
fsh -U
```

**IPv6**:
```bash
fsh -s [::]:6339

fsh -f [::1]:6339/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4

fsh -f -p -w 127.0.0.1:22,[::1]:22 127.0.0.1/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4

fsh -p [::1]:22 127.0.0.1/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4
fsh -p 2200:[::1]:22 127.0.0.1/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4
fsh -p [::1]:2200:[::1]:22 127.0.0.1/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4
```

### OpenWrt 24.10+

Repo: https://github.com/openwrt/packages/tree/master/net/fsh

```sh
# Install package
opkg install fsh

# Edit /etc/config/fsh

# Restart server
/etc/init.d/fshs restart

# Restart client
/etc/init.d/fshc restart
```

## Classes

```
          +-> HevSocks5 -> HevSocks5Server -> HevSocks5ServerUS
HevObject +-> HevFshBase +-> HevFshServer
          |              +-> HevFshClient
          +-> HevFshTokenManager
          +-> HevFshSessionManager
          +-> HevFshClientFactory
          +-> HevFshIO +-> HevFshSession
                       +-> HevFshClientBase +-> HevFshClientAccept +-> HevFshClientPortAccept
                                            |                      +-> HevFshClientSockAccept
                                            |                      +-> HevFshClientTermAccept
                                            |
                                            +-> HevFshClientConnect +-> HevFshClientPortConnect
                                            |                       +-> HevFshClientSockConnect
                                            |                       +-> HevFshClientTermConnect
                                            |
                                            +-> HevFshClientListen +-> HevFshClientPortListen
                                            |                      +-> HevFshClientSockListen
                                            |
                                            +-> HevFshClientForward
```

## Contributors

* **hev** - https://hev.cc

## License

MIT
