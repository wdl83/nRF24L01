all: nRF24L01_tx_test.Makefile nRF24L01_rx_test.Makefile
	make -f nRF24L01_tx_test.Makefile
	make -f nRF24L01_rx_test.Makefile

clean: nRF24L01_tx_test.Makefile nRF24L01_rx_test.Makefile
	make -f nRF24L01_tx_test.Makefile clean
	make -f nRF24L01_rx_test.Makefile clean
