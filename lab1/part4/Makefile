#INSTRUCTIONS:
#1.Type "make" into the command line to compile your code
#2.Type "make test-short" to run the binary with "path_short.txt"
#3.Type "make test-very-long" to run the binary with "path_very_long.txt"
#NOTE : Make sure to always run "make" after editing your ".c" file
CC = gcc
CLIENT = "multi_service_server"
SERVER = "localhost"
PORT = "2094"

all : server test

server : multi_service_server.*.c
	$(CC) multi_service_server.*.c -g -Wall -o $(CLIENT) 

test : server
	./$(CLIENT) $(PORT) $(PORT)

#NOTE : This command will write the output to "out1.txt"
test-short :
	./$(CLIENT) $(SERVER) $(PORT) /ece463/lab1/path_short.txt > out1.txt
	diff out1.txt test_short.txt 

#NOTE : This command will write the output to "out2.txt"
test-very-long :
	./$(CLIENT) $(SERVER) $(PORT) /ece463/lab1/path_very_long.txt > out2.txt
	diff out2.txt test_very_long.txt 

mem-test: server
	valgrind --leak-check=full --show-leak-kinds=all ./$(CLIENT) $(PORT) $(PORT) 2>vlog.txt
