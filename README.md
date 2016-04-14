# nct
NFS Client Tool

nct is a tool for testing NFS server response to repeated operations.  It may
be of most use to NFS server developers interested in seeing how patches
under test affect performance.

nct currently only supports NFS READ and GETATTR operations.  I expect it to
support more operations in the future, but currently I have taken a hiatus
from NFS work so it's not clear when that might come about.

# Examples

## NFS READ

Given an NFS server named "gw" exporting an 8GiB sparse file named
"/export/sparse-8192MB-0" you can instruct nct to create 16 threads
that will continually read sequentially through the file until the
test duration has elapsed.

    $ ./nct -m -t16 -d10 read gw:/export/sparse-8192MB-0
    
     SAMPLES  DURATION      OPS    TXMB    RXMB    LATENCY
          10   1005439     1799    0.23  112.59    8840.91
          20   1005198     1800    0.23  112.72    8877.85
          30   1000434     1791    0.23  112.16    8921.13
          40   1000616     1791    0.23  112.16    8920.50
          50   1000321     1791    0.23  112.16    8921.14
          60   1003670     1797    0.23  112.53    8891.37
          70   1004710     1798    0.23  112.59    8883.16
          80   1000611     1792    0.23  112.22    8918.55
          90   1003950     1797    0.23  112.53    8888.89
         100   1004822     1761    0.22  110.28    8881.17

In the above the the -m option tells nct to print a status mark once per
second, while the -d10 says run the test for a duration of 10 seconds.
The -t16 options tells nct to create 16 threads.  Each thread will read
sequentially through the file at the next offset (i.e., they all share
the same offset and hence do not all read the same blocks).

You can capture operational statistics at a fixed interval of 100ms
by giving the -o option.  The -o option specifies a directory into
which all the details stats should be stored.  Note that the -m option
perturbs the results, so generally you don't want to give it when
collecting detailed stats.  The result of the -o opton is that gnuplot
will be run on the collected data and generate a graph that can be
viewed to examine the response curve.

    $ ./nct -t16 -d10 -o ~/nct-read-1 read gw:/export/sparse-8192MB-0
    
             MIN          AVG          MAX  DESC
            1809         1818         1869  REQ/s (requests per second)
          238788       240023       246708  TX/s (bytes transmitted per second)
       118786176    119394584    122726016  RX/s (bytes received per second)
            8884         8923         9173  Latency (microseconds per request)
    
    $ ls -latr nct-read-1
    total 178
    drwxrwxrwt  27 root  wheel    155 Apr 14 05:20 ../
    -rw-r--r--   1 greg  wheel   7419 Apr 14 05:20 raw
    -rw-r--r--   1 greg  wheel    406 Apr 14 05:20 rcvd.gnuplot
    -rw-r--r--   1 greg  wheel  22148 Apr 14 05:20 rcvd.png
    -rw-r--r--   1 greg  wheel    404 Apr 14 05:20 sent.gnuplot
    -rw-r--r--   1 greg  wheel  22261 Apr 14 05:20 sent.png
    -rw-r--r--   1 greg  wheel    399 Apr 14 05:20 latency.gnuplot
    -rw-r--r--   1 greg  wheel  19385 Apr 14 05:20 latency.png
    -rw-r--r--   1 greg  wheel    405 Apr 14 05:20 requests.gnuplot
    drwxr-xr-x   2 greg  wheel     11 Apr 14 05:20 ./
    -rw-r--r--   1 greg  wheel  23600 Apr 14 05:20 requests.png


You can now point your browser at file:///usr/home/${USER}/nct-read-1/rcvd.png
to view the response curve.


## NFS GETATTR

Much like the NFS READ operation described above you, can issue NFS GETATTR
operations.  Currently, all threads just hammer away that the single
file/direcotry you specify on the nct command line.  However, a planned
enhancement is that if given a directory nct will issue GETATTR requests
iteratively to each file in the directory, re-cycling through the list
until the test duration has elapsed.

In the following example we run the test with only 1 thread for 60 seconds:

    $ ./nct -d60 -t1 -o ~/nct-getattr-1 getattr gw:/export/sparse-8192MB-0

In this case, you're probably more interested in latency and so you would want
to point your browser at file:///usr/home/${USER}/nct-getattr-1/latency.png.
