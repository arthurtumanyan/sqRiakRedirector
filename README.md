This is a RIAK based redirector for SQUID. The idea is following:
  Riak stores blocked resource's URL in a specified bucket (ie,'blocked').
  Redirector program checks url against blocked resource by requesting appropriate key from riak.
  If key exists, then program redirects user to 'redrect_url' page.
  
  -- Added: it is possible per fqdn redirection, i.e, to redirect from site1 to site2,
            just create a key 'site1' with text value 'site2', otherwise ,
            if key exists without value, client will be redirected to 'redirect_url'

      curl -v -d 'http://site2.org'; -H "Content-Type: text/plain" http://<ip>:8098/buckets/blocked/keys/www.site1.org
 
COMPILE
  
      gcc -c -g `pkg-config --cflags libcurl` `pkg-config --cflags libconfig` -MMD -MP -MF "redir.o.d" -o redir.o redir.c
      gcc -o sqriakredirector redir.o `pkg-config --libs libcurl` `pkg-config --libs libconfig`   -Wall -pedantic -ansi
  
INSTALL

  Add to squid.conf followed lines
  
    url_rewrite_program /path/to/redirector /path/to/sqriak.conf
    url_rewrite_bypass off
    url_rewrite_children 10

TODO

    add pid file support
    add blocking/redirecting support by user
