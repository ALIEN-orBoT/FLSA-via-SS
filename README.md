# Background

## Requirements
- A Linux distribution(This project was developed and tested with recent versions of [Ubuntu](https://ubuntu.com/))
- g++ (version >=10)
- [openssl](https://www.openssl.org/)
- [emp-ot](https://github.com/emp-toolkit/emp-ot)

# Getting started
## Install dependencies
Make sure that you have g++ installed.Check that you have installed correctly by running:
```shell
$ g++ --version
```
You should see something like:
```shell
g++ (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0
Copyright (C) 2021 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

Download and install the openssl,emp-ot,and the git tools.
On Ubuntu, you should be able to install the g++,openssl and git tools with
```shell
$ sudo apt-get install g++ git libssl-dev
```
And follow the links above to install the emp-ot.
## Download the source
Clone the git repository by running:
```shell
$ git clone https://github.com/ALIEN-orBoT/FLSA-via-SS.git
```
Enter the Framework directory: `cd FLSA-via-SS`
## Compile and Run
Compile the program with:
```shell
$ g++ server.cxx net_share.cxx -o server -lssl -lcrypto
$ g++ client.cxx net_share.cxx -o client
```
### Usage example
1. Run `./server 0` to start the first server
2. In another window, run `./server 1` to start the second server
3. In another window, run `./server 2` to start the third server
4. In another window, run `./client 10 INTSUMSP` to run a meta-client that sends out client messages
## Supported protocols
- BITSUM
- INTSUM
- ANDOP / OROP

To be updated...
