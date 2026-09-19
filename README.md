PCSC lite project
=================

Middleware to access a smart card using SCard API (PC/SC)
---------------------------------------------------------

To compile locally inside of a container use (`act` needs to be installed on your system):

```sh
$ act -b -W .github/workflows/build_meson.yml -j local-build
```

once done, built libs are inside of `dist/`. To install those use following on host machine:

```sh
# 1. Copy staged build files to host root and update library cache
sudo cp -av dist/* /
sudo ldconfig

# 2. Setup systemd unit files, system user, and reload daemon
sudo cp /usr/lib/systemd/user/pcscd.* /lib/systemd/system/
sudo systemd-sysusers
sudo systemctl daemon-reload

# 3. Enable and start the systemd socket
sudo systemctl reset-failed pcscd.socket
sudo systemctl enable --now pcscd.socket

# 4. Install standard CCID drivers and testing tools
sudo apt update
sudo apt install -y pcsc-tools libccid

# 5. Restart daemon and test smart card reader detection
sudo systemctl restart pcscd
pcsc_scan
```

and to check the installation:

```sh
pcscd --version
```

Uninstall:

```sh
# 1. Stop and disable active systemd units
sudo systemctl disable --now pcscd.service pcscd.socket

# 2. Remove systemd unit files & sysusers config
sudo rm -f /lib/systemd/system/pcscd.service /lib/systemd/system/pcscd.socket
sudo rm -f /usr/lib/systemd/user/pcscd.service /usr/lib/systemd/user/pcscd.socket
sudo rm -f /usr/lib/sysusers.d/pcscd-sysusers.conf
sudo systemctl daemon-reload

# 3. Remove binaries, headers, libraries, and configurations
sudo rm -f /usr/local/sbin/pcscd
sudo rm -f /usr/local/bin/pcsc-spy
sudo rm -rf /usr/local/include/PCSC
sudo rm -f /usr/local/lib/x86_64-linux-gnu/libpcsclite.so*
sudo rm -f /usr/local/lib/x86_64-linux-gnu/libpcsclite_real.so*
sudo rm -f /usr/local/lib/x86_64-linux-gnu/libpcscspy.so*
sudo rm -f /usr/local/lib/x86_64-linux-gnu/pkgconfig/libpcsclite.pc
sudo rm -f /usr/local/etc/default/pcscd
sudo rm -f /usr/local/share/metainfo/fr.apdu.pcsclite.metainfo.xml
sudo rm -f /usr/local/share/man/man5/reader.conf.5
sudo rm -f /usr/local/share/man/man1/pcsc-spy.1
sudo rm -f /usr/local/share/man/man8/pcscd.8
sudo rm -rf /usr/local/share/doc/pcsc-lite
sudo rm -f /usr/share/polkit-1/actions/org.debian.pcsc-lite.policy

# 4. Update system dynamic library cache
sudo ldconfig
```

The project Web site is: https://pcsclite.apdu.fr/

<a href="https://codeclimate.com/github/LudovicRousseau/PCSC"><img src="https://codeclimate.com/github/LudovicRousseau/PCSC/badges/gpa.svg" /></a>

Contributors
============

<a href="https://github.com/LudovicRousseau/PCSC/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=LudovicRousseau/PCSC" />
</a>

