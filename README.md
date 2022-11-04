# Dnote

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

## Overview
This project is split into three binaries:

* `dnoted` serves as the central daemon
* `dnote` sends data about new notifications
* `dnotec` sends commands or queries data

### Client Usage

#### dnote
Text is read through standard input. Multiple lines are supported.

Window locations are specified numerically and by the following pattern on a monitor:
```
|---+---+---|
| 6 | 5 | 4 |
|---+---+---|
| 7 | 0 | 3 |
|---+---+---|
| 8 | 1 | 2 |
|---+---+---|
```

| Option                 | Description                                           |
|------------------------|-------------------------------------------------------|
| `-id [string]`         | asscociate message with id                            |
| `-exp [seconds]`       | time until expiration; '0' will never expire          |
| `-minw [pixels]`       | minimum window width                                  |
| `-center`              | center text                                           |
| `-no-center`           | don't center text                                     |
| `-loc [0-8]`           | window location                                       |
| `-ploc [x] [y]`        | specify precise location, relative to geometry origin |
| `-pbar [val] [out of]` | construct a progress bar                              |
| `-cmd [command]`       | run shell command when window is selected             |
| `-img [filepath]`      | render png to window                                  |
| `-img-header`          | position png at top of window                         |
| `-img-inline`          | position png next to text                             |
| `-imut`                | remove ability to kill notification by clicking       |
| `-v`                   | print version info                                    |

#### dnotec

| Option             | Description                             |
|--------------------|-----------------------------------------|
| `-list`            | list active non-anonymous notifications |
| `-kill [ID]`       | kill notification with id               |
| `-clear`           | kill all active notifications           |
| `-renew [ID]`      | renew notification with id              |
| `-select [ID]`     | select notification with id             |
| `-img-list`        | list pngs contained in memory           |
| `-img-load [PATH]` | load or reload a png into memory        |
| `-v`               | print version info                      |

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

# associate the notification with an id, so any existing matches and their settings will be overwritten
for i in $(seq 0 10); do
	echo $i out of 10 | dnote -id abc -pbar $i 10
	sleep 0.1
done

# control exactly when a notification disappears
ID='file copy'
echo copying files | dnote -id "$ID" -exp 0 -imut
cp "$SRC" "$DEST"
dnotec -kill "$ID"

# have the server run a shell command when the notification is selected
ID=volume
echo "Current Volume: $(pamixer --get-volume)" | dnote -id "$ID" -cmd 'pavucontrol'
# left click, or:
dnotec -select "$ID"

# render a png to the notification
PNG_PATH='./image.png'
echo "$MESSAGE" | dnote -img "$PNG_PATH" -img-inline
# reload the png file after changes
dnotec -img-load "$PNG_PATH"
```
