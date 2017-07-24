#!/usr/bin/env python

"""read_mouse_delta.py: 

"""
from __future__ import print_function
    
__author__           = "Dilawar Singh"
__copyright__        = "Copyright 2017-, Dilawar Singh"
__version__          = "1.0.0"
__maintainer__       = "Dilawar Singh"
__email__            = "dilawars@ncbs.res.in"
__status__           = "Development"

import sys
import io
import struct
import os
import time
import math
import threading
import Queue
import usb.core 

# Find the gaming mouse.
mouse_ = usb.core.find( idVendor = 0x046D, idProduct = 0xc24e )
assert mouse_ is not None, "Could not find gaming mouse"

mouse_.set_configuration( )
if mouse_.is_kernel_driver_active(0):
    mouse_.detach_kernel_driver(0)

user_ = os.environ.get( 'USER', ' ' )

user_interrupt_ = False
cur_, prev_ = (0,0,0), (0,0,0)

def getMouseEvent( mouseF, q ):
    global user_interrupt_
    while True:
        if user_interrupt_:
            break 
        buf = mouseF.read( 0x81, 3, 1 );
        x,y = struct.unpack( "bb", buf[1:] );
        t = time.time( )
        q.put( (t, x, y) )

def printMouse( q ):
    global user_interrupt_
    global prev_, cur_
    while True:
        if user_interrupt_:
            break 
        if not q.empty( ):
            val = q.get( )
            prev_, cur_ = cur_, val 
            dx, dy = (cur_[1] - prev_[1]),  (cur_[2]-prev_[2]) 
	    dist = ( dx ** 2.0 + dy ** 2.0 ) ** 0.5
            v = dist / (cur_[0] - prev_[0])
	    theta = math.atan( dx / max(1e-6,dy) )
            print( 'velocity = % 8.3f, % 8.3f' % (v, theta) )


def main( ):
    global mouse_
    global user_interrupt_
    q = Queue.Queue( )
    readT = threading.Thread( name = 'get_mouse', target=getMouseEvent, args=(mouse_,q))
    writeT = threading.Thread( name = 'print_mouse', target=printMouse, args=(q,) )
    readT.daemon = True
    writeT.daemon = True
    readT.start( )
    writeT.start( )

    # Main thread, just to catch interrupts.
    try:
        while 1:
            time.sleep( 0.1 )
    except KeyboardInterrupt as e:
        user_interrupt_ = True 
    except Exception as e:
        user_interrupt_ = True
    mouse_.close( )
    print( '> All done' )

if __name__ == '__main__':
    main()

