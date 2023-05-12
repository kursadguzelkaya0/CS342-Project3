all: librm.a myapp app
librm.a: rm.c
	gcc -Wall -c rm.c
	ar -cvq librm.a rm.o
	ranlib librm.a
myapp: myapp.c
	gcc -Wall -o myapp myapp.c -L. -lrm -lpthread
app: app.c
	gcc -Wall -o app app.c -L. -lrm -lpthread
clean:
	rm -fr *.o *.a *~ a.out myapp app rm.o rm.a librm.a