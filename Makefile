
main:main.c
	gcc -o main main.c -lpthread -DDEBUG
	sudo ./main /dev/ttysWK3 /dev/ttysWK2


mainPc:mainPc.c
	gcc -o mainPc mainPc.c -lpthread -DDEBUG
	#sudo ./mainPc /dev/ttyUSB0 /dev/ttyUSB2
	sudo ./mainPc /dev/ttysWK2 /dev/ttysWK3

check:
	md5sum DST_FILE savefile0  savefile1

