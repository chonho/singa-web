import sys
import select
import paramiko

HOST = 'localhost'
USER = 'chonho'
PSWD = '1q2w3e4r'

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect(HOST, username=USER, password=PSWD)

#stdin, stdout, stderr = ssh.exec_command('cd singa-web/incubator-singa')

#stdin, stdout, stderr = ssh.exec_command('ls singa-web/incubator-singa -l')

stdin, stdout, stderr = ssh.exec_command('nohup python singa-web/incubator-singa/api/singatest.py >/dev/null 2>1& &')

while not stdout.channel.exit_status_ready():
  if stdout.channel.recv_ready():
    rl, wl, xl = select.select([stdout.channel], [], [], 0.0)
    if len(rl) > 0:
      for line in stdout:
        print line.strip('\n')

#-----------
print 'done'
ssh.close()
