This is a RIAK based redirector for SQUID. The idea is following:
  Riak stores blocked resource's URL in a specified bucket (ie,'blocked').
  Redirector program checks url against blocked resource by requesting appropriate key from riak.
  If key exists, then program redirects user to 'redrect_url' page
  
  
