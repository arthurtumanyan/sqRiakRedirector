This is a RIAK based redirector for SQUID. The idea is following:
  Riak stores blocked resource's URL in a specified bucket (ie,'blocked').
  Redirector program checks url against blocked resource by requesting appropriate key from riak.
  If key exists, then program redirects user to 'redrect_url' page.
  
COMPILE
  
      gcc -c -g `pkg-config --cflags libcurl` `pkg-config --cflags libconfig` -MMD -MP -MF "redir.o.d" -o redir.o redir.c
      gcc -o sqriakredirector redir.o `pkg-config --libs libcurl` `pkg-config --libs libconfig`   -Wall -pedantic -ansi
  
INSTALL

    url_rewrite_program /path/to/redirector /path/to/sqriak.conf
    url_rewrite_bypass off
    url_rewrite_children 10

TODO

    add pid file support
    add blocking/redirecting support by user
