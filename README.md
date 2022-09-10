# dnote

## Introduction
Dnote is a notification utility for X based on dmenu. It is suitable for handling requests in quick succession and allows for the construction of a progress bar, making it ideal for volume indicators, etc.

Configuration is done by editing `config.h`.

## Installation
```bash
git clone https://github.com/kolunmi/dnote
cd dnote
make install
```

## Examples
```bash
dnoted &
echo message | dnote -minw 200
echo top right | dnote -loc 4
echo '75%' | dnote -pbar 3 4
```
