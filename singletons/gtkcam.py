#!/usr/bin/env python

import sys
import serial
import time
import pygtk
import gtk
import gobject

def print_hex(hex_string):
        print "[",
        for c in hex_string:
            print "0x%02x" % ord(c),
        print "]",
        return
                 
def verify_msg(data,ser):
        ser_data = ser.read(3)
        if (ser_data != data):
            print_hex(ser_data)
            print "does not match",
            print_hex(data)
            sys.exit(1)
        ser_data = ser_data + str(ser.read(3)) 
        return ser_data

class ImagesExample:

    def close_application(self, widget, event, data=None):
        gtk.main_quit()
        return False

    def __init__(self):
        self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
        self.window.connect("delete_event", self.close_application)
        self.window.show()
        self.gimage = gtk.Image()
        self.gimage.show()
        self.window.add(self.gimage)

	# get the port/baudrate from the command line
	
	port = "/dev/ttyUSB0"
	baudrate = 115200
	self.filename = "more.jpg"
	# connect to the serial port
	self.ser = serial.Serial()
	self.ser.port = port
	self.ser.baudrate = baudrate
	print self.ser.port, self.ser.baudrate
	self.ser.timeout = 10  # 10 second timeout
	
	try:
	    self.ser.open()
	except serial.SerialException, e:
	    sys.stderr.write("Could not open serial port %s: %s\n" % (self.ser.portstr, e))
	    sys.exit(1)
	msg_sync = "\xAA\x0D\x00\x00\x00\x00"
	self.ser.flushInput()
	self.ser.flushOutput()
	
	
	while not self.ser.inWaiting() == 12:
	    print "sync"
	    self.ser.write(msg_sync); time.sleep(.1)
	# verify ack and sync
	ack = verify_msg("\xAA\x0E\x0D",self.ser)
	sync = verify_msg("\xAA\x0D\x00",self.ser)
	
	self.ser.write(ack)
	time.sleep(.01)
	
	
	#print "Waiting 2 seconds"; time.sleep(2)
	print "Initiating setup"
	
	#Initial JPEG preview, VGA
	self.ser.write("\xAA\x01\x00\x07\x00\x07");
	ack = verify_msg("\xAA\x0E\x01",self.ser)
	# Set Package Size (512 bytes)
	self.ser.write("\xAA\x06\x08\x00\x02\x00")
	ack = verify_msg("\xAA\x0E\x06",self.ser)
	
	self.ser.write("\xAA\x07\x01\x01\x00\x00")
	ack = verify_msg("\xAA\x0E\x07",self.ser)
	
	self.ser.baudrate = 921600;
        gobject.timeout_add(100,self.updater)
	
    def updater(self):
        # Snapshot
        self.ser.write("\xAA\x05\x00\x00\x00\x00")
        ack = verify_msg("\xAA\x0E\x05", self.ser)
        print "Requesting Picture"
        self.gimage.set_from_file(self.filename)

        # Get Picture
        self.ser.write("\xAA\x04\x01\x00\x00\x00")

        time.sleep(.01)

        ack=verify_msg("\xAA\x0E\x04", self.ser)
        print ord(ack[0]), ord(ack[1]), ord(ack[2])
        data=verify_msg("\xAA\x0A\x01", self.ser)
	
        size = data[3:6][::-1]  # last three bytes are the size
        size_dec = int(size.encode("hex"), 16)
        print "Requesting " + str(size_dec) + " Bytes"
        # [ ID (2) ][ Data Size (2)][ Image Size (506) ][ Checksum (2) ]
        got = 0
        image = ''
        i = 0
        errors = 0
	
        while (i < 65536):
	        if (got >= size_dec): break
	        lbyte = i >> 8
	        hbyte = i & 0xff
	        data = ''  # byte string for checksum
	        request = "\xAA\x0E\x00\x00" + chr(hbyte) + chr(lbyte)
	        self.ser.write(request)
                
	        # print_hex(request)
                
	        # read the first 4 bytes which has the ID and payload size
	        header=self.ser.read(4)
	        data = data + header
	        id_hex = header[0:2][::-1]
	        id_dec = int(id_hex.encode("hex"),16)
	        psize_hex = header[2:4][::-1]
	        psize_dec = int(psize_hex.encode("hex"),16)
	        # read the payload data
	        payload = self.ser.read(psize_dec)
	        data = data + payload
	        image = image + payload
	        got = got + psize_dec
	        # read the checksum
	        csdata = self.ser.read(2)
	        cs_hex = csdata[0:1].encode("hex")
	        cs_dec = int(cs_hex,16)
	
	        # calculate the checksum
	        cs_calc = 0
	        for c in data:
	            cs_calc = cs_calc + int(c.encode("hex"), 16)
	            cs_calc = cs_calc & 0xff
                '''	
	        print "ID = " + str(id_dec) + " Size = " + str(psize_dec),
	        print  "CS = 0x" + str(cs_hex),
	        print  "CS_CALC = " + hex(cs_calc),
	        print  " | " + str(got) + " bytes read"
                '''
	        if (cs_dec != cs_calc):
	            print "Checksum failure! retrying"
	            errors = errors + 1
	        else:
	            i=i+1
	
        if not (got == size_dec):
	        print "ERROR: invalid payload"
	        sys.exit(1)
	
        print "Done requesting data"
        self.ser.write("\xAA\x0E\x00\x00\xF0\xF0")
	
        FILE =  open(self.filename, 'wb')
        FILE.write(image)
        FILE.close
            
        gobject.timeout_add(50,self.updater)

def main():
    gtk.main()
    return 0

if __name__ == "__main__":
    ImagesExample()
    main()
