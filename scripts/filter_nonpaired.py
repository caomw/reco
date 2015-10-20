#!/usr/bin/python

import os
import os.path as osp
import argparse as ap
import numpy as np
import re

parser = ap.ArgumentParser(description='Delete all right frame files in folder based on which left frame files remain.')

parser.add_argument("-f", "--folder", help="Folder to work in", 
                    required=False, default= ["./"])
parser.add_argument("-s", "--save_frame_numbers", help="Save an array containing frame numbers of the remaining pairs.",
                    action="store_true",default = False)
if __name__ == "__main__":
    args = parser.parse_args()
    files = [f for f in os.listdir(args.folder) if osp.isfile(osp.join(args.folder,f)) and f.endswith(".png")]
    files.sort()
    
    rfiles = [f for f in files if "r_" in f]
    lfiles = [f for f in files if "l_" in f]
    num_pattern = re.compile("\d{4}")
    rnums = [int(re.findall(num_pattern,f)[0]) for f in rfiles]
    lnums = [int(re.findall(num_pattern,f)[0]) for f in lfiles]
    rdict = {}
    ldict = {}
    
    for ix_right in xrange(len(rnums)):
        rfile = rfiles[ix_right]
        rnum = rnums[ix_right]
        rdict[rnum] = rfile
    for ix_left in xrange(len(lnums)):
        lfile = lfiles[ix_left]
        lnum = lnums[ix_left]
        ldict[lnum] = lfile
        
    count_r_removed = 0
    count_l_removed = 0
    #filter right based on left
    for rnum, rfile in rdict.iteritems():
        if(not rnum in ldict):
            os.remove(osp.join(args.folder,rfile))
            count_r_removed += 1
    #filter left based on right
    for lnum, lfile in ldict.iteritems():
        if(not lnum in rdict):
            os.remove(osp.join(args.folder,lfile))
            count_l_removed += 1

    print "removed {0:d} files ({1:d} left and {2:d} right)"\
        .format(count_l_removed + count_r_removed, count_l_removed, count_r_removed)
    
    if(args.save_frame_numbers):
        rnums_arr = np.array(rnums)
        np.savez_compressed(osp.join(args.folder,"frame_numbers.npz"),frame_numbers=rnums_arr)