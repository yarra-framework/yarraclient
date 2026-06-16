# Building
## Debian-based Linux

Get QT5 development files
```
apt install qtbase5-dev-tools qtbase5-dev libqt5svg5-dev qt5-qmake
```

Use `qmake` and `make` to build

```
qmake -o SAC-build/Makefile StandaloneClient/SAC.pro 
make -C SAC-build

# run 
SAC-build/SAC
```


> [!NOTE]
> Tested 2025-06-15 on debian 12 "bookworm" with QMake version 3.1 Using Qt version 5.15.8

