#!/usr/bin/python

import sys

doc = open(sys.argv[1], 'r')

#sorts arcs for lemon static graph
lineList = []
firstLine = ''
for line in doc.readlines():
    for w in line.split(' '):
        if w == 'c':
            break
        elif w == 'p':
            L = line.split()
            #print L[2],L[3]
            firstLine = "{0} {1}".format( L[2],L[3] )
        elif w == 'a':
            L = line.split()
            lineList.append( (int(L[1]),int(L[2]),int(L[3]) ) )             
            #print L[1],L[2],L[3]

lineList.sort()
print firstLine
for line in lineList:
  print "{0} {1} {2}".format( line[0],line[1],line[2] )
