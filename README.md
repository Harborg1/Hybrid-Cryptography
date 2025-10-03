# Hybrid Crypto TLS Project

This project demonstrates a simple **TLS 1.3 client-server** communication using **C** and **OpenSSL** inside a Vagrant-managed Ubuntu 24.04 LTS VM.

---

## Prerequisites
- [VirtualBox](https://www.virtualbox.org/) installed  
- [Vagrant](https://developer.hashicorp.com/vagrant/downloads) installed  
- Git (optional, for cloning the repo)

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

### 4. Upgrade to Ubuntu 24.04

```bash
sudo do-release-upgrade
```

- First it checks for a new release.  
- You will be asked to continue → type `y`.  
- It downloads and installs 24.04 packages.  
- Finally, the system restarts.  

---

### 5. Log in again and check version

```bash
vagrant ssh
lsb_release -a
```

Expected output:

```makefile
Description:    Ubuntu 24.04.x LTS
Codename:       noble
```
## Installing OpenSSL 3.5.0 from Source (Vagrant Ubuntu)

Sometimes you may need the latest OpenSSL version instead of the one shipped with Ubuntu.  
---

### 1. Update and install dependencies

```bash
sudo apt update
sudo apt install -y build-essential checkinstall zlib1g-dev wget
```

---

### 2. Download and extract OpenSSL 3.5.0

```bash
wget https://www.openssl.org/source/openssl-3.5.0.tar.gz
tar -xvzf openssl-3.5.0.tar.gz
cd openssl-3.5.0
```

---

### 3. Configure the build (installing under `/opt/openssl-3.5`)

```bash
./Configure --prefix=/opt/openssl-3.5 --openssldir=/opt/openssl-3.5 shared zlib
```

---

### 4. Build and install

```bash
make -j$(nproc)
sudo make install
```

---

### 5. Add OpenSSL 3.5.0 to PATH and library path permanently

```bash
echo 'export PATH=/opt/openssl-3.5/bin:$PATH' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=/opt/openssl-3.5/lib64:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```

---

### 6. Verify installation
```bash
LD_LIBRARY_PATH=/opt/openssl-3.5/lib64 /opt/openssl-3.5/bin/openssl version
```

Expected output:

```bash
OpenSSL 3.5.0  8 Apr 2025 (Library: OpenSSL 3.5.0  8 Apr 2025)
```
---

## TLS Setup Instructions

### 1. Install Required Packages (inside the VM)

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install build-essential git openssl libssl-dev libsodium-dev -y
```

---

### 2. Navigate to the Project Folder

```bash
cd /vagrant
```

---

### 3. Generate Certificates
Generate a self-signed RSA key and certificate for the server:

```bash
LD_LIBRARY_PATH=/opt/openssl-3.5/lib64 \
  /opt/openssl-3.5/bin/openssl req -x509 -newkey rsa:3072 \
  -keyout key.pem -out cert.pem \
  -days 365 -nodes \
  -subj "/CN=localhost"
```

This creates:  
- `key.pem` → server’s private key  
- `cert.pem` → server’s certificate  

---

### 4. Build the Code
Compile the server and client:

```bash
gcc server.c -o server \
  -I/opt/openssl-3.5/include \
  -L/opt/openssl-3.5/lib64 \
  -lssl -lcrypto


gcc server.c -o client \
  -I/opt/openssl-3.5/include \
  -L/opt/openssl-3.5/lib64 \
  -lssl -lcrypto

```

---

### 5. Run the Programs

Open **two terminals**:

**Terminal 1 (Server):**
```bash
vagrant ssh
cd /vagrant
./server
```

**Terminal 2 (Client):**
```bash
vagrant ssh
cd /vagrant
./client
```

---

## Testing the Connection
- When you run the server, it will wait for a client connection.  
- When you run the client, it will establish a TLS 1.3 connection with the server.  
- Both programs should print messages showing that the TLS handshake succeeded and that encrypted data was exchanged.

---
