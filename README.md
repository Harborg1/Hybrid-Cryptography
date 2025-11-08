# Hybrid Cryptography Project

This README provides step-by-step instructions for installing and configuring the dependencies required to build and run a TLS 1.3 connection between a client and a server using OpenSSL 3.5.
The setup supports both classical and hybrid cryptography through the OQS provider.

---

## Prerequisites
- [VirtualBox](https://www.virtualbox.org/) installed  
- [Vagrant](https://developer.hashicorp.com/vagrant/downloads) installed  
---

## VM Setup Instructions

### 1. Clone the github repository.

Clone the github repository and navigate to the project folder.

### 2. Initialize the VM with Ubuntu 22.04 (jammy64)
In your project folder (where the `Vagrantfile` will be created):

```bash
vagrant init ubuntu/jammy64
vagrant up
vagrant ssh
```
---

### 3. Update the System (inside the VM)

```bash
sudo apt update && sudo apt upgrade -y && sudo apt dist-upgrade -y
```

---

### 4. Install release-manager

```bash
sudo apt install update-manager-core -y
```

---

### 5. Upgrade to Ubuntu 24.04.3

```bash
sudo do-release-upgrade
```
- First it checks for a new release.  
- You will be asked to continue → type `y`.  
- It downloads and installs 24.04 packages.  
---

Reboot the VM
```bash
sudo reboot
```
Login to the VM

```bash
vagrant ssh
```

### 6. Check Ubuntu version

```bash
lsb_release -a
```
Expected output:

```makefile
Description:    Ubuntu 24.04.3 LTS
Codename:       noble
```
## Installing OpenSSL 3.5.0 from Source (Vagrant Ubuntu)
---

### For this project, we will use OpenSSL version 3.5.0.  

### 1. Update and install dependencies

```bash
sudo apt update
sudo apt install -y build-essential checkinstall zlib1g-dev wget
```
---
### 2. Download and extract OpenSSL 3.5.0, configure and install

```bash
wget https://www.openssl.org/source/openssl-3.5.0.tar.gz
tar -xvzf openssl-3.5.0.tar.gz
cd openssl-3.5.0

./Configure --prefix=/opt/openssl-3.5 \
            --openssldir=/opt/openssl-3.5 \
            shared zlib enable-autoload-config

make -j$(nproc)
sudo make install
```
---
### 3. Add OpenSSL 3.5.0 to PATH and library path
```bash
nano ~/.bashrc
```
Add these exports at the bottom of the file.
```bash
export PATH=/opt/openssl-3.5/bin:$PATH
export LD_LIBRARY_PATH=/opt/openssl-3.5/lib64:$LD_LIBRARY_PATH
export OPENSSL_MODULES=/opt/openssl-3.5/lib64/ossl-modules
export OPENSSL_CONF=/opt/openssl-3.5/openssl.cnf
```
### 4. Compile bashrc

```bash
source ~/.bashrc
```
### 5. Verify 
```bash
openssl version
```
Expected output:

```makefile
OpenSSL 3.5.0 8 Apr 2025 (Library: OpenSSL 3.5.0 8 Apr 2025)
```
---

## Hybrid cryptography setup

### 1. Clean old installs (if they exist) 
```bash
sudo rm -rf /usr/local/lib/liboqs* /usr/local/include/oqs
sudo rm -rf /usr/local/lib/ossl-modules/oqsprovider.so
sudo rm -rf /opt/openssl-3.5/lib/oqs* /opt/openssl-3.5/lib64/oqs*
sudo rm -rf /opt/openssl-3.5/include/oqs
```
---
### 2. Install dependencies
```bash
sudo apt update
sudo apt install -y cmake ninja-build build-essential git
```
---
### 3. Build & Install liboqs
```bash
#!/bin/bash
set -e  # exit on first error
cd ~
git clone https://github.com/open-quantum-safe/liboqs.git
cd liboqs
rm -rf build
mkdir build && cd build
cmake -GNinja \
  -DCMAKE_INSTALL_PREFIX=/opt/openssl-3.5 \
  -DCMAKE_INSTALL_LIBDIR=lib64 \
  -DBUILD_SHARED_LIBS=ON \
  ..
ninja
sudo ninja install
```
---
#### 3.1 Verify install
```bash
ls -l /opt/openssl-3.5/lib64 | grep oqs
```
Expected output:

```makefile
lrwxrwxrwx 1 root root       11 Oct  3 16:19 liboqs.so -> liboqs.so.8
-rw-r--r-- 1 root root 13114080 Oct  3 16:19 liboqs.so.0.14.1-dev
lrwxrwxrwx 1 root root       20 Oct  3 16:19 liboqs.so.8 -> liboqs.so.0.14.1-dev
```

---
### 4 Install oqs-provider
```bash
cd ~
git clone https://github.com/open-quantum-safe/oqs-provider.git
cd oqs-provider
rm -rf build
mkdir build && cd build

cmake -GNinja \
  -DCMAKE_PREFIX_PATH=/opt/openssl-3.5 \
  -DOPENSSL_ROOT_DIR=/opt/openssl-3.5 \
  -DCMAKE_INSTALL_PREFIX=/opt/openssl-3.5 \
  ..
ninja
sudo ninja install
```

### 5 Open the openssl.cnf file

```bash
sudo nano /opt/openssl-3.5/openssl.cnf
```
Remove openssl_conf = openssl_init after the line  HOME=.

Paste the following into the very top of the file.

It is important that the first line of openssl.cnf is openssl_conf = openssl_init (no blank space).
```makefile
openssl_conf = openssl_init

[openssl_init]
providers = provider_sect

[provider_sect]
default = default_sect

[default_sect]
activate = 1

[oqsprovider_sect]
activate = 1
module = /opt/openssl-3.5/lib64/ossl-modules/oqsprovider.so

```

#### 5.1 Verify
Check the providers.
```bash
openssl list -providers -provider oqsprovider
```
Expected output:
```makefile
vagrant@ubuntu-jammy:~$ openssl list -providers -provider oqsprovider
Providers:
  default
    name: OpenSSL Default Provider
    version: 3.5.0
    status: active
  oqsprovider
    name: OpenSSL OQS Provider
    version: 0.10.1-dev
    status: active
```

#### 6.  See the public key algorithms from the OQS provider

```bash
openssl list -public-key-algorithms -provider oqsprovider
```

## Generate key exchange files and compile the server- and client code.

### 1. Navigate to vagrant folder

```bash
cd /vagrant
```
---
### 2. Generate RSA key and X509 certificate for authentication
```bash
   LD_LIBRARY_PATH=/opt/openssl-3.5/lib64 \
  /opt/openssl-3.5/bin/openssl req -x509 -newkey rsa:3072 \
  -keyout key.pem -out cert.pem \
  -days 365 -nodes \
  -subj "/CN=localhost"
```

---
### 4. Build the Code
Run the make file. This will compile the server.c and client.c files automatically.
```bash
make
```
In case the make file does not work, you can compile the server and client manually instead in the vagrant folder.

```bash
gcc server.c -o server \
  -I/opt/openssl-3.5/include \
  -L/opt/openssl-3.5/lib64 \
  -lssl -lcrypto

gcc client.c -o client \
  -I/opt/openssl-3.5/include \
  -L/opt/openssl-3.5/lib64 \
  -lssl -lcrypto
```
### 5. Verify that the code has compiled

```bash
ls
```

You should see the compiled server and client binaries in the folder.

---
##  Run the tests

Open **two terminals**:

Inside the project folder, login to the VM
```bash
vagrant ssh
```

Go into the vagrant folder

```bash
cd /vagrant
```


### 1. Select test configuration

There are 3 types of tests you can run:

1. TLS connection between client and server

2. Hello message exchange

3. File transfer of ENISA recommendations pdf from server to client

### 2. Running the tests

All 3 tests use the same command structure on both the server and client sides.

On the server side, run:

```bash
./server <PORT-NUMBER> <TEST-TYPE >
```


On the client side, run:

```bash
./client <PORT-NUMBER> <TEST-TYPE>
```
where PORT-NUMBER specifies the port used for establishing the TLS connection and TEST-TYPE is either 0,1, or 2.

#### 1.1 TLS connection 
To establish the TLS connection, run
```bash
./server <PORT-NUMBER> 0
```
in **Terminal 1 (Server):** where the first argument is some port number, e.g 5000 and 0 indicates that we are
running test option 1 , i.e. establishing a TLS connection between client and server.

**Terminal 2 (Client):**
Run the client.
```bash
./client <PORT-NUMBER> 0
```

#### 1.2 Hello Message
To make the Hello Message, run
```bash
./server <PORT-NUMBER> 1
```
in **Terminal 1 (Server):** where 1 indicates that we are running test
option 2, i.e. the Hello Message exchange.

**Terminal 2 (Client):**
Run the client.
```bash
./client <PORT-NUMBER> 1
```

#### 1.3 File Transfer
To make the file transfer, run
```bash
./server <PORT-NUMBER> 2
```
in **Terminal 1 (Server):** where 2 indicates that we are running test option 3,
i.e. the transfer of the ENISA file from the server to the client.

**Terminal 2 (Client):**
Run the client.
```bash
./client <PORT-NUMBER> 2
```
---
# Testing the Connection
- First run the server. It will wait for a client connection.  
- Use the same port number in both terminals.
- Change the last argument (0, 1, or 2) to select the test type.






