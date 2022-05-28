install:
	git clone https://github.com/calccrypto/uint128_t.git
test:
	g++ -g -Wno-everything test.cpp -o ./main