#! /usr/bin/python

def print_buff(way, text):
  hexa = text.split(' ')
  code = int(hexa[0], 16)
  length = int(hexa[1], 16)
  msg = ''
  for i in range(0, length):
    msg += chr(int(hexa[i+2], 16))
  print(way + ': code=' + hex(code) + ', msg="' + msg + '"')
  print('')

with open('insulinx.log') as f:
  buff = ['', '', '', '']
  way = ''
  for line in f:
    line = line.strip()
    if 'going down' in line:
      way = 'Send'
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
      print_buff(way, ' '.join(buff))
