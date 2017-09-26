# HevFsh

HevFsh is a solution for connect to remote shell in local networks.

## How to Build
```bash
git clone git://github.com/heiher/hev-fsh
cd hev-fsh
git submodule init
git submodule update
make
```

## How to Run

**Forwarding Server**:
```bash
# public address: 10.0.0.1

bin/hev-fsh -a 0.0.0.0 -p 6339

# log to file
bin/hev-fsh -a 0.0.0.0 -p 6339 -l /var/log/fsh.log
```

**Server**:
```bash
# login (require root)
bin/hev-fsh -s 10.0.0.1 -p 6339

# current user
bin/hev-fsh -s 10.0.0.1 -p 6339

# specific user (require root)
bin/hev-fsh -s 10.0.0.1 -p 6339 -u nobody
```

**Client**:
```bash
bin/hev-fsh -s 10.0.0.1 -p 6339 -c TOKEN
```

## Authors
* **Heiher** - https://hev.cc

## License
LGPL
