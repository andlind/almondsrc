#!/usr/bin/python3
import sys
import psutil
from optparse import OptionParser

def parse_options():
    parser = OptionParser()

    parser.add_option('-w', '--warning', type="int", dest="warning",
                     help="Warning percent free")
    parser.add_option('-c', '--critical', type="int", dest="critical",
                     help="Critical percent free")
    options, args = parser.parse_args()

    if (not options.warning or not options.critical):
        parser.error("use --help for help ")

    return options

def main():
    options = parse_options()

    mem = psutil.virtual_memory()
    p_free = 100 - mem.percent

    if p_free <= options.critical:
        msg, r = ("CRITICAL ", 2)
    elif p_free <= options.warning:
        msg, r = ("WARNING ", 1)
    else:
        msg, r = ("OK ", 0)

    #print ("%s (Percent free: %s%%, Total: %s MB, Used: %s MB, Buffers: %s MB, Cached: %s MB) | free=%s total=%s used=% buffers=%s cached=%ss" % (msg, p_free, (mem.total / 1024 / 1024), (mem.used / 1024 / 1024),
                                                                                               #(mem.buffers / 1024 / 1024), (mem.cached / 1024 / 1024), p_free, (mem.total /1024 / 1024),
                                                                                               #(mem.used / 1024 / 1024),(mem.buffers / 1024 / 1024), (mem.cached / 1024 / 1024))) 
    print ("%s (Percent free: %s%%, Total: %s MB, Used: %s MB, Buffers: %s MB, Cached: %s MB) | free=%s total=%s used=%s buffers=%s cached=%s" % (msg, p_free, (mem.total / 1024 / 1024), (mem.used / 1024 / 1024), (mem.buffers / 1024 / 1024), (mem.cached / 1024 / 1024), p_free, (mem.total /1024 / 1024), (mem.used / 1024 / 1024), (mem.buffers / 1024 / 1024), (mem.cached / 1024 / 1024)))

    sys.exit(r)

if __name__ == '__main__':
    main()
