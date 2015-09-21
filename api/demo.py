import os
import subprocess

SINGA_ROOT = '../'

cmd = 'ps -a'
ret = subprocess.Popen(cmd.strip().split(' '), stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
cmd = 'grep foodology'
ret = subprocess.Popen(cmd.strip().split(' '), stdin = ret.stdout, stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
pid = ret.communicate()[0].split(" ")[2]


print '--- SINGA Demo (interactive mode) ---'
print '  run' 

qFlag = True

while( qFlag ):

  keyin = raw_input('>> ')
  args = keyin.split(' ')
  print args

  if args[0] in ('run'):

      #cmd = "fab -f fabricsinga.py doCmd:op=1"
      cmd = 'kill -USR1 %s' % pid 
      ret = subprocess.Popen(cmd.strip().split(' '), stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
      for line in iter(ret.stdout.readline, ''):
           print line

  if args[0] in ('test'):

      cmd = 'kill -USR1 %s' % pid 
      subprocess.call( cmd.strip().split(" ")  )

  if args[0] in ('q', 'quit'):
    qFlag = False



