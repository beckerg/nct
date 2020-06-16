# nct
NFS Client Tool

*nct* is a tool for testing NFS server response to repeated operations.  It may
be of most use to NFS server developers interested in seeing how patches
under test affect performance.

*nct* currently only supports NFS **NULL**, **READ**, and **GETATTR** operations.
Each *nct* instance opens just one connection to the server, but may employ one
or more threads and one or more requests in flight.

# Examples

## NFS READ

Given an NFS server exporting an 8GiB sparse file named
*"/export/sparse-8192MB-0"* you can instruct *nct* to read
sequentially through the file until the test duration
has elapsed.

The following tests were run with two directly connected servers
(each with one 12-core E5-2690 v3) with **HT** disabled.

$ uname -a
FreeBSD sm3.cc.codeconcepts.com 12.1-STABLE FreeBSD 12.1-STABLE r362012 SM1  amd64

### Chelsio T62100-SO-CR with TOE enabled.

Single threaded, at most one read request in flight:
    
$ sudo ./nct -m1 -d10 -j1 read 10.100.0.1:/export/sparse-8192MB-0 131072

```
 SAMPLES  DURATION      OPS    TXMB    RXMB  LATMIN  LATAVG  LATMAX
      10   1004902    11568    1.46 1447.41    76.0    86.4    79.9
      20    999954    12825    1.61 1604.69    76.5    77.9    88.0
      30   1000960    12939    1.63 1618.95    75.6    77.2    79.9
      40   1000033    12950    1.63 1620.33    75.7    77.1    79.9
      50    994466    12817    1.61 1603.69    76.4    77.9    80.0
      60   1004545    12975    1.63 1623.33    76.0    77.0    79.2
      70   1000025    12893    1.62 1613.20    76.0    77.5    79.4
      80   1000293    12945    1.63 1619.71    75.9    77.2    79.6
      90   1000007    12944    1.63 1619.58    76.0    77.2    79.6
     100    999632    12856    1.62 1608.57    76.1    77.2    79.7
```

Single threaded, at most two read requests in flight:

$ sudo ./nct -m1 -d10 -j2 read 10.100.0.1:/export/sparse-8192MB-0 131072

```
 SAMPLES  DURATION      OPS    TXMB    RXMB  LATMIN  LATAVG  LATMAX
      10   1006125    19798    2.49 2477.17    83.8   101.0    89.8
      20    999482    22901    2.88 2865.42    84.4    87.3    89.8
      30    999984    22946    2.89 2870.93    43.9    87.1   119.7
      40    996521    22782    2.87 2850.53    84.3    87.7    90.0
      50   1003473    23036    2.90 2882.31    84.1    86.7    89.9
      60    999347    22395    2.82 2801.98    87.0    89.2    92.0
      70   1000893    22381    2.82 2800.36    86.9    89.3    91.8
      80    999807    22362    2.82 2797.98    87.1    89.4    91.9
      90   1000065    22370    2.82 2798.86    87.4    89.3    91.8
     100   1000013    22126    2.79 2768.45    77.9    89.7    92.3
```

### Chelsio T62100-SO-CR with TOE disabled.

Single threaded, at most two read requests in flight:

$ sudo ./nct -m1 -d10 -j1 read 10.100.1.1:/export/sparse-8192MB-0 131072

```
 SAMPLES  DURATION      OPS    TXMB    RXMB  LATMIN  LATAVG  LATMAX
      10   1005531    12778    1.61 1598.81   111.0   156.5   194.2
      20    999670    14279    1.80 1786.62   132.1   140.0   173.1
      30   1000001    14268    1.80 1785.24   133.3   140.1   173.5
      40   1000096    13952    1.76 1745.70   112.4   143.3   199.8
      50   1000059    13862    1.75 1734.32   110.4   144.2   195.5
      60   1000285    13936    1.75 1743.70   134.7   143.4   177.1
      70    998641    13904    1.75 1739.70   129.8   143.8   196.2
      80   1000949    13944    1.76 1744.70   135.9   143.4   178.2
      90    999958    13892    1.75 1738.20   129.2   143.9   195.1
     100    999942    13786    1.74 1724.81    95.5   144.0   181.9
```

Single threaded, at most three read requests in flight:

$ sudo ./nct -m1 -d10 -j3 read 10.100.1.1:/export/sparse-8192MB-0 131072

```
 SAMPLES  DURATION      OPS    TXMB    RXMB  LATMIN  LATAVG  LATMAX
      10   1005702    15557    1.96 1946.52   175.4   192.9   196.2
      20   1000126    18083    2.28 2262.58   137.0   165.8   197.9
      30    999775    18419    2.32 2304.62   153.7   162.8   173.4
      40   1000077    18435    2.32 2306.50   153.6   162.7   173.9
      50    999439    18438    2.32 2307.00   152.2   162.6   183.4
      60    999848    18443    2.32 2307.63   153.1   162.6   176.1
      70   1000004    18443    2.32 2307.63   152.1   162.6   176.1
      80    999926    18426    2.32 2305.37   150.2   162.7   174.4
      90   1000979    18450    2.32 2308.50   153.3   162.5   173.8
     100    999925    18296    2.30 2289.23    95.7   162.6   177.8
```

