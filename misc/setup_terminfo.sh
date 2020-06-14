#!/bin/bash
set -e
set -x
ssh $REMOTE mkdir -p .terminfo/r/


#for macos:
# hexadecimal representation is used instead of simply the first letter
#ref: https://unix.stackexchange.com/questions/410335/why-isnt-screen-on-macos-picking-up-my-terminfo
ssh $REMOTE ln -s .terminfo/r .terminfo/72

scp /lib/terminfo/r/rxvt-unicode-256color $REMOTE:.terminfo/r/
