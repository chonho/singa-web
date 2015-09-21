from fabric.api import run, env, cd, hide
import multiprocessing


env.hosts = ['127.0.0.1']
env.user = 'chonho'
env.password = '1q2w3e4r'

def getHostname():
    run("hostname")

def getPwd():
    with cd('singa-web/incubator-singa'):
        run('pwd')

def doCmd(op):
    with cd('singa-web/incubator-singa'):
        result = ''
        if op=='1':
            with hide('running'):
                pid = run('ps -a | grep foodology | awk \'{print $1}\'')
            pid = int(pid.split("\n")[0])
	    cmd = 'kill -USR1 %d' % pid
	    result = run(cmd)
            #result = run('./test-lee.sh 1')
        if op=='0':
            result = run('./test-lee.sh 0 input45.bin')
        if op=='2':
            #result = run('bin/singa-run.sh -exec foodology/foodology.bin -conf foodology/chinesefood/job.conf -mode 2 -net 2', pty=False)
            result = run('bin/singa-run.sh -exec foodology/foodology.bin -conf foodology/chinesefood/job.conf -mode 2 -net 2', pty=False)
            #result = run('bin/singa-run.sh -conf foodology/chinesefood/job.conf -mode 2 -net 2 > /dev/null 2>&1 &', pty=False)
            #run('./test-fabric.sh >/dev/null &', pty=False)
        #print result 
