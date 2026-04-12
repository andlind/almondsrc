#!/bin/bash
#  ##     ####     ##   ##   ## ##   ###  ##  ### ##  # 
#   ##     ##       ## ##   ##   ##    ## ##   ##  ## # 
# ## ##    ##      # ### #  ##   ##   # ## #   ##  ## # 
# ##  ##   ##      ## # ##  ##   ##   ## ##    ##  ## # 
# ## ###   ##      ##   ##  ##   ##   ##  ##   ##  ## # 
# ##  ##   ##  ##  ##   ##  ##   ##   ##  ##   ##  ## # 
####  ##  ### ###  ##   ##   ## ##   ###  ##  ### ##  #
#
#              Almond install script 
#        @andreas.lindell@almondmonitor.com
#
#!/usr/bin/env bash
set -euo pipefail

# Source distro info
source /etc/os-release

# Default to Ubuntu-style flow, flip when we detect RHEL/rocky
USE_APT=true

echo "Detected distro: $ID ($VERSION_ID)"
case "$ID" in
  rocky|rhel|centos)
    echo "Using yum/dnf for RPM-based distro"
    USE_APT=false
    ;;
  ubuntu|debian)
    echo "Using apt for Debian-based distro"
    ;;
  *)
    echo "Unknown distro '$ID'. Attempting apt install (may fail)"
    ;;
esac

echo "Installing Nagios plugins + prerequisites"
if $USE_APT; then
  sudo apt-get update
  sudo apt-get install -y \
    make autoconf automake libtool \
    nagios-plugins python3-psutil
else
  sudo yum install -y \
    make autoconf automake libtool \
    nagios-plugins-all python3-psutil
fi

echo "Preparing source tree"
aclocal
autoreconf -fi

echo "Configuring build"
./configure --prefix=/opt/almond

echo "Building and installing"
make
sudo make install

echo "Deploying config files"
for f in plugins.conf almond.conf memalloc.conf aliases.conf; do
  sudo cp "conf/$f" /etc/almond/
done

sudo cp start_almond.sh gardener.py memalloc.alm /opt/almond/

echo "Copying plugin scripts"
sudo mkdir -p /opt/almond/plugins
if $USE_APT; then
  sudo cp -r plugins/* /opt/almond/plugins/
else
  sudo cp plugins/*.{sh,py,pl} /opt/almond/plugins/ || true
  sudo ln -sf /usr/lib64/nagios/plugins/* /opt/almond/plugins/
fi

echo "Copying howru files"
sudo cp -rp www/* /opt/almond/

echo "Setting up runtime dirs"
sudo mkdir -p /var/log/almond /opt/almond/api_cmd

echo "Installation complete!"
