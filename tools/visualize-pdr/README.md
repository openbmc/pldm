# Overview
pldm_visualise_pdrs.py is a python script that can be used to fetch the PDR's
from the BMC and can parse them to display a full system view.

# Requirements
- Python 3.6+
- graphviz
    - Graphviz is open source graph visualization software. Graph visualization is
      a way of representing structural information as diagrams of abstract graphs
      and networks.
    - There are standard package availabe for graphviz for both rpm based as well
      as the debian based sytem, it can be installed using :

```bash
   RPM Distro : sudo dnf install graphviz
   Debian Distro : sudo apt install graphviz
   Mac Distro : brew install graphviz
   
```
- The `requirements.txt` file should list all Python libraries that the tool depend
on, and they will be installed using:

```bash
    sudo pip3 install -r requirements.txt
```
# Usage

```ascii
usage: pldm_visualise_pdrs.py [-h] --bmc BMC --uname UNAME --passwd PASSWD [--port PORT]

optional arguments:
  -h, --help       show this help message and exit
  --bmc BMC        BMC Ip address/BMC Hostname
  --uname UNAME    BMC username
  --passwd PASSWD  BMC Password
  --port PORT      BMC SSH port

```
- Provide the hostname/IP address of the BMC to connect to.
- Provide the BMC user name.
- Provide the BMC Password.
- Provide the SSH Portnumber.