### Intel(R) PRO/10GbE X540-AT2

Single threaded, at most one read request in flight:

$ sudo ./nct -m1 -d10 -j1 read 10.10.0.1:/export/sparse-8192MB-0 131072

```
 SAMPLES  DURATION      OPS    TXMB    RXMB  LATMIN  LATAVG  LATMAX
      10   1005799     4837    0.61  605.22   195.2   206.6   246.7
      20    995149     5141    0.65  643.25   182.1   194.4   238.9
      30   1004055     5233    0.66  654.76   182.1   191.0   221.6
      40   1000876     5184    0.65  648.63   183.6   192.8   213.4
      50    999193     5162    0.65  645.88   184.2   193.6   259.9
      60    999763     5167    0.65  646.51   184.0   193.5   239.7
      70   1001301     5179    0.65  648.01   182.8   193.0   231.1
      80    998956     5170    0.65  646.88   184.4   193.3   239.1
      90   1000677     5182    0.65  648.38   183.0   192.9   223.1
     100    999239     5145    0.65  643.75   181.8   193.2   236.5
```

Single threaded, at most two read requests in flight:

$ sudo ./nct -m1 -d10 -j2 read 10.10.0.1:/export/sparse-8192MB-0 131072

```
 SAMPLES  DURATION      OPS    TXMB    RXMB  LATMIN  LATAVG  LATMAX
      10   1005751     8912    1.12 1115.09   198.7   224.4   231.6
      20    999963     8940    1.13 1118.59   190.2   223.7   327.4
      30   1000103     8933    1.12 1117.72   184.5   223.8   372.3
      40    999623     8935    1.12 1117.97   190.1   223.8   316.6
      50   1000123     8942    1.13 1118.84   185.0   223.6   310.7
      60   1000090     8940    1.13 1118.59   185.0   223.6   353.5
      70    994186     8891    1.12 1112.46   188.7   224.9   363.1
      80   1005693     8987    1.13 1124.35   188.3   222.4   336.7
      90    994202     8887    1.12 1111.96   185.9   225.0   341.6
     100   1005688     8927    1.12 1116.96   183.6   222.4   313.8
```

In the above example the **-m** option tells *nct* to print a status mark
once per second, while **-d10** says run the test for a duration of 10 seconds.
The **-j** option tells *nct* to how many requests it can have in flight at any
given moment.

*nct* can capture operational statistics at a fixed interval of 100ms
by giving the **-o** option.  The **-o** option specifies a directory into
which all the raw stats should be stored.  Note that the **-m** option
perturbs the results, so generally you don't want to give it when
collecting detailed stats.  The result of the **-o** option is that
gnuplot will be run on the collected data and generate a graph that
can be viewed to examine the response curve.

    $ ./nct -j1 -d300 -o ~/getattr getattr 10.100.0.1:/export/sparse-8192MB-0

```
         MIN          AVG          MAX           TOTAL  DESC
     6384760      7814038      8953792      2344956064  bytes transmitted per second
     5766880      7057840      8087296      2118024832  bytes received per second
        15.0         15.6         19.0       294483066  latency per request (usecs)
       51490        63016        72208        18910936  requests per second


    $ ls -latr ~/getattr 
    total 126
    drwxr-xr-x  26 greg  greg     50 Jun  3 13:50 ../
    -rw-r--r--   1 greg  greg  62566 Jun  3 13:52 raw
    -rw-r--r--   1 greg  greg    448 Jun  3 13:52 recv.gnuplot
    -rw-r--r--   1 greg  greg  29583 Jun  3 13:52 recv.png
    -rw-r--r--   1 greg  greg    445 Jun  3 13:52 send.gnuplot
    -rw-r--r--   1 greg  greg  23644 Jun  3 13:52 send.png
    -rw-r--r--   1 greg  greg    440 Jun  3 13:52 latency.gnuplot
    -rw-r--r--   1 greg  greg  24076 Jun  3 13:52 latency.png
    -rw-r--r--   1 greg  greg    447 Jun  3 13:52 requests.gnuplot
    drwxr-xr-x   2 greg  greg     11 Jun  3 13:52 ./
    -rw-r--r--   1 greg  greg  28360 Jun  3 13:52 requests.png
```

You can now point your browser at *file:///usr/home/${USER}/getattr/getattr.png*
to view the response curve (or maybe *xv ~/getattr/latency.png*).


## NFS GETATTR

Much like the NFS **READ** operation described above you, can also issue
NFS **GETATTR** operations.  Currently, all threads just hammer away at
the single file/directory you specify on the *nct* command line.  However,
a planned enhancement is that if given a directory *nct* will issue
**GETATTR** requests iteratively/randomly to each file in the directory
until the test duration has elapsed.

