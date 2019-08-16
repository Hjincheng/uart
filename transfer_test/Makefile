#!/bin/bash

TTY1=/dev/ttysWK2
TTY2=/dev/ttysWK3
TEST_FILE1=sent_file/test_file
TEST_FILE2=sent_file/test_file
BAUD=115200

init:
	stty -F $(TTY1) ospeed $(BAUD) ispeed $(BAUD) cs8 raw
	stty -F $(TTY2) ospeed $(BAUD) ispeed $(BAUD) cs8 raw
	echo "init /dev/ttysWK3 baud: $(BAUD)"
	echo "init /dev/ttysWK2 baud: $(BAUD)"

send:
	cat $(TEST_FILE1) > $(TTY1) &
	cat $(TEST_FILE2) > $(TTY2) &
	echo "cat $(TEST_FILE) > $(TTY1) &"
	echo "cat $(TEST_FILE) > $(TTY2) &"

receive:
	while :
	do
		echo "1234567" > $TTY
	done

