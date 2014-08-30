#! /usr/bin/python

import sys

def print_msg(way, text):
  hexa = text.split(' ')
  code = int(hexa[0], 16)
  length = int(hexa[1], 16)
  msg = ''
  for i in range(0, length):
    n = int(hexa[i+2], 16)
    if (n == 10):
      msg += "\\n"
    elif (n == 13):
      msg += "\\r"
    elif ((n >= 32 and n <= 126)):
      msg += chr(n)
    else:
      msg += hex(n)
  print('%s: code=0x%02x, msg="%s"' % (way, code, msg))

def parse(filename):
  with open(filename) as f:
    buff = ['', '', '', '']
    way = ''
    for line in f:
      line = line.strip()

      if 'going down' in line:
        way = 'Sent'
      elif 'coming back' in line:
        way = 'Received'
      elif line.startswith('00000000:'):
        buff[0] = line[10:]
      elif line.startswith('00000010:'):
        buff[1] = line[10:]
      elif line.startswith('00000020:'):
        buff[2] = line[10:]
      elif line.startswith('00000030:'):
        buff[3] = line[10:]
        print_msg(way, ' '.join(buff))

if __name__ == "__main__":
  if len(sys.argv) != 2:
    print("Usage: %s <file>" % sys.argv[0])
  else:
    parse(sys.argv[1])

