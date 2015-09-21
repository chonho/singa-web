import sys
import select
import paramiko

ROOT = 'singa-web/incubator-singa'

HOST = 'localhost'
USER = 'chonho'
PSWD = '1q2w3e4r'

ssh = paramiko.Transport((HOST, 22))
ssh.connect(username=USER, password=PSWD)

nbytes = 4096
out_data = []
err_data = []
channel = ssh.open_channel(kind='session')
#cmd = '%s/bin/singa-run.sh -exec %s/foodology/foodology.bin -singa_conf %s/conf/singa.conf -conf %s/foodology/chinesefood/job.conf -mode 2 -net 2' % (ROOT, ROOT, ROOT, ROOT)
channel.exec_command('nohup singa-web/incubator-singa/bin/singa-run.sh -exec foodology/foodology.bin -conf foodology/chinesefood/job.con -mode 2 -net 2 > /dev/null 2>&1 &')
#ses.exec_command('ls -l')

while True:
  if channel.recv_ready():
    out_data.append(channel.recv(nbytes))
  if channel.recv_stderr_ready():
    err_data.append(channel.recv_stderr(nbytes))
  if channel.exit_status_ready():
    break

#-----------
print ''.join(out_data)
print ''.join(err_data)
ssh.close()
