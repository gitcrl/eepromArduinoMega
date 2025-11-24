
# adaptado do codigo: Chris Baird,, <cjb@brushtail.apana.org.au> threw

import sys
import serial
import time

RECSIZE = 128

ser = serial.Serial('/dev/ttyACM0', 500000, timeout=2.5)
sys.argv.pop(0)
dumpstart = -1
dumpend = -1
s = sys.argv[0]
time.sleep(1)                   # weirdness it started needing -cjb


def calcwriteline(a, l):
    print (a)
    ck = 0
    s = "W" + ("%04x" % a) + ":"
    for c in l:
        s = s + ("%02x" % c)
        ck = ck ^ c
    s = s + ("ff"*RECSIZE)
    s = s[:6+RECSIZE*2]
    if (len(l) & 1):
        ck = ck ^ 255
    ck = ck & 255
    s = s + "," + ("%02x" % ck)
    print ("***",s)
    return s.upper()


def waitokay():
    bad = 0
    while True:
        s = ser.readline().decode()
        print ("wait ok ",s)
        if s == "OK\r\n":
            break
        else:
            bad = bad + 1
        if bad > 50:
            sys.exit("errh")

if s == "-r":
    dumpstart = int(sys.argv[1])
    dumpend = int(sys.argv[2])
    print (dumpstart, dumpend)
    if dumpstart > -1:
        while (dumpstart <= dumpend):
            addr = "%04x" % dumpstart
            s = "R" + addr + chr(10)
            print ("sending - ",s)
            ser.write(s)
            l = ser.readline()
            print (l.upper())
            waitokay()
            dumpstart = dumpstart + RECSIZE

if s == "-f":
    fl = open("readfix.bin", 'w')
    dumpstart = int(sys.argv[1])
    dumpend = int(sys.argv[2])
    print (dumpstart, dumpend)
    if dumpstart > -1:
        while (dumpstart <= dumpend):
            addr = "%04x" % dumpstart
            s = "R" + addr + chr(10)
            print ("sending - ",s)
            ser.write(s)
            l = ser.readline()
            o = l.upper()
            content = o[5:-5]
            for i in range(0,32,2):
                by = content[i:i+2]
                print (str(i/2),)
                fl.write(chr(int(by,16)))
            waitokay()
            dumpstart = dumpstart + RECSIZE
    fl.close()



if s == "-R":
    dumpstart = int(sys.argv[1])
    dumpend = int(sys.argv[2])
    if dumpstart > -1:
        while (dumpstart <= dumpend):
            addr = "%04x" % dumpstart
            s = "R" + addr + chr(10)
            ser.write(s)
            l = ser.readline()
            o = l.upper()
            content = o[5:-5]
            for i in range(0,64,2):
                by = content[i:i+2]
                print (by,)
            print()
            waitokay()
            dumpstart = dumpstart + RECSIZE


if s == "-b":
    dumpstart = int(sys.argv[1])
    dumpend = int(sys.argv[2])
    if dumpstart > -1:
        while (dumpstart <= dumpend):
            addr = "%04x" % dumpstart
            s = "R" + addr + chr(10)
            ser.write(s)
            l = ser.readline()
            o = l.upper()
            content = o[5:-5]
            for i in range(0,64,2):
                by = content[i:i+2]
                sys.stdout.write(chr(int(by, 16))),
            waitokay()
            dumpstart = dumpstart + RECSIZE


if s == "-s":
    s = "H7" + chr(10)
    ser.write(s.encode())
    print (ser.readline())
    f = open(sys.argv[1], 'rb')
    a = 0
    while True:
        l = f.read(RECSIZE)
        if len(l) == 0:
            break
        s = calcwriteline(a, l)
        print (s)
        sys.stdout.flush()
        ser.write((s + chr(10)).encode())
        waitokay()
        sys.stdout.flush()
        if len(l) != RECSIZE:
            break
        a = a + RECSIZE
    f.close()


if s == "-v":
    f = open(sys.argv[1], 'rb')
    s = "H7" + chr(10)
    ser.write(s.encode())
    print (ser.readline())
    a = 0
    badcount = 0
    while True:
        s = "R" + ("%04x" % a) + chr(10)
        ser.write(s.encode())
        l = ser.readline()
        l = l.upper()
        waitokay()

        romt = "ROM  %04x:" % a

        rom = [None]*RECSIZE
        for p in range(RECSIZE):
            i = 5 + (p*2)
            c = int(l[i:i+2], 16)
            romt = romt + str(" %02x" % c)
            rom[p] = c
        #print romt, "\r",
        sys.stdout.flush()

        r = f.read(RECSIZE)
        if len(r) == 0:
            break
        okay = 1
        filet = "FILE %04x:" % a
        for i in range(len(r)):
            filet = filet + " %02x" % r[i]
            if rom[i] != r[i]:
                okay = 0
                badcount = badcount + 1

        if okay == 0:
            print (romt, "\r",)
            print (filet)
            print ("MISMATCH!!")
            print ()
            sys.stdout.flush()

        if len(r) != RECSIZE:
            break
        else:
            a = a + RECSIZE

    print()
    print (badcount, "errors!")
    sys.stdout.flush()
    f.close()


if s == "-S":
    f = open(sys.argv[1], 'rb')
    a = 0
    while True:
        s = "R" + ("%04x" % a) + chr(10)
        ser.write(s)
        l = ser.readline()
        l = l.upper()
        print ("serin:"+l)
        waitokay()

        romt = "ROM  %04x:" % a
        rom = [None]*RECSIZE
        for p in range(RECSIZE):
            i = 5 + (p*2)
            c = int(l[i:i+2], 16)
            romt = romt + str(" %02x" % c)
            rom[p] = c

        fileReaded = f.read(RECSIZE)
        sizeReaded = len(fileReaded)
        if sizeReaded == 0:
            break
        print (romt)
        sys.stdout.flush()
        okay = 1
        filet = "FILE %04x:" % a
        for i in range(sizeReaded):
            if rom[i] != ord(fileReaded[i]):
                okay = 0
                filet = filet + " %02x" % ord(fileReaded[i])
            else:
                filet = filet + "   "

        if okay == 0:
            s = calcwriteline(a, fileReaded)
            print ()
            print (filet, "UPDATING")
            sys.stdout.flush()
            ser.write(s+chr(10))
            waitokay()
        else:
            print (" OKAY")
        sys.stdout.flush()

        if len(fileReaded) != RECSIZE:
            break
        else:
            a = a + RECSIZE

    f.close()
