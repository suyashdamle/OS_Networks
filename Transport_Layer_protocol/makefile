sender.out: protocol_stack.h.gch receiver.out
	g++ -g sender.cpp -o sender.out -lpthread
protocol_stack.h.gch: protocol_stack.h
	g++ protocol_stack.h

receiver.out:
	g++ -g receiver.cpp  -o receiver.out -lpthread

clean:
	rm protocol_stack.h.gch sender.out receiver.out
