#
CPP = gcc
P = -Wall -lcgi -lmysqlclient -ldl -O2
OBJECT = logfile.o report.o utils.o
LDFLAGS=-L/usr/lib64/mysql -L/usr/lib64/

ALL: report main

report: $(OBJECT)
        $(CPP) $(OBJECT) $(P) -o report.cgi $(LDFLAGS)

main: haarp.c 
	$(CPP) haarp.c $(P) -o haarp.cgi $(LDFLAGS)

%.o: %.c %.h
	$(CPP) $(P) -c -o $@ $<

clean:
      	rm -f *.cgi *.o
