### Versions

* smart card reader driver name and version
* pcsc-lite version
* the output of the command `/usr/sbin/pcscd --version`

### Platform

* Operating system or GNU/Linux distribution name and version
* Smart card middleware name and version
* Smart card reader manufacturer name and reader model name
* Smart card name

### Issue

* What do you do?
* What result do you expect?
* What result do you get instead?

### Log

Then you shall generate a complete log (do not truncate it).

* If you need to enter the smart card PIN to reproduce the problem then
  consider changing your PIN before generating the logs as the PIN value
  will be included in the logs.
* Kill any running pcscd process
* (re)start pcscd exactly as described bellow:
```
sudo LIBCCID_ifdLogLevel=0x000F pcscd --foreground --debug --apdu --color | tee log.txt
```
* Stop pcscd (using Control-C) after the problem occurred and send me the
  generated log.txt file

See also https://pcsclite.apdu.fr/#support
