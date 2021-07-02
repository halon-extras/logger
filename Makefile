all: logger

logger:
	g++ -I/opt/halon/include/ -I/usr/local/include/ -fPIC -shared logger.cpp -o logger.so
