CC = g++

all: sr gbn

sr: SenderSR ReceiverSR

gbn: SenderGBN ReceiverGBN

SenderSR: SenderSR.cpp
	$(CC) SenderSR.cpp -pthread -o SenderSR

ReceiverSR: ReceiverSR.cpp
	$(CC) ReceiverSR.cpp -pthread -o ReceiverSR

SenderGBN: SenderGBN.cpp
	$(CC) SenderGBN.cpp -pthread -o SenderGBN

ReceiverGBN: ReceiverGBN.cpp
	$(CC) ReceiverGBN.cpp -o ReceiverGBN

.PHONY: clean

clean:
	rm -f SenderSR ReceiverSR SenderGBN ReceiverGBN