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
# start the daemon
dnoted &

# create a notification with a window at least 200 pixels wide
echo message | dnote -minw 200

# control the location and time until expiration
echo top right | dnote -loc 4 -exp 15

# construct a progress bar with fraction values
echo '75%' | dnote -pbar 3 4

# associate the notification with an id, so any existing matches will be overwritten
for i in {0..10}; do
	echo $i out of 10 | dnote -id abc -pbar $i 10
	sleep 0.1
done
```
