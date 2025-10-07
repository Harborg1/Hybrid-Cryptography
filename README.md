# Hybrid Cryptography Project

This README provides step-by-step instructions for installing and configuring the required dependencies to build and run a TLS client and server using OpenSSL 3.5. 
The setup enables both classical and hybrid cryptography through the OQS provider.  

**Estimated installation time:** 50–70 minutes

---

## Prerequisites
- [VirtualBox](https://www.virtualbox.org/) installed  
- [Vagrant](https://developer.hashicorp.com/vagrant/downloads) installed  
---

## VM Setup Instructions

### 1. Start VM with Ubuntu 22.04 (jammy64)
In your project folder (where the `Vagrantfile` will be created):

```bash
vagrant init ubuntu/jammy64
vagrant up
vagrant ssh
```
---

### 2. Update the System (inside the VM)

```bash
sudo apt update && sudo apt upgrade -y && sudo apt dist-upgrade -y
```

---

### 3. Install release-manager

```bash
sudo apt install update-manager-core -y
```

---

### 4. Upgrade to Ubuntu 24.04.3

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

### 5. Check Ubuntu version

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

## Connect client and server

### 1. Navigate to vagrant folder

```bash
cd /vagrant
```
---
### 2. Generate RSA key
```bash
   LD_LIBRARY_PATH=/opt/openssl-3.5/lib64 \
  /opt/openssl-3.5/bin/openssl req -x509 -newkey rsa:3072 \
  -keyout key.pem -out cert.pem \
  -days 365 -nodes \
  -subj "/CN=localhost"
```
---
### 4. Build the Code
Compile the server.c and client.c files inside the vagrant folder:
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
---
### 5. Run the Programs
Open **two terminals**:

**Terminal 1 (Server):**
Navigate to the project folder and run
```bash
vagrant ssh
```
Then run the server.
```bash
cd /vagrant
./server
```

**Terminal 2 (Client):**
Navigate to the project folder and run
```bash
vagrant ssh
```
Then run the client.
```bash
cd /vagrant
./client
```

---
### 6. Testing the Connection
- First run the server. It will wait for a client connection.  
- Then run the client. It will establish a TLS 1.3 connection with the server.  
- Both programs should print messages showing that the TLS handshake succeeded.
- In the client terminal,  "Hello from server" will be printed.
- In the server terminal, "Hello from client" will be printed.

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
openssl list -providers
```
Expected output:
```makefile
Providers:
  default
    name: OpenSSL Default Provider
    version: 3.5.0
    status: active
```

```bash
openssl list -providers | grep oqs
```
Expected output:
```makefile
Providers:
 oqsprovider
    name: OpenSSL OQS Provider
    version: 0.10.1-dev
    status: active
```

#### 6.  See the public key algorithms from the OQS provider

```bash
openssl list -public-key-algorithms -provider oqsprovider
```










