import os
import json
import subprocess
import zipfile
from collections import OrderedDict
from threading import Thread
from flask import Flask, request, make_response, send_from_directory

SINGA_ROOT = '../'
FOOD_WORKSPACE = SINGA_ROOT + 'foodology/'

class Job(Thread):

  workspace = ''
  
  def __init__(self):
    if not os.path.exists(FOOD_WORKSPACE):
      os.mkdir(FOOD_WORKSPACE)
    Thread.__init__(self)
    
  '''
  create workspace 
    create workspace for job configuration
    input
       path_model: path to job configuration (zip) 
  '''
  def createWorkspace(self, model_zip):
    if self.workspace == '':
      self.workspace = FOOD_WORKSPACE + model_zip.split(".")[0]
      print '--- Create a workspace: {0}'.format(self.workspace)
      if not os.path.isdir(self.workspace):
        zf = zipfile.ZipFile(model_zip, 'r')
        for f_org in zf.namelist():
          f = FOOD_WORKSPACE + f_org 
          if not os.path.basename(f):
            os.mkdir(f)
          else:
            uzf = file(f, 'wb')
            uzf.write(zf.read(f_org))
            uzf.close()
        zf.close()
      else:
        print '-!- {0} exsits'.format(self.workspace)
    else:
      print '-!- {0} exsits'.format(self.workspace)


  def removeWorkspace(self):
    if os.path.isdir(self.workspace):
      cmd = "rm -rf %s" % (self.workspace)
      subprocess.call( cmd.strip().split(" ")  ) 
      print '--- Remove a workspace: {0}'.format(self.workspace)
      self.workspace = ''

  '''
  start job
    run singa and wait
  '''
  def execute(self):
  #def run(self):
    print self.workspace
    data = []
    procs = subprocess.Popen([os.path.join(SINGA_ROOT, 'bin/singa-run.sh'), '-workspace=%s' % self.workspace], stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
    output = iter(procs.stdout.readline, '')
    for line in output:
      if 'job_id' in line:
        jobid = int(line.split('job_id =')[1].split(']')[0])
      elif 'proc' in line:
        host = line.split('-> ')[-1].split(':')[0]
        port = line.split('-> ')[-1].split(':')[-1].split('(')[0]
      elif 'prob' in line:
        data.append({'label':int(line.split('prob')[1].split(':')[0]),
                     'prob':float(line.split('prob')[1].split(':')[1])
                   })
      elif 'done' in line:
        output = {'JobID':jobid, 'Host':host, 'Port':port[:-1], 'Data':data}
        #return json.dumps(output,indent=2)
        self.msg = json.dumps(output,indent=2)
        print self.msg
        data = []
 
'''
test main
'''
ths = []

#job = Job()
#job.createWorkspace("chinesefood.zip")
#job.execute()
#ths.append(job)
#job.removeWorkspace()

job1 = Job()
job1.createWorkspace("chinesefood1.zip")
job1.execute()
#ths.append(job1)
'''
for i in range(len(ths)):
  ths[i].start()

for i in range(len(ths)):
  ths[i].join()
'''
