# dnote

![screenshot](/screenshot.png "screenshot")

## Introduction
Dnote is a notification utility for X based on dmenu.
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
echo "$MESSAGE" | dnote -minw 200

# control the location and time until expiration
echo top right | dnote -loc 4 -exp 15

# construct a progress bar with fraction values
echo '75%' | dnote -pbar 3 4

# associate the notification with an id, so any existing matches will be overwritten
for i in {0..10}; do
	echo $i out of 10 | dnote -id abc -pbar $i 10
	sleep 0.1
done

# render a png to the notification
echo "$MESSAGE" | dnote -img './image.png' -img-inline
```