In the following example we run the test with only 1 thread for 60 seconds:

    $ ./nct -d60 -j1 -o ~/nct-getattr-1 getattr 10.100.0.1:/export/sparse-8192MB-0

In this case, you're probably more interested in latency and so you would want
to point your browser at file:///usr/home/${USER}/nct-getattr-1/latency.png.



### Chelsio T62100-SO-CR with TOE enabled.

Single threaded, at most one gettar request in flight:

$ sudo ./nct -m1 -d10 -j1 getattr 10.100.0.1:/export/sparse-8192MB-0

```
 SAMPLES  DURATION      OPS    TXMB    RXMB  LATMIN  LATAVG  LATMAX
      10   1005465    57899    6.63    6.18    16.5    17.1    23.3
      20   1000064    60007    6.87    6.41    14.9    16.5    20.9
      30   1000139    63169    7.23    6.75    15.1    15.7    20.7
      40    999682    65819    7.53    7.03    14.3    15.1    20.9
      50    999779    67512    7.73    7.21    14.2    14.7    22.1
      60    995533    67430    7.72    7.20    14.2    14.7    28.8
      70    999960    68252    7.81    7.29    14.2    14.5    27.1
      80   1005524    68319    7.82    7.30    14.1    14.5    19.5
      90    999034    67928    7.77    7.26    14.2    14.6    27.2
     100    999968    68234    7.81    7.29    14.1    14.5    21.3
```

### Chelsio T62100-SO-CR with TOE disabled.

Single threaded, at most one gettar request in flight:

$ sudo ./nct -m1 -d10 -j1 getattr 10.100.1.1:/export/sparse-8192MB-0

```
 SAMPLES  DURATION      OPS    TXMB    RXMB  LATMIN  LATAVG  LATMAX
      10   1006186    49106    5.62    5.25    19.3    20.2    36.2
      20    999441    52356    5.99    5.59    16.9    19.0    26.3
      30   1000060    57035    6.53    6.09    16.5    17.4    24.1
      40    999216    58519    6.70    6.25    16.5    17.0    21.6
      50   1000040    58199    6.66    6.22    16.6    17.1    20.4
      60   1000131    58210    6.66    6.22    16.4    17.1    20.5
      70    999754    58594    6.71    6.26    16.4    17.0    20.5
      80   1000976    58236    6.66    6.22    16.5    17.1    21.9
      90    999991    58178    6.66    6.21    16.6    17.1    21.7
     100   1000001    58267    6.67    6.22    16.5    16.9    19.3
```

### Intel(R) PRO/10GbE X540-AT2

Single threaded, at most one getattr request in flight:

$ sudo ./nct -m1 -d10 -j1 getattr 10.10.0.1:/export/sparse-8192MB-0

```
 SAMPLES  DURATION      OPS    TXMB    RXMB  LATMIN  LATAVG  LATMAX
      10   1004891    30547    3.50    3.26    31.7    32.6    34.5
      20    999984    30499    3.49    3.26    31.6    32.6    34.0
      30   1000602    30506    3.49    3.26    31.4    32.6    47.8
      40   1000390    30506    3.49    3.26    31.6    32.6    34.2
      50    999050    30462    3.49    3.25    30.9    32.7    36.8
      60   1000946    30542    3.50    3.26    29.2    32.6    81.6
      70    999026    30488    3.49    3.26    31.4    32.7    33.9
      80   1000037    30517    3.49    3.26    31.7    32.7    33.7
      90   1000927    30545    3.50    3.26    31.6    32.6    33.7
     100    998989    30339    3.47    3.24    30.9    32.7    34.7
```

## NFS NULL

### Chelsio T62100-SO-CR with TOE enabled.

Single threaded, at most one null request in flight:

$ sudo ./nct -m1 -d10 -j1 null 10.100.0.1:/export/sparse-8192MB-0

```
 SAMPLES  DURATION      OPS    TXMB    RXMB  LATMIN  LATAVG  LATMAX
      10   1006224    72067    3.02    1.65    12.2    13.9    29.6
      20    999115    77712    3.26    1.78    11.2    12.9    17.4
      30    999935    83706    3.51    1.92    11.1    11.9    16.9
      40    999935    85114    3.57    1.95    11.1    11.7    17.0
      50    999788    85289    3.58    1.95    11.1    11.7    17.8
      60    999955    85041    3.57    1.95    11.1    11.7    17.1
      70    995418    84894    3.56    1.94    11.1    11.8    17.2
      80   1004645    85737    3.60    1.96    11.1    11.6    17.1
      90   1000483    85501    3.59    1.96    11.1    11.7    17.5
     100   1000031    84979    3.57    1.95    11.1    11.7    17.2
```

$ ./nct -j1 -d300 -o ~/null null 10.100.0.1:/export/sparse-8192MB-0

         MIN          AVG          MAX           TOTAL  DESC
     3260444      3837736      3913844      1151704444  bytes transmitted per second
     1778424      2093310      2134824       628202424  bytes received per second
        11.3         11.4         14.2    778978297589  latency per request (usecs)
           0        87221        88951        26175101  requests per second
