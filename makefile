all: cook waiter customer

cook: cook.c ipc_shared.h
	gcc -Wall -o cook cook.c

waiter: waiter.c ipc_shared.h
	gcc -Wall -o waiter waiter.c

customer: customer.c ipc_shared.h
	gcc -Wall -o customer customer.c

db:
	gcc -Wall -o gencustomers gencustomers.c
	./gencustomers > customers.txt

clean:
	-rm -f cook waiter customer gencustomers a.out