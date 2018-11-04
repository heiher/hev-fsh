# HevFsh

HevFsh is a solution for connect to remote shell in local networks.

## How to Build
```bash
git clone --recursive git://github.com/heiher/hev-fsh
cd hev-fsh
make
```

## How to Run

**Server**:
```bash
fsh -s [-l LOG] [SERVER_ADDR:SERVER_PORT]

# Listen on 0.0.0.0:6339 and log to stdout
fsh -s

# Listen on specific address:port
fsh -s 10.0.0.1:8000

# Log to file
fsh -s -l /var/log/fsh.log
```

**Forwarder** (A host in private network):
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
    fsh -f -w 192.168.0.1:22,192.168.1.3:80 10.0.0.1

    # Reject the TCP ports in black list (others allowed)
    fsh -f -b 192.168.0.1:22,192.168.1.3:80 10.0.0.1
    ```

**Connector** (Another host in private network):
* **Terminal**
    ```bash
    fsh SERVER_ADDR[:SERVER_PORT]/TOKEN

    # Connect to forwarder's terminal
    fsh 10.0.0.1/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4
    ```
* **TCP Port**
    ```bash
    fsh -p [LOCAL_ADDR:]LOCAL_PORT:REMOTE_ADD:REMOTE_PORT SERVER_ADDR[:SERVER_PORT]/TOKEN

    # Map the TCP port to forwarder's network service
    fsh -p 2200:192.168.0.1:22 10.0.0.1/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4
    fsh -p 0.0.0.0:2200:192.168.0.1:22 10.0.0.1/8b9bf4e7-b2b2-4115-ac97-0c7f69433bc4
    ```

## Authors
* **Heiher** - https://hev.cc

## License
LGPL
